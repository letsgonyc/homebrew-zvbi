#ifndef MISC_H
#define MISC_H

//////////////////////////
// generic macros/typedefs
//////////////////////////

#define FAC	(1<<16)		// factor for fix-point arithmetic

#define $ (void *)
#define NELEM(x) ((int)(sizeof(x)/sizeof(*(x))))
#define NORETURN(x) void x __attribute__((__noreturn__))
#define DEFINE(x) typeof(x) x
#define OFFSET_OF(type, elem) ((u8 *)&((type *)0)->elem - (u8 *)0)
#define BASE_OF(type, elem, p) ((type *)((u8 *)(p) - OFFSET_OF(type, elem)))

#define not !
#define elif else if
#define Case break; case
#define Default break; default
#define streq(a, b) (strcmp((a), (b)) == 0)
#define min(a,b) ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })
#define max(a,b) ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
#define bound(a,b,c) ({ typeof(a) _a = a; typeof(b) _b = b; typeof(c) _c = c; \
			_b < _a ? _a : _b > _c ? _c : _b; })

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

////////////////////////
// prototypes for misc.c
////////////////////////

extern char *prgname;
void setprgname(char *argv_0);

NORETURN(fatal(const char *str, ...));
NORETURN(fatal_ioerror(const char *str));
NORETURN(out_of_mem(int size));
void error(const char *str, ...);
void ioerror(const char *str);

#endif
