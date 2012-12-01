/*
** Module   :COLLECT.H
** Abstract :
**
** Copyright (C) Sergey I. Yevtushenko
** Log: Fri  05/09/97   Last cleanup before sending it.
*/

#ifndef  __COLLECT_H
#define  __COLLECT_H

#ifndef  __BASE_H
#include "base.h"
#endif

CLASSDEF(Collection);

typedef void (*ForEachFunc)(Ptr);

class Collection
{
    protected:

        Ptr * ppData;
        DWord     dwLast;
        DWord     dwCount;
        DWord     dwDelta;
        Byte      bDuplicates;

    public:

        //ÄÄÄÄÄ New

        Collection(DWord aCount =10, DWord aDelta =5);
        ~Collection();
        Ptr Get(DWord);
        Ptr Remove(DWord);

        virtual void Add(Ptr);
        virtual void At(Ptr, DWord);
        virtual void Free(Ptr p)     { delete p;}

        void  ForEach(ForEachFunc);
        DWord Count()                {return dwLast;}
        void  RemoveAll();

};

CLASSDEF(SortedCollection);

class SortedCollection:public Collection
{
    public:

        //ÄÄÄÄÄ New

        SortedCollection(DWord aCount = 10, DWord aDelta = 5):
            Collection(aCount,aDelta){ bDuplicates = 1;};

        virtual int   Compare(Ptr p1,Ptr p2)
                        {return *((int *)p1) - *((int *)p2);}

        virtual DWord Look(Ptr);

        //ÄÄÄÄÄ Replaced

        virtual void Add(Ptr);
};

#endif //__COLLECT_H
