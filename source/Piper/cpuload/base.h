/*
** Module   :BASE.H
** Abstract :
**
** $Author$
** $Log$
*/

#ifndef  __BASE_H
#define  __BASE_H

typedef unsigned char Byte;
typedef unsigned short Word;
typedef unsigned long DWord;
typedef void *Ptr;
typedef Byte *PByte;
typedef Word *PWord;
typedef DWord *PDWord;
typedef char *CPtr;
typedef Word word;
typedef DWord dword;

#define CLASSPTR(Class) typedef Class* P##Class;
#define CLASSREF(Class) typedef Class& R##Class;
#define CLASSDEF(Class) class Class;\
                        CLASSPTR(Class)\
                        CLASSREF(Class)
#define STRUCDEF(Struc) struct Struc;\
                        CLASSPTR(Struc)\
                        CLASSREF(Struc)

#endif //__BASE_H

