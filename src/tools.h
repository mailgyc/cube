// generic useful stuff for any C++ program

#ifndef _TOOLS_H
#define _TOOLS_H

#ifdef __GNUC__
#define gamma __gamma
#endif

#include <math.h>

#ifdef __GNUC__
#undef gamma
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>
#include <algorithm>
#ifdef __GNUC__
#include <new>
#else
#include <new.h>
#endif

#ifdef NULL
#undef NULL
#endif
#define NULL 0

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define rnd(max) (rand()%(max))
#define rndreset() (srand(1))
#define rndtime() { loopi(lastmillis&0xF) rnd(i+1); }
#define loop(v,m) for(int v = 0; v<(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)

#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'

// easy safe IStrings

#define _MAXDEFSTR 260
typedef char IString[_MAXDEFSTR];

inline void strn0cpy(char *d, const char *s, size_t m) {
	strncpy(d, s, m);
	d[(m) - 1] = 0;
}

inline void strcpy_s(char *d, const char *s) {
	strn0cpy(d, s, _MAXDEFSTR);
}

inline void strcat_s(char *d, const char *s) {
	size_t n = strlen(d);
	strn0cpy(d + n, s, _MAXDEFSTR - n);
}

#define fast_f2nat(val) ((int)(val)) 

extern char *path(char *s);
extern char *loadfile(char *fn, int *size);
extern void endianswap(void *, int, int);

// memory pool that uses buckets and linear allocation for small objects
// VERY fast, and reasonably good memory reuse 

struct Pool {
	enum {
		POOLSIZE = 4096
	};   // can be absolutely anything
	enum {
		PTRSIZE = sizeof(char *)
	};
	enum {
		MAXBUCKETS = 65
	};   // meaning up to size 256 on 32bit pointer systems
	enum {
		MAXREUSESIZE = MAXBUCKETS * PTRSIZE - PTRSIZE
	};
	inline size_t bucket(size_t s) {
		return (s + PTRSIZE - 1) >> PTRBITS;
	}
	enum {
		PTRBITS = PTRSIZE == 2 ? 1 : PTRSIZE == 4 ? 2 : 3
	};

	char *p;
	size_t left;
	char *blocks;
	void *reuse[MAXBUCKETS];

	Pool();
	~Pool() {
		dealloc_block(blocks);
	}

	void *alloc(size_t size);
	void dealloc(void *p, size_t size);
	void *realloc(void *p, size_t oldsize, size_t newsize);

	char *IString(char *s, size_t l);
	char *IString(char *s) {
		return IString(s, strlen(s));
	}
	void deallocstr(char *s) {
		dealloc(s, strlen(s) + 1);
	}
	char *IStringbuf(char *s) {
		return IString(s, _MAXDEFSTR - 1);
	}

	void dealloc_block(void *b);
	void allocnext(size_t allocsize);
};


#define loopv(v)    if(false) {} else for(int i = 0; i<(v).size(); i++)

//template<class T> struct hashtable {
//	struct Chain {
//		Chain *next;
//		char *key;
//		T data;
//	};
//
//	int size;
//	int numelems;
//	Chain **table;
//	Pool *parent;
//	Chain *enumc;
//
//	hashtable() {
//		this->size = 1 << 10;
//		this->parent = gp();
//		numelems = 0;
//		table = (Chain **) parent->alloc(size * sizeof(T));
//		for (int i = 0; i < size; i++)
//			table[i] = NULL;
//	}
//
//	hashtable(hashtable<T> &v);
//	void operator=(hashtable<T> &v);
//
//	T *access(char *key, T *data = NULL) {
//		unsigned int h = 5381;
//		for (int i = 0, k; k = key[i]; i++)
//			h = ((h << 5) + h) ^ k;    // bernstein k=33 xor
//		h = h & (size - 1);                   // primes not much of an advantage
//		for (Chain *c = table[h]; c; c = c->next) {
//			for (char *p1 = key, *p2 = c->key, ch; (ch = *p1++) == *p2++;)
//				if (!ch)    //if(strcmp(key,c->key)==0)
//				{
//					T *d = &c->data;
//					if (data)
//						c->data = *data;
//					return d;
//				};
//		};
//		if (data) {
//			Chain *c = (Chain *) parent->alloc(sizeof(Chain));
//			c->data = *data;
//			c->key = key;
//			c->next = table[h];
//			table[h] = c;
//			numelems++;
//		};
//		return NULL;
//	}
//};

//#define enumerate(ht,t,e,b) loopi(ht->size) for(ht->enumc = ht->table[i]; ht->enumc; ht->enumc = ht->enumc->next) { t e = &ht->enumc->data; b; }

Pool *gp();
inline char *newIString(char *s) {
	return gp()->IString(s);
}

inline char *newIString(char *s, size_t l) {
	return gp()->IString(s, l);
}

inline char *newIStringbuf(char *s) {
	return gp()->IStringbuf(s);
}

#endif

