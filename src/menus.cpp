// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

struct mitem {
	char *text, *action;
};

struct gmenu {
	char *name;
	std::vector<mitem> items;
	int mwidth;
	int menusel;
};

std::vector<gmenu> menus;

int vmenu = -1;

std::vector<int> menustack;

void menuset(int menu) {
	if ((vmenu = menu) >= 1)
		resetmovement(player1);
	if (vmenu == 1)
		menus[1].menusel = 0;
}

void showmenu(char *name) {
	loopv(menus)
		if (i > 1 && strcmp(menus[i].name, name) == 0) {
			menuset(i);
			return;
		};
}

int menucompare(mitem *a, mitem *b) {
	int x = atoi(a->text);
	int y = atoi(b->text);
	if (x > y)
		return -1;
	if (x < y)
		return 1;
	return 0;
}

void sortmenu(int start, int num) {
	qsort(&menus[0].items[start], num, sizeof(mitem),
			(int (__cdecl *)(const void *, const void *))menucompare);};

void refreshservers();

bool rendermenu() {
	if (vmenu < 0) {
		menustack.resize(0);
		return false;
	};
	if (vmenu == 1)
		refreshservers();
	gmenu &m = menus[vmenu];
	IString title;
	std::sprintf(title, vmenu > 1 ? "[ %s menu ]" : "%s", m.name);
	int mdisp = m.items.size();
	int w = 0;
	loopi(mdisp)
	{
		int x = text_width(m.items[i].text);
		if (x > w)
			w = x;
	};
	int tw = text_width(title);
	if (tw > w)
		w = tw;
	int step = FONTH / 4 * 5;
	int h = (mdisp + 2) * step;
	int y = (VIRTH - h) / 2;
	int x = (VIRTW - w) / 2;
	blendbox(x - FONTH / 2 * 3, y - FONTH, x + w + FONTH / 2 * 3, y + h + FONTH,
			true);
	draw_text(title, x, y, 2);
	y += FONTH * 2;
	if (vmenu) {
		int bh = y + m.menusel * step;
		blendbox(x - FONTH, bh - 10, x + w + FONTH, bh + FONTH + 10, false);
	};
	for(int i = 0; i < mdisp; ++i)
	{
		draw_text(m.items[i].text, x, y, 2);
		y += step;
	};
	return true;
}

void newmenu(char *name) {
	gmenu menu;
	menu.name = newIString(name);
	menu.menusel = 0;
	menus.emplace_back(menu);
}

void menumanual(int m, int n, char *text) {
	if (!n)
		menus[m].items.resize(0);
	mitem mitem;
	mitem.text = text;
	mitem.action = "";
	menus[m].items.emplace_back(mitem);
}

void menuitem(char *text, char *action) {
	gmenu &menu = menus.back();
	mitem mi;
	mi.text = newIString(text);
	mi.action = action[0] ? newIString(action) : mi.text;
	menu.items.emplace_back(mi);
}

COMMAND(menuitem, ARG_2STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);

bool menukey(int code, bool isdown) {
	if (vmenu <= 0)
		return false;
	int menusel = menus[vmenu].menusel;
	if (isdown) {
		if (code == SDLK_ESCAPE) {
			menuset(-1);
			if (!menustack.empty()) {
				int l = menustack.back();
				menustack.pop_back();
				menuset(l);
			}
			return true;
		} else if (code == SDLK_UP || code == -4)
			menusel--;
		else if (code == SDLK_DOWN || code == -5)
			menusel++;
		int n = menus[vmenu].items.size();
		if (menusel < 0)
			menusel = n - 1;
		else if (menusel >= n)
			menusel = 0;
		menus[vmenu].menusel = menusel;
	} else {
		if (code == SDLK_RETURN || code == -2) {
			char *action = menus[vmenu].items[menusel].action;
			if (vmenu == 1)
				connects(getservername(menusel));
			menustack.emplace_back(vmenu);
			menuset(-1);
			execute(action, true);
		};
	};
	return true;
}

