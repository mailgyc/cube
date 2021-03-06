// console.cpp: the console buffer, its display, and command line control

#include "cube.h"
#include <ctype.h>

struct cline {
	char *cref;
	int outtime;
};
std::vector<cline> conlines;

const int ndraw = 5;
const int WORDWRAP = 80;
int conskip = 0;

bool saycommandon = false;
IString commandbuf;

void setconskip(int n) {
	conskip += n;
	if (conskip < 0)
		conskip = 0;
}

COMMANDN(conskip, setconskip, ARG_1INT);

void conline(const char *sf, bool highlight) // add a line to the console buffer
{
	cline cl;
	if (conlines.size() > 100) {
		cl.cref = conlines.back().cref;
		conlines.pop_back();
	} else {
		cl.cref = newIStringbuf(""); // constrain the buffer size
	}
	cl.outtime = lastmillis;              // for how long to keep line on screen
	conlines.insert(conlines.begin(), cl);
	if (highlight)             // show line in a different colour, for chat etc.
	{
		cl.cref[0] = '\f';
		cl.cref[1] = 0;
		strcat_s(cl.cref, sf);
	} else {
		strcpy_s(cl.cref, sf);
	};
	puts(cl.cref);
	fflush(stdout);
}

void conoutf(const char *s, ...) {

	IString sf = { 0 };
	va_list ap;
	va_start(ap, s);
//	std::snprintf(sf, _MAXDEFSTR, s, ap);
	vsnprintf(sf, _MAXDEFSTR, s, ap);
	sf[_MAXDEFSTR - 1] = 0;
	va_end(ap);

	s = sf;
	int n = 0;
	while (strlen(s) > WORDWRAP)                 // cut IStrings to fit on screen
	{
		IString t;
		strn0cpy(t, s, WORDWRAP + 1);
		conline(t, n++ != 0);
		s += WORDWRAP;
	};
	conline(s, n != 0);
}

void renderconsole()       // render buffer taking into account time & scrolling
{
	int nd = 0;
	char *refs[ndraw];
	for (int i = 0; i < conlines.size(); ++i) {
		if (conskip ?
				i >= conskip - 1 || i >= conlines.size() - ndraw :
				lastmillis - conlines[i].outtime < 20000) {
			refs[nd++] = conlines[i].cref;
			if (nd == ndraw)
				break;
		};
	}
	for(int i = 0; i < nd; ++i)
	{
		draw_text(refs[i], FONTH / 3, (FONTH / 4 * 5) * (nd - i - 1) + FONTH / 3, 2);
	};
}

// keymap is defined externally in keymap.cfg

struct keym {
	int code;
	std::string name;
	std::string action;
} keyms[256];
int numkm = 0;

void keymap(char *code, char *key, char *action) {
	keyms[numkm].code = std::atoi(code);
	keyms[numkm].name = key;
	keyms[numkm++].action = action;
}

COMMAND(keymap, ARG_3STR);

void bindkey(char *key, char *action) {
	for (char *x = key; *x; x++) {
		*x = toupper(*x);
	}
	std::string _key(key);
	for(int i = 0; i < numkm; ++i)
		if (keyms[i].name == _key) {
			keyms[i].action = action;
			return;
		};
	conoutf("unknown key \"%s\"", key);
}

COMMANDN(bind, bindkey, ARG_2STR);

void saycommand(char *init)         // turns input to the command line on or off
		{
	if (!init)
		init = "";
	strcpy_s(commandbuf, init);
}

void mapmsg(char *s) {
	strn0cpy(hdr.maptitle, s, 128);
}

COMMAND(saycommand, ARG_VARI);
COMMAND(mapmsg, ARG_1STR);

#ifndef WIN32
#include <X11/Xlib.h>
#include <SDL2/SDL_syswm.h>
#endif

void pasteconsole() {
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	wminfo.subsystem = SDL_SYSWM_X11;
	//SDL_GetWindowWMInfo( &wminfo);

	int cbsize;
	char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
	if (!cb || !cbsize)
		return;
	int commandlen = strlen(commandbuf);
	for (char *cbline = cb, *cbend;
			commandlen + 1 < _MAXDEFSTR && cbline < &cb[cbsize];
			cbline = cbend + 1) {
		cbend = (char *) memchr(cbline, '\0', &cb[cbsize] - cbline);
		if (!cbend)
			cbend = &cb[cbsize];
		if (commandlen + cbend - cbline + 1 > _MAXDEFSTR)
			cbend = cbline + _MAXDEFSTR - commandlen - 1;
		memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
		commandlen += cbend - cbline;
		commandbuf[commandlen] = '\n';
		if (commandlen + 1 < _MAXDEFSTR && cbend < &cb[cbsize])
			++commandlen;
		commandbuf[commandlen] = '\0';
	};
	XFree(cb);
}

std::vector<char *> vhistory;
int histpos = 0;

void history(int n) {
	static bool rec = false;
	if (!rec && n >= 0 && n < vhistory.size()) {
		rec = true;
		execute(vhistory[vhistory.size() - n - 1]);
		rec = false;
	};
}

COMMAND(history, ARG_1INT);

void keypress(int code, bool isdown) {
	if (saycommandon)                            // keystrokes go to commandline
	{
		if (isdown) {
			switch (code) {
			case SDLK_RETURN:
				break;

			case SDLK_BACKSPACE:
			case SDLK_LEFT: {
				for (int i = 0; commandbuf[i]; i++)
					if (!commandbuf[i + 1])
						commandbuf[i] = 0;
				resetcomplete();
				break;
			}
			case SDLK_UP:
				if (histpos)
					strcpy_s(commandbuf, vhistory[--histpos]);
				break;
			case SDLK_DOWN:
				if (histpos < vhistory.size())
					strcpy_s(commandbuf, vhistory[histpos++]);
				break;
			case SDLK_TAB:
				complete(commandbuf);
				break;
			case SDLK_v:
				if (SDL_GetModState() & (KMOD_LCTRL | KMOD_RCTRL)) {
					pasteconsole();
					return;
				}
				break;
			default:
				resetcomplete();
				//if(cooked) { char add[] = { cooked, 0 }; strcat_s(commandbuf, add); };
			};
		} else {
			if (code == SDLK_RETURN) {
				if (commandbuf[0]) {
					if (vhistory.empty() || strcmp(vhistory.back(), commandbuf)) {
						vhistory.emplace_back(newIString(commandbuf)); // cap this?
					};
					histpos = vhistory.size();
					if (commandbuf[0] == '/')
						execute(commandbuf, true);
					else
						toserver(commandbuf);
				};
				saycommand(NULL);
			} else if (code == SDLK_ESCAPE) {
				saycommand(NULL);
			};
		};
	} else if (!menukey(code, isdown)) {               // keystrokes go to menu
		for (int i = 0; i < numkm; ++i) {
			if (keyms[i].code == code) // keystrokes go to game, lookup in keymap and execute
			{
				execute(keyms[i].action.c_str(), isdown); // @suppress("Invalid arguments")
				return;
			}
		}
	}
}

char *getcurcommand() {
	return saycommandon ? commandbuf : NULL;
}

void writebinds(FILE *f) {
	for(int i = 0; i < numkm; ++i)
	{
		if (!keyms[i].action.empty())
			fprintf(f, "bind \"%s\" [%s]\n", keyms[i].name.c_str(), keyms[i].action.c_str());
	}
}

