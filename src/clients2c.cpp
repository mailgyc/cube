// client processing of the incoming network stream

#include "cube.h"

extern int clientnum;
extern bool c2sinit, senditemstoserver;
extern std::string toservermap;
extern IString clientpassword;

void neterr(char *s) {
	conoutf("illegal network message (%s)", s);
	disconnect();
}

void changemapserv(const std::string &name, int mode)    // forced map change from the server
{
	gamemode = mode;
	load_world(name);
}

void changemap(const std::string &mapname)              // request map change, server may ignore
{
	toservermap = mapname;
}

// update the position of other clients in the game in our world
// don't care if he's in the scenery or other players,
// just don't overlap with our client

void updatepos(Sprite *d) {
	const float r = player1->radius + d->radius;
	const float dx = player1->o.x - d->o.x;
	const float dy = player1->o.y - d->o.y;
	const float dz = player1->o.z - d->o.z;
	const float rz = player1->aboveeye + d->eyeheight;
	const float fx = (float) fabs(dx), fy = (float) fabs(dy), fz = (float) fabs(
			dz);
	if (fx < r && fy < r && fz < rz && d->state != CS_DEAD) {
		if (fx < fy)
			d->o.y += dy < 0 ? r - fy : -(r - fy);  // push aside
		else
			d->o.x += dx < 0 ? r - fx : -(r - fx);
	};
	int lagtime = lastmillis - d->lastupdate;
	if (lagtime) {
		d->plag = (d->plag * 5 + lagtime) / 6;
		d->lastupdate = lastmillis;
	};
}

void localservertoclient(uchar *buf, int len) // processes any updates from the server
{
	if (ENET_NET_TO_HOST_16(*(ushort *)buf) != len)
		neterr("packet length");
	incomingdemodata(buf, len);

	uchar *end = buf + len;
	uchar *p = buf + 2;
	char text[MAXTRANS];
	int cn = -1, type;
	Sprite *spr = NULL;
	bool mapchanged = false;

	while (p < end)
		switch (type = getint(p)) {
		case SV_INITS2C:                    // welcome messsage from the server
		{
			cn = getint(p);
			int prot = getint(p);
			if (prot != PROTOCOL_VERSION) {
				conoutf( "you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
				disconnect();
				return;
			};
			toservermap = "";
			clientnum = cn;                 // we are now fully connected
			if (!getint(p)) {
				toservermap = getclientmap(); // we are the first client on this server, set map
			}
			sgetstr();

			if (text[0] && strcmp(text, clientpassword)) {
				conoutf( "you need to set the correct password to join this server!");
				disconnect();
				return;
			};
			if (getint(p) == 1) {
				conoutf("server is FULL, disconnecting..");
			};
			break;
		}
		case SV_POS:                        // position of another client
		{
			cn = getint(p);
			spr = getclient(cn);
			if (!spr)
				return;
			spr->o.x = getint(p) / DMF;
			spr->o.y = getint(p) / DMF;
			spr->o.z = getint(p) / DMF;
			spr->yaw = getint(p) / DAF;
			spr->pitch = getint(p) / DAF;
			spr->roll = getint(p) / DAF;
			spr->vel.x = getint(p) / DVF;
			spr->vel.y = getint(p) / DVF;
			spr->vel.z = getint(p) / DVF;
			int f = getint(p);
			spr->strafe = (f & 3) == 3 ? -1 : f & 3;
			f >>= 2;
			spr->move = (f & 3) == 3 ? -1 : f & 3;
			spr->onfloor = (f >> 2) & 1;
			int state = f >> 3;
			if (state == CS_DEAD && spr->state != CS_DEAD)
				spr->lastaction = lastmillis;
			spr->state = state;
			if (!demoplayback)
				updatepos(spr);
			break;
		}
		case SV_SOUND:
			playsound(getint(p), &spr->o);
			break;
		case SV_TEXT:
			sgetstr();
			conoutf("%s:\f %s", spr->name, text);
			break;
		case SV_MAPCHANGE:
			sgetstr();
			changemapserv(text, getint(p));
			mapchanged = true;
			break;
		case SV_ITEMLIST: {
			int n;
			if (mapchanged) {
				senditemstoserver = false;
				resetspawns();
			};
			while ((n = getint(p)) != -1) {
				if (mapchanged)
					setspawn(n, true);
			}
			break;
		}

		case SV_MAPRELOAD:          // server requests next map
		{
			getint(p);
			std::string nextmapalias = std::string("nextmap_") + getclientmap();
			std::string map = getalias(nextmapalias);     // look up map in the cycle
			changemap(map.empty() ? getclientmap() : map);
			break;
		}

		case SV_INITC2S: // another client either connected or changed name/team
		{
			sgetstr();

			if (spr->name[0]) {         // already connected
				if (strcmp(spr->name, text))
					conoutf("%s is now known as %s", spr->name, text);
			} else {                   // new client
				c2sinit = false;    // send new players my info again 
				conoutf("connected: %s", text);
			};
			strcpy_s(spr->name, text);
			sgetstr();

			strcpy_s(spr->team, text);
			spr->lifesequence = getint(p);
			break;
		}

		case SV_CDIS:
			cn = getint(p);
			if (!(spr = getclient(cn)))
				break;
			conoutf("player %s disconnected", spr->name[0] ? spr->name : "[incompatible client]");
			zapSprite(players[cn]);
			break;

		case SV_SHOT: {
			int gun = getint(p);
			Vec3 s, e;
			s.x = getint(p) / DMF;
			s.y = getint(p) / DMF;
			s.z = getint(p) / DMF;
			e.x = getint(p) / DMF;
			e.y = getint(p) / DMF;
			e.z = getint(p) / DMF;
			if (gun == GUN_SG)
				createrays(s, e);
			shootv(gun, s, e, spr);
			break;
		}

		case SV_DAMAGE: {
			int target = getint(p);
			int damage = getint(p);
			int ls = getint(p);
			if (target == clientnum) {
				if (ls == player1->lifesequence)
					selfdamage(damage, cn, spr);
			} else
				playsound(S_PAIN1 + rnd(5), &getclient(target)->o);
			break;
		}

		case SV_DIED: {
			int actor = getint(p);
			if (actor == cn) {
				conoutf("%s suicided", spr->name);
			} else if (actor == clientnum) {
				int frags;
				if (isteam(player1->team, spr->team)) {
					frags = -1;
					conoutf("you fragged a teammate (%s)", spr->name);
				} else {
					frags = 1;
					conoutf("you fragged %s", spr->name);
				};
				addmsg(1, 2, SV_FRAGS, player1->frags += frags);
			} else {
				Sprite *a = getclient(actor);
				if (a) {
					if (isteam(a->team, spr->name)) {
						conoutf("%s fragged his teammate (%s)", a->name, spr->name);
					} else {
						conoutf("%s fragged %s", a->name, spr->name);
					};
				};
			};
			playsound(S_DIE1 + rnd(2), &spr->o);
			spr->lifesequence++;
			break;
		}
		case SV_FRAGS:
			players[cn]->frags = getint(p);
			break;
		case SV_ITEMPICKUP:
			setspawn(getint(p), false);
			getint(p);
			break;
		case SV_ITEMSPAWN: {
			int i = getint(p);
			setspawn(i, true);
			if (i >= entityList.size())
				break;
			Vec3 v = { entityList[i].x, entityList[i].y, entityList[i].z };
			playsound(S_ITEMSPAWN, &v);
			break;
		}
		case SV_ITEMACC:       // server acknowledges that I picked up this item
			realpickup(getint(p), player1);
			break;
		case SV_EDITH: // coop editing messages, should be extended to include all possible editing ops
		case SV_EDITT:
		case SV_EDITS:
		case SV_EDITD:
		case SV_EDITE: {
			int x = getint(p);
			int y = getint(p);
			int xs = getint(p);
			int ys = getint(p);
			int v = getint(p);
			Rect b = { x, y, xs, ys };
			switch (type) {
			case SV_EDITH:
				editheightxy(v != 0, getint(p), b);
				break;
			case SV_EDITT:
				edittexxy(v, getint(p), b);
				break;
			case SV_EDITS:
				edittypexy(v, b);
				break;
			case SV_EDITD:
				setvdeltaxy(v, b);
				break;
			case SV_EDITE:
				editequalisexy(v != 0, b);
				break;
			};
			break;
		}

		case SV_EDITENT:            // coop edit of ent
		{
			int i = getint(p);
			while (entityList.size() <= i) {
				entityList.emplace_back(Entity());
				entityList.back().type = NOTUSED;
			}
			int to = entityList[i].type;
			entityList[i].type = getint(p);
			entityList[i].x = getint(p);
			entityList[i].y = getint(p);
			entityList[i].z = getint(p);
			entityList[i].attr1 = getint(p);
			entityList[i].attr2 = getint(p);
			entityList[i].attr3 = getint(p);
			entityList[i].attr4 = getint(p);
			entityList[i].spawned = false;
			if (entityList[i].type == LIGHT || to == LIGHT)
				calclight();
			break;
		}
		case SV_PING:
			getint(p);
			break;
		case SV_PONG:
			addmsg(0, 2, SV_CLIENTPING, player1->ping = (player1->ping * 5 + lastmillis - getint(p)) / 6);
			break;
		case SV_CLIENTPING:
			players[cn]->ping = getint(p);
			break;
		case SV_GAMEMODE:
			nextmode = getint(p);
			break;
		case SV_TIMEUP:
			timeupdate(getint(p));
			break;
		case SV_RECVMAP: {
			sgetstr();

			conoutf("received map \"%s\" from server, reloading..", text);
			int mapsize = getint(p);
			writemap(text, mapsize, p);
			p += mapsize;
			changemapserv(text, gamemode);
			break;
		}
		case SV_SERVMSG:
			sgetstr();
			conoutf("%s", text);
			break;
		case SV_EXT: // so we can messages without breaking previous clients/servers, if necessary
		{
			for (int n = getint(p); n; n--)
				getint(p);
			break;
		}
		default:
			neterr("type");
			return;
		}
}

