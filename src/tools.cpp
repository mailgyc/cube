// implementation of generic tools

#include "tools.h"
#include <new>

//////////////////////////// pool ///////////////////////////

Pool::Pool() {
	blocks = 0;
	allocnext(POOLSIZE);
	for (int i = 0; i < MAXBUCKETS; i++)
		reuse[i] = NULL;
}

void *Pool::alloc(size_t size) {
	if (size > MAXREUSESIZE) {
		return malloc(size);
	} else {
		size = bucket(size);
		void **r = (void **) reuse[size];
		if (r) {
			reuse[size] = *r;
			return (void *) r;
		} else {
			size <<= PTRBITS;
			if (left < size)
				allocnext(POOLSIZE);
			char *r = p;
			p += size;
			left -= size;
			return r;
		};
	};
}

void Pool::dealloc(void *p, size_t size) {
	if (size > MAXREUSESIZE) {
		free(p);
	} else {
		size = bucket(size);
		if (size)    // only needed for 0-size free, are there any?
		{
			*((void **) p) = reuse[size];
			reuse[size] = p;
		}
	}
}

void *Pool::realloc(void *p, size_t oldsize, size_t newsize) {
	void *np = alloc(newsize);
	if (!oldsize)
		return np;
	memcpy(np, p, newsize > oldsize ? oldsize : newsize);
	dealloc(p, oldsize);
	return np;
}

void Pool::dealloc_block(void *b) {
	if (b) {
		dealloc_block(*((char **) b));
		free(b);
	}
}

void Pool::allocnext(size_t allocsize) {
	char *b = (char *) malloc(allocsize + PTRSIZE);
	*((char **) b) = blocks;
	blocks = b;
	p = b + PTRSIZE;
	left = allocsize;
}

char *Pool::IString(char *s, size_t l) {
	char *b = (char *) alloc(l + 1);
	strncpy(b, s, l);
	b[l] = 0;
	return b;
}

Pool *gp() // useful for global buffers that need to be initialisation order independant
{
	static Pool *p = NULL;
	return p ? p : (p = new Pool());
}

///////////////////////// misc tools ///////////////////////
char *loadfile(char *fn, int *size) {
	FILE *f = fopen(fn, "rb");
	if (!f) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc(len + 1);
	if (!buf) {
		return NULL;
	}
	buf[len] = 0;
	size_t rlen = fread(buf, 1, len, f);
	fclose(f);
	if (len != rlen || len <= 0) {
		free(buf);
		return NULL;
	}
	if (size != NULL) {
		*size = len;
	}
	return buf;
}

void endianswap(void *memory, int stride, int length) // little indians as storage format
{
	if (*((char *) &stride))
		return;
	for(int w = 0; w < length; ++w) {
		for(int i = 0; i < stride/2; ++i)
		{
			unsigned char *p = (unsigned char *) memory + w * stride;
			unsigned char t = p[i];
			p[i] = p[stride - i - 1];
			p[stride - i - 1] = t;
		}
	}
}
