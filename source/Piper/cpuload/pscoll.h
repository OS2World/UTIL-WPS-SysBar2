/*
** Module   :PSCOLL.H
** Abstract :Process collection
**
** Copyright (C) Sergey I. Yevtushenko
** Log: Fri  05/09/97   Last cleanup before sending it.
*/

#ifndef  __COLLECT_H
#include "collect.h"
#endif

#ifndef  __PSCOLL_H
#define  __PSCOLL_H


struct ProcInfo
{
    int iPid;
    int iSid;
    int iType;
    int iTouched;
    int lOldUser;
    int lDeltaUser;
    int lOldSystem;
    int lDeltaSystem;

    char cProxName[1025];
};

typedef ProcInfo* PInfo;

class ProcessCollection:public SortedCollection
{
        ProcInfo * CheckAndInsert(ProcInfo *, int&);
        ProcInfo * ProcessCollection::LocatePID(int pid);
        unsigned long ulTotalUsr;
        unsigned long ulTotalSys;

    public:
        ProcessCollection() {}
        ~ProcessCollection(){}

//        void Print( char *pszBuf );
        int GetCPULoad( void );
        void CollectData();
        virtual int Compare(Ptr p1,Ptr p2) {return PInfo(p1)->iPid - PInfo(p2)->iPid;}
};

#endif //__PSCOLL_H

