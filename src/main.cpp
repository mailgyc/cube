#include <iostream>
#include "cube.h"

void cleanup(const std::string &msg)         // single program exit point;
{
	stop();
	disconnect(true);
	writecfg();
	cleangl();
	cleansound();
	cleanupserver();
	SDL_ShowCursor(1);
	if (!msg.empty()) {
		std::cout << msg << std::endl;
	};
	SDL_Quit();
	exit(1);
}

void quit()                     // normal exit
{
	writeservercfg();
	cleanup(NULL);
}

void fatal(char *s, char *o)    // failure exit
{
	std::string ss(s);
	std::string oo(o);
	cleanup(ss + oo + ":" + SDL_GetError());
}

void *alloc(int s) // for some big chunks... most other allocs use the memory pool
{
	void *b = calloc(1, s);
	if (!b)
		fatal("out of memory!");
	return b;
}

int scr_w = 640;
int scr_h = 480;

void screenshot() {
	SDL_Surface *image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
	if (image) {
		SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
		if (temp) {
			glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
			for (int idx = 0; idx < scr_h; idx++) {
				char *dest = (char *) temp->pixels + 3 * scr_w * idx;
				memcpy(dest, (char *) image->pixels + 3 * scr_w * (scr_h - 1 - idx), 3 * scr_w);
				endianswap(dest, 3, scr_w);
			};
			char buf[80];
			std::sprintf(buf, "screenshots/screenshot_%d.bmp", lastmillis);
			SDL_SaveBMP(temp, path(buf));
			SDL_FreeSurface(temp);
		};
		SDL_FreeSurface(image);
	};
}

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);
VARP(minmillis, 0, 5, 1000);

int islittleendian = 1;
int framesinmap = 0;

int main(int argc, char **argv) {
	bool dedicated = false;
	int par = 0, uprate = 0, maxcl = 4;
	char *sdesc = "", *ip = "", *master = NULL, *passwd = "";
	islittleendian = *((char *) &islittleendian);

	printf("init");

	for (int i = 1; i < argc; i++) {
		char *a = &argv[i][2];
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'd':
				dedicated = true;
				break;
			case 'w':
				scr_w = atoi(a);
				break;
			case 'h':
				scr_h = atoi(a);
				break;
			case 'u':
				uprate = atoi(a);
				break;
			case 'n':
				sdesc = a;
				break;
			case 'i':
				ip = a;
				break;
			case 'm':
				master = a;
				break;
			case 'p':
				passwd = a;
				break;
			case 'c':
				maxcl = atoi(a);
				break;
			default:
				printf("unknown commandline option");
			}
		} else {
			printf("unknown commandline argument");
		}
	};
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | par) < 0)
		fatal("Unable to initialize SDL");

	printf("net");
	if (enet_initialize() < 0)
		fatal("Unable to initialise network module");

	initclient();
	initserver(dedicated, uprate, sdesc, ip, master, passwd, maxcl); // never returns if dedicated

	printf("world");
	empty_world(7, true);

	printf("video: sdl");
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		fatal("Unable to initialize SDL Video");

	printf("video: mode");
	SDL_Window *window = SDL_CreateWindow("cube engine", 0, 0, scr_w, scr_h, SDL_WINDOW_OPENGL);
	SDL_GL_CreateContext(window);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	printf("video: misc");
	//SDL_SetRelativeMouseMode(SDL_TRUE);

	printf("gl");
	gl_init(scr_w, scr_h);

	printf("basetex");
	int xs, ys;
	if (!installtex(2, path(newIString("data/newchars.png")), xs, ys)
			|| !installtex(3, path(newIString("data/martin/base.png")), xs, ys)
			|| !installtex(6, path(newIString("data/martin/ball1.png")), xs, ys)
			|| !installtex(7, path(newIString("data/martin/smoke.png")), xs, ys)
			|| !installtex(8, path(newIString("data/martin/ball2.png")), xs, ys)
			|| !installtex(9, path(newIString("data/martin/ball3.png")), xs, ys)
			|| !installtex(4, path(newIString("data/explosion.jpg")), xs, ys)
			|| !installtex(5, path(newIString("data/items.png")), xs, ys)
			|| !installtex(1, path(newIString("data/crosshair.png")), xs, ys))
		fatal(
				"could not find core textures (hint: run cube from the parent of the bin directory)");

	printf("sound");
	initsound();

	printf("cfg");
	newmenu("frags\tpj\tping\tteam\tname");
	newmenu("ping\tplr\tserver");
	exec("data/keymap.cfg");
	exec("data/menus.cfg");
	exec("data/prefabs.cfg");
	exec("data/sounds.cfg");
	exec("servers.cfg");
	if (!execfile("config.cfg"))
		execfile("data/defaults.cfg");
	exec("autoexec.cfg");

	printf("localconnect");
	localconnect();
	changemap("metl3");	// if this map is changed, also change depthcorrect()

	printf("mainloop");
	int ignore = 5;
	for (;;) {
		int millis = SDL_GetTicks() * gamespeed / 100;
		if (millis - lastmillis > 200)
			lastmillis = millis - 200;
		else if (millis - lastmillis < 1)
			lastmillis = millis - 1;
		if (millis - lastmillis < minmillis)
			SDL_Delay(minmillis - (millis - lastmillis));
		cleardlights();
		updateworld(millis);
		if (!demoplayback)
			serverslice((int) time(NULL), 0);
		static float fps = 30.0f;
		fps = (1000.0f / curtime + fps * 50) / 51;
		computeraytable(player1->o.x, player1->o.y);
		readdepth(scr_w, scr_h);
		SDL_GL_SwapWindow(window);
		extern void updatevol();
		updatevol();
		if (framesinmap++ < 5)// cheap hack to get rid of initial sparklies, even when triple buffering etc.
		{
			player1->yaw += 5;
			gl_drawframe(scr_w, scr_h, fps);
			player1->yaw -= 5;
		}
		gl_drawframe(scr_w, scr_h, fps);
		SDL_Event event;
		unsigned int lasttype = 0, lastbut = 0;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				quit();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keypress(event.key.keysym.sym, event.key.state == SDL_PRESSED);
				break;
			case SDL_MOUSEMOTION:
				if (ignore) {
					ignore--;
					break;
				}
				mousemove(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (lasttype == event.type && lastbut == event.button.button)
					break; // why?? get event twice without it
				keypress(-event.button.button, event.button.state != 0);
				lasttype = event.type;
				lastbut = event.button.button;
				break;
			};
		};
	};
	quit();
	return 1;
}

