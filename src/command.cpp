// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include <sstream>
#include <iostream>
#include "cube.h"

enum {
	ID_VAR, ID_COMMAND, ID_ALIAS
};

struct Ident {
	int type;           // one of ID_* above
	char *name;
	int min, max;       // ID_VAR
	int *storage;       // ID_VAR
	void (*fun)();      // ID_VAR, ID_COMMAND
	int narg;           // ID_VAR, ID_COMMAND
	std::string action;       // ID_ALIAS
	bool persist;
};

template <typename T>
std::string tostr(T value) {
	std::ostringstream os;
	os << value;
	return os.str();
}

static std::map<std::string, Ident> *idents = NULL;    // contains ALL vars/commands/aliases

void alias(char *name, char *action) {

	if (idents->find(name) == idents->end()) {
		name = newIString(name);
		Ident b = { ID_ALIAS, name, 0, 0, 0, 0, 0, action, true };
		idents->insert(std::make_pair(name, b));
	} else {
		Ident &b = idents->at(name);
		if (b.type == ID_ALIAS)
			b.action = action;
		else
			conoutf("cannot redefine builtin %s with an alias", name);
	};
}

COMMAND(alias, ARG_2STR);

// variable's and commands are registered through globals, see cube.h

int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist) {
	if (!idents) {
		idents = new std::map<std::string, Ident>;
	}
	Ident v = { ID_VAR, name, min, max, storage, fun, 0, "", persist };
	idents->insert(std::make_pair(name, v));
	return cur;
}

void setvar(char *name, int i) {
	*(idents->at(name).storage) = i;
}

int getvar(char *name) {
	return *(idents->at(name).storage);
}

bool identexists(char *name) {
	return idents->find(name) != idents->end();
}

std::string getalias(const std::string &name) {
	if (idents->find(name) == idents->end()) {
		return "";
	}
	Ident &i = idents->at(name);
	return i.type == ID_ALIAS ? i.action : "";
}

bool addcommand(char *name, void (*fun)(), int narg) {
	if (!idents) {
		idents = new std::map<std::string, Ident>;
	}
	Ident c = { ID_COMMAND, name, 0, 0, 0, fun, narg, "", false };
	idents->insert(std::make_pair(name, c));
	return false;
}

std::string parseexp(char *&p, int right)  // parse any nested set of () or []
{
	int left = *p++;
	char *word = p;

	for (int brak = 1; brak;) {
		int c = *p++;
		if (c == '\r') {
			*(p - 1) = ' ';               // hack
		}
		if (c == left) {
			brak++;
		} else if (c == right) {
			brak--;
		} else if (c == 0) {
			p--;
			conoutf("missing \"%c\"", right);
			return "";
		}
	}
	std::string s(word, p - word - 1);
	if (left == '(') {
		s = tostr(execute(s.c_str())); // evaluate () exps directly, and substitute result
	}
	return s;
}

#define __cdecl
/*
 *  consume all accept char
 *  int strspn(string s, char[] accept)
 *
 *  consume all, until any reject appear
 *  int strcspn(string s, char[] reject)
 */
std::string parseword(char *&p)        // parse single argument, including expressions
{
	p += strspn(p, " \t\r");

	if (p[0] == '/' && p[1] == '/') {
		p += strcspn(p, "\n\0");
	}

	if (*p == '\"') {
		p++;
		char *word = p;
		p += strcspn(p, "\"\r\n\0");
		std::string s(word, p - word);
		if (*p == '\"') {
			p++;
		}
		return s;
	}
	if (*p == '(') {
		return parseexp(p, ')');
	}
	if (*p == '[') {
		return parseexp(p, ']');
	}

	char *word = p;
	p += strcspn(p, "; \t\r\n\0");
	if (p - word == 0)
		return "";
	return std::string(word, p - word);
}

std::string lookup(const std::string &n)            // find value of ident referenced with $ in exp
{
	std::string key = n.substr(1);
	if (idents->find(key) != idents->end()) {
		Ident &id = idents->at(key);
		switch (id.type) {
		case ID_VAR:
			return tostr(*(id.storage));
		case ID_ALIAS:
			return id.action;
		}
	}
	conoutf("\nunknown alias lookup: %s\n", n.c_str());
	return n;
}

int exec_command(bool isdown, int val, int numargs, Ident* id, const std::string w[]) {
	switch (id->narg) 	// use very ad-hoc function signature, and just call it
	{
	case ARG_1INT:
		if (isdown)
			((void (__cdecl *)(int)) id->fun)(std::stoi(w[1]));
		break;
	case ARG_2INT:
		if (isdown)
			((void (__cdecl *)(int, int)) id->fun)(std::stoi(w[1]), std::stoi(w[2]));
		break;
	case ARG_3INT:
		if (isdown) {
			int arg0 = std::stoi(w[1]);
			int arg1 = std::stoi(w[2]);
			int arg2 = w[3].empty() ? 0 : std::stoi(w[3]);
			((void (__cdecl *)(int, int, int)) id->fun)(arg0, arg1, arg2);
		}
		break;
	case ARG_4INT:
		if (isdown)
			((void (__cdecl *)(int, int, int, int)) id->fun)(std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]), std::stoi(w[4]));
		break;
	case ARG_NONE:
		if (isdown)
			((void (__cdecl *)()) id->fun)();
		break;
	case ARG_1STR:
		if (isdown)
			((void (__cdecl *)(char *)) id->fun)(w[1].c_str());
		break;
	case ARG_2STR:
		if (isdown)
			((void (__cdecl *)(char *, char *)) id->fun)(w[1].c_str(), w[2].c_str());
		break;
	case ARG_3STR:
		if (isdown)
			((void (__cdecl *)(char *, char *, char*)) id->fun)(w[1].c_str(), w[2].c_str(), w[3].c_str());
		break;
	case ARG_5STR:
		if (isdown)
			((void (__cdecl *)(char *, char *, char*, char*, char*)) id->fun)( w[1].c_str(), w[2].c_str(), w[3].c_str(), w[4].c_str(), w[5].c_str());
		break;
	case ARG_DOWN:
		((void (__cdecl *)(bool)) id->fun)(isdown);
		break;
	case ARG_DWN1:
		((void (__cdecl *)(bool, char *)) id->fun)(isdown, w[1].c_str());
		break;
	case ARG_1EXP:
		if (isdown)
			val = ((int (__cdecl *)(int)) id->fun)(execute(w[1].c_str()));
		break;
	case ARG_2EXP:
		if (isdown)
			val = ((int (__cdecl *)(int, int)) id->fun)(execute(w[1].c_str()), execute(w[2].c_str()));
		break;
	case ARG_1EST:
		if (isdown)
			val = ((int (__cdecl *)(char *)) id->fun)(w[1].c_str());
		break;
	case ARG_2EST:
		if (isdown)
			val = ((int (__cdecl *)(char *, char *)) id->fun)(w[1].c_str(), w[2].c_str());
		break;
	case ARG_VARI:
		if (isdown) {
			std::string r = "";               // limit, remove
			for (int i = 1; i < numargs; i++) {
				r += w[i]; // make string-list out of all arguments
				if (i == numargs - 1)
					break;
				r += " ";
			}
			((void (__cdecl *)(char *)) id->fun)(r.c_str());
			break;
		}
	}
	return val;
}

int execute(char *paction, bool isdown)    // all evaluation happens here, recursively
{
	const int MAXWORDS = 25;                    // limit, remove
	std::string wordbuf[MAXWORDS];
	int val = 0;
	for (bool cont = true; cont;)              // for each ; seperated statement
	{
		int numargs = MAXWORDS;
		for(int i = 0; i < MAXWORDS; ++i)      // collect all argument values
		{
			wordbuf[i] = "";
			if (i > numargs)
				continue;
			std::string s = parseword(paction);             // parse and evaluate exps
			if (s[0] == 0) {
				numargs = i;
				s = "";
			}
			if (s[0] == '$') {
				s = lookup(s);          // substitute variables
			}
			wordbuf[i] = s;
		}
		paction += strcspn(paction, ";\n\0");
		cont = *paction++ != 0; // more statements if this isn't the end of the IString
		std::string c = wordbuf[0];
		if (c[0] == '/') {
			c = c.substr(1); // strip irc-style command prefix
		}
		if (c[0] == 0) {
			continue;                  // empty statement
		}

		if (idents->find(c) == idents->end()) {
			val = std::stoi(c);
			if (!val && c[0] != '0')
				conoutf("unknown command: %s", c.c_str());
		} else {
			Ident *id = &(idents->at(c));
			switch (id->type) {
			case ID_COMMAND:      		// game defined commands
				val = exec_command(isdown, val, numargs, id, wordbuf);
				break;
			case ID_VAR:                        // game defined variabled 
				if (isdown) {
					if (!wordbuf[1][0]) {
						conoutf("%s = %d", c.c_str(), *id->storage); // var with no value just prints its current value
					} else {
						if (id->min > id->max) {
							conoutf("variable is read-only");
						} else {
							int i1 = std::stoi(wordbuf[1]);
							if (i1 < id->min || i1 > id->max) {
								i1 = i1 < id->min ? id->min : id->max; // clamp to valid range
								conoutf("valid range for %s is %d..%d", c.c_str(), id->min, id->max);
							}
							*id->storage = i1;
						};
						if (id->fun) {
							((void (__cdecl *)()) id->fun)(); // call trigger function if available
						}
					};
				}
				break;
			case ID_ALIAS: // alias, also used as functions and (global) variables
				for (int i = 1; i < numargs; i++) {
					char t[20];
					std::sprintf(t, "arg%d", i); // set any arguments as (global) arg values so functions can access them
					alias(t, wordbuf[i].c_str());
				}
				val = execute(id->action.c_str(), isdown);
				break;
			}
		}
	}
	return val;
}

// tab-completion of all idents
int completesize = 0, completeidx = 0;

void resetcomplete() {
	completesize = 0;
}

void complete(char *s) {
	if (*s != '/') {
		IString t;
		strcpy_s(t, s);
		strcpy_s(s, "/");
		strcat_s(s, t);
	};
	if (!s[1])
		return;
	if (!completesize) {
		completesize = (int) strlen(s) - 1;
		completeidx = 0;
	};
	int idx = 0;
	for(auto &id : *idents) {
		if(strncmp(id.second.name, s+1, completesize)==0 && idx++==completeidx) {
			strcpy_s(s, "/"); strcat_s(s, id.second.name);
		}
	}
	completeidx++;
	if (completeidx >= idx)
		completeidx = 0;
}

bool execfile(char *cfgfile) {
	char *buf = loadfile(cfgfile, NULL);
	if (!buf) {
		return false;
	}
	execute(buf);
	free(buf);
	return true;
}

void exec(char *cfgfile) {
	if (!execfile(cfgfile))
		conoutf("could not read \"%s\"", cfgfile);
}

void writecfg() {
	FILE *f = fopen("config.cfg", "w");
	if (!f)
		return;
	fprintf(f, "// automatically written on exit, do not modify delete this file to have defaults.cfg overwrite these settings modify settings in game, or put settings in autoexec.cfg to override anything\n");
	writeclientinfo(f);
	fprintf(f, "\n");
	for(auto &id : *idents) {
		if(id.second.type==ID_VAR && id.second.persist) {
			fprintf(f, "%s %d\n", id.second.name, *(id.second.storage));
		}
	}
	fprintf(f, "\n");
	writebinds(f);
	fprintf(f, "\n");
	for(auto &id : *idents) {
		if(id.second.type==ID_ALIAS && !strstr(id.second.name, "nextmap_")) {
			fprintf(f, "alias \"%s\" [%s]\n", id.second.name, id.second.action.c_str());
		}
	}
	fclose(f);
}

COMMAND(writecfg, ARG_NONE);

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void intset(char *name, int v) {
	std::string t = tostr(v);
	alias(name, t.c_str());
}

void ifthen(char *cond, char *thenp, char *elsep) {
	execute(cond[0] != '0' ? thenp : elsep);
}

void loopa(char *times, char *body) {
	int t = atoi(times);
	for(int i = 0; i < t; ++i)
	{
		intset("i", i);
		execute(body);
	}
}

void whilea(char *cond, char *body) {
	while (execute(cond))
		execute(body);
}

// can't get any simpler than this :)
void onrelease(bool on, char *body) {
	if (!on)
		execute(body);
}

void concat(char *s) {
	alias("s", s);
}

void concatword(char *s) {
	for (char *a = s, *b = s; *a = *b; b++)
		if (*a != ' ')
			a++;
	concat(s);
}

int listlen(char *a) {
	if (!*a)
		return 0;
	int n = 0;
	while (*a)
		if (*a++ == ' ')
			n++;
	return n + 1;
}

void at(char *s, char *pos) {
	int n = atoi(pos);
	for(int i = 0; i < n; ++i) {
		s += strcspn(s, " \0");
		s += strspn(s, " ");
	}
	s[strcspn(s, " \0")] = 0;
	concat(s);
}

COMMANDN(loop, loopa, ARG_2STR);
COMMANDN(while, whilea, ARG_2STR);
COMMANDN(if, ifthen, ARG_3STR);
COMMAND(onrelease, ARG_DWN1);
COMMAND(exec, ARG_1STR);
COMMAND(concat, ARG_VARI);
COMMAND(concatword, ARG_VARI);
COMMAND(at, ARG_2STR);
COMMAND(listlen, ARG_1EST);

int add(int a, int b) {
	return a + b;
}

COMMANDN(+, add, ARG_2EXP);
int mul(int a, int b) {
	return a * b;
}

COMMANDN(*, mul, ARG_2EXP);
int sub(int a, int b) {
	return a - b;
}

COMMANDN(-, sub, ARG_2EXP);
int divi(int a, int b) {
	return b ? a / b : 0;
}

COMMANDN(div, divi, ARG_2EXP);
int mod(int a, int b) {
	return b ? a % b : 0;
}

COMMAND(mod, ARG_2EXP);
int equal(int a, int b) {
	return (int) (a == b);
}

COMMANDN(=, equal, ARG_2EXP);
int lt(int a, int b) {
	return (int) (a < b);
}

COMMANDN(<, lt, ARG_2EXP);
int gt(int a, int b) {
	return (int) (a > b);
}

COMMANDN(>, gt, ARG_2EXP);

int strcmpa(char *a, char *b) {
	return strcmp(a, b) == 0;
}

COMMANDN(strcmp, strcmpa, ARG_2EST);

int rndn(int a) {
	return a > 0 ? rnd(a) : 0;
}

COMMANDN(rnd, rndn, ARG_1EXP);

int explastmillis() {
	return lastmillis;
}

COMMANDN(millis, explastmillis, ARG_1EXP);

