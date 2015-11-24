#ifndef SRC_VM_H_
#define SRC_VM_H_

#include <map>

struct Token {
	int type;           // one of ID_* above
	char *name;
	int min, max;       // ID_VAR
	int *storage;       // ID_VAR
	void (*fun)();      // ID_VAR, ID_COMMAND
	int narg;           // ID_VAR, ID_COMMAND
	char *action;       // ID_ALIAS
	bool persist;
};

class VM {
public:
	static VM* getInstance();

	// command
	int variable(char *name, int min, int cur, int  max, int *storage, void (*fun)(), bool persist);

	void setvar(char *name, int i);
	int getvar(char *name);

	void alias(char *name, char *action);
	char *getalias(char *name);

	bool identexists(char *name);

	bool addcommand(char *name, void (*fun)(), int narg);

	char* parseexp(char *&p, int right);
	char* parseword(char *&p);

	char* lookup(char *n);

	int execute(char *p, bool down = true);
	void exec(char *cfgfile);
	bool execfile(char *cfgfile);

	void resetcomplete();
	void complete(char *s);


	void writecfg();

private:
	std::map<std::string, Token> env;
	int completesize = 0;
	int completeidx = 0;
};

#endif /* SRC_VM_H_ */
