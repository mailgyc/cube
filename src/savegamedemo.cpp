// loading and saving of savegames & demos, dumps the spawn state of all mapents, the full state of all Sprites (monsters + player)

#include "cube.h"

extern int islittleendian;

gzFile f = NULL;
bool demorecording = false;
bool demoplayback = false;
bool demoloading = false;
std::vector<Sprite *> playerhistory;
int democlientnum = 0;

void startdemo();

void gzput(int i) {
	gzputc(f, i);
}

void gzputi(int i) {
	gzwrite(f, &i, sizeof(int));
}

void gzputv(Vec3 &v) {
	gzwrite(f, &v, sizeof(Vec3));
}

void gzcheck(int a, int b) {
	if (a != b)
		fatal("savegame file corrupt (short)");
}

int gzget() {
	char c = gzgetc(f);
	return c;
}

int gzgeti() {
	int i;
	gzcheck(gzread(f, &i, sizeof(int)), sizeof(int));
	return i;
}

void gzgetv(Vec3 &v) {
	gzcheck(gzread(f, &v, sizeof(Vec3)), sizeof(Vec3));
}

void stop() {
	if (f) {
		if (demorecording)
			gzputi(-1);
		gzclose(f);
	};
	f = NULL;
	demorecording = false;
	demoplayback = false;
	demoloading = false;
	loopv(playerhistory)
		zapSprite(playerhistory[i]);
	playerhistory.resize(0);
}

void stopifrecording() {
	if (demorecording)
		stop();
}

void savestate(char *fn) {
	stop();
	f = gzopen(fn, "wb9");
	if (!f) {
		conoutf("could not write %s", fn);
		return;
	};
	gzwrite(f, (void *) "CUBESAVE", 8);
	gzputc(f, islittleendian);
	gzputi(SAVEGAMEVERSION);
	gzputi(sizeof(Sprite));
	char buf[_MAXDEFSTR];
	sprintf(buf, "%s", getclientmap().c_str());
	gzwrite(f, buf, _MAXDEFSTR);
	gzputi(gamemode);
	gzputi(entityList.size());
	for(Entity &en : entityList) {
		gzputc(f, en.spawned);
	}
	gzwrite(f, player1, sizeof(Sprite));
	std::vector<Sprite *> &monsters = getmonsters();
	gzputi(monsters.size());
	for(Sprite *m : monsters) {
		gzwrite(f, m, sizeof(Sprite));
	}
	gzputi(players.size());
	for(Sprite *p : players) {
		gzput(p == NULL);
		gzwrite(f, p, sizeof(Sprite));
	};
}

void savegame(char *name) {
	if (!m_classicsp) {
		conoutf("can only save classic sp games");
		return;
	};
	IString fn;
	std::sprintf(fn, "savegames/%s.csgz", name);
	savestate(fn);
	stop();
	conoutf("wrote %s", fn);
}

void loadstate(char *fn) {
	stop();
	if (multiplayer())
		return;
	f = gzopen(fn, "rb9");
	if (!f) {
		conoutf("could not open %s", fn);
		return;
	};

	char buf[_MAXDEFSTR];
	gzread(f, buf, 8);
	// not supporting save->load accross incompatible architectures simpifies things a LOT
	if (strncmp(buf, "CUBESAVE", 8) || gzgetc(f) != islittleendian || gzgeti() != SAVEGAMEVERSION || gzgeti() != sizeof(Sprite)) {
		conoutf("aborting: savegame/demo from a different version of cube or cpu architecture");
		stop();
	} else {
		memset(buf, 0, _MAXDEFSTR);
		gzread(f, buf, _MAXDEFSTR);
		nextmode = gzgeti();
		changemap(std::string(buf)); // continue below once map has been loaded and client & server have updated
	}
}

void loadgame(char *name) {
	char fn[_MAXDEFSTR];
	std::sprintf(fn, "savegames/%s.csgz", name);
	loadstate(fn);
}

void loadgameout() {
	stop();
	conoutf("loadgame incomplete: savegame from a different version of this map");
}

void loadgamerest() {
	if (demoplayback || !f)
		return;

	if (gzgeti() != entityList.size())
		return loadgameout();
	for(Entity &en : entityList) {
		en.spawned = gzgetc(f) != 0;
		if (en.type == CARROT && !en.spawned)
			trigger(en.attr1, en.attr2, true);
	};
	restoreserverstate(entityList);

	gzread(f, player1, sizeof(Sprite));
	player1->lastaction = lastmillis;

	int nmonsters = gzgeti();
	std::vector<Sprite *> &monsters = getmonsters();
	if (nmonsters != monsters.size())
		return loadgameout();
	loopv(monsters) {
		gzread(f, monsters[i], sizeof(Sprite));
		monsters[i]->enemy = player1;    // lazy, could save id of enemy instead
		monsters[i]->lastaction = monsters[i]->trigger = lastmillis + 500; // also lazy, but no real noticable effect on game
		if (monsters[i]->state == CS_DEAD)
			monsters[i]->lastaction = 0;
	};
	restoremonsterstate();

	int nplayers = gzgeti();
	loopi(nplayers)
		if (!gzget()) {
			Sprite *d = getclient(i);
			assert(d);
			gzread(f, d, sizeof(Sprite));
		};

	conoutf("savegame restored");
	if (demoloading)
		startdemo();
	else
		stop();
}

// demo functions

int starttime = 0;
int playbacktime = 0;
int ddamage, bdamage;
Vec3 dorig;

void record(char *name) {
	if (m_sp) {
		conoutf("cannot record singleplayer games");
		return;
	};
	int cn = getclientnum();
	if (cn < 0)
		return;
	IString fn;
	std::sprintf(fn, "demos/%s.cdgz", name);
	savestate(fn);
	gzputi(cn);
	conoutf("started recording demo to %s", fn);
	demorecording = true;
	starttime = lastmillis;
	ddamage = bdamage = 0;
}

void demodamage(int damage, Vec3 &o) {
	ddamage = damage;
	dorig = o;
}

void demoblend(int damage) {
	bdamage = damage;
}

void incomingdemodata(uchar *buf, int len, bool extras) {
	if (!demorecording)
		return;
	gzputi(lastmillis - starttime);
	gzputi(len);
	gzwrite(f, buf, len);
	gzput(extras);
	if (extras) {
		gzput(player1->gunselect);
		gzput(player1->lastattackgun);
		gzputi(player1->lastaction - starttime);
		gzputi(player1->gunwait);
		gzputi(player1->health);
		gzputi(player1->armour);
		gzput(player1->armourtype);
		loopi(NUMGUNS)
			gzput(player1->ammo[i]);
		gzput(player1->state);
		gzputi(bdamage);
		bdamage = 0;
		gzputi(ddamage);
		if (ddamage) {
			gzputv(dorig);
			ddamage = 0;
		};
		// FIXME: add all other client state which is not send through the network
	};
}

void demo(char *name) {
	IString fn;
	std::sprintf(fn, "demos/%s.cdgz", name);
	loadstate(fn);
	demoloading = true;
}

void stopreset() {
	conoutf("demo stopped (%d msec elapsed)", lastmillis - starttime);
	stop();
	for(auto p : players) {
		zapSprite(p);
	}
	disconnect(0, 0);
}

int demoplaybackspeed = variable("demoplaybackspeed", 10, 100, 1000, &demoplaybackspeed, NULL, false);
int scaletime(int t) {
	return (int) (t * (100.0f / demoplaybackspeed)) + starttime;
}

void readdemotime() {
	if (gzeof(f) || (playbacktime = gzgeti()) == -1) {
		stopreset();
		return;
	};
	playbacktime = scaletime(playbacktime);
}

void startdemo() {
	democlientnum = gzgeti();
	demoplayback = true;
	starttime = lastmillis;
	conoutf("now playing demo");
	Sprite *d = getclient(democlientnum);
	assert(d);
	*d = *player1;
	readdemotime();
}

int demodelaymsec = variable("demodelaymsec", 0, 120, 500, &demodelaymsec, NULL, false);

void catmulrom(Vec3 &z, Vec3 &a, Vec3 &b, Vec3 &c, float s, Vec3 &dest) // spline interpolation
{
	Vec3 t1 = b, t2 = c;

	vsub(t1, z);
	vmul(t1, 0.5f)
	vsub(t2, a);
	vmul(t2, 0.5f);

	float s2 = s * s;
	float s3 = s * s2;

	dest = a;
	Vec3 t = b;

	vmul(dest, 2 * s3 - 3 * s2 + 1);
	vmul(t, -2 * s3 + 3 * s2);
	vadd(dest, t);
	vmul(t1, s3 - 2 * s2 + s);
	vadd(dest, t1);
	vmul(t2, s3 - s2);
	vadd(dest, t2);
}

void fixwrap(Sprite *a, Sprite *b) {
	while (b->yaw - a->yaw > 180)
		a->yaw += 360;
	while (b->yaw - a->yaw < -180)
		a->yaw -= 360;
}

void demoplaybackstep() {
	while (demoplayback && lastmillis >= playbacktime) {
		int len = gzgeti();
		if (len < 1 || len > MAXTRANS) {
			conoutf("error: huge packet during demo play (%d)", len);
			stopreset();
			return;
		};
		uchar buf[MAXTRANS];
		gzread(f, buf, len);
		localservertoclient(buf, len);  // update game state

		Sprite *target = players[democlientnum];
		assert(target);

		int extras;
		if (extras = gzget()) // read additional client side state not present in normal network stream
				{
			target->gunselect = gzget();
			target->lastattackgun = gzget();
			target->lastaction = scaletime(gzgeti());
			target->gunwait = gzgeti();
			target->health = gzgeti();
			target->armour = gzgeti();
			target->armourtype = gzget();
			loopi(NUMGUNS)
				target->ammo[i] = gzget();
			target->state = gzget();
			target->lastmove = playbacktime;
			if (bdamage = gzgeti())
				damageblend(bdamage);
			if (ddamage = gzgeti()) {
				gzgetv(dorig);
				particle_splash(3, ddamage, 1000, dorig);
			};
			// FIXME: set more client state here
		};

		// insert latest copy of player into history
		if (extras
				&& (playerhistory.empty()
						|| playerhistory.back()->lastupdate != playbacktime)) {
			Sprite *d = newSprite();
			*d = *target;
			d->lastupdate = playbacktime;
			playerhistory.emplace_back(d);
			if (playerhistory.size() > 20) {
				zapSprite(playerhistory[0]);
				playerhistory.erase(playerhistory.begin());
			};
		};

		readdemotime();
	};

	if (demoplayback) {
		int itime = lastmillis - demodelaymsec;
		for (int i = playerhistory.size() - 1; i >= 0; --i)
			if (playerhistory[i]->lastupdate < itime) // find 2 positions in history that surround interpolation time point
					{
				Sprite *a = playerhistory[i];
				Sprite *b = a;
				if (i + 1 < playerhistory.size())
					b = playerhistory[i + 1];
				*player1 = *b;
				if (a != b)                          // interpolate pos & angles
						{
					Sprite *c = b;
					if (i + 2 < playerhistory.size())
						c = playerhistory[i + 2];
					Sprite *z = a;
					if (i - 1 >= 0)
						z = playerhistory[i - 1];
					//if(a==z || b==c) printf("* %d\n", lastmillis);
					float bf = (itime - a->lastupdate)
							/ (float) (b->lastupdate - a->lastupdate);
					fixwrap(a, player1);
					fixwrap(c, player1);
					fixwrap(z, player1);
					vdist(dist, v, z->o, c->o);
					if (dist < 16)	// if teleport or spawn, dont't interpolate
							{
						catmulrom(z->o, a->o, b->o, c->o, bf, player1->o);
						catmulrom(*(Vec3 *) &z->yaw, *(Vec3 *) &a->yaw,
								*(Vec3 *) &b->yaw, *(Vec3 *) &c->yaw, bf,
								*(Vec3 *) &player1->yaw);
					};
					fixplayer1range();
				};
				break;
			};
		//if(player1->state!=CS_DEAD) showscores(false);
	};
}

void stopn() {
	if (demoplayback)
		stopreset();
	else
		stop();
	conoutf("demo stopped");
}

COMMAND(record, ARG_1STR);
COMMAND(demo, ARG_1STR);
COMMANDN(stop, stopn, ARG_NONE);

COMMAND(savegame, ARG_1STR);
COMMAND(loadgame, ARG_1STR);
