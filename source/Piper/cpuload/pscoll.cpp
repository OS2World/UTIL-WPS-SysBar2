/*
** Module   :PSCOLL.CPP
** Abstract :Process collection
**
** Copyright (C) Sergey I. Yevtushenko
** Log: Thu  03/04/97   Designed from two different source trees
**      Fri  05/09/97   Last cleanup before sending it.
*/

#define INCL_WIN
#define INCL_NLS
#define INCL_DOS
#define INCL_GPI
#include <os2.h>
#include "pscoll.h"
#include <string.h>
#include <stdio.h>
#include "dosqss.h"
#include <process.h>

#define PQ_BUFSIZE  0x10000
#define MQ_BUFSIZE  0x8000

PQTOPLEVEL pqData = 0;

APIRET QuerySysInfo(void)
{
    APIRET RC;
    if(!pqData)
        pqData = (PQTOPLEVEL) new char[PQ_BUFSIZE];
    memset(pqData, 0, PQ_BUFSIZE);
    RC = DosQuerySysState(0x1F, 0, 0, 0, pqData, PQ_BUFSIZE);
    return RC;
}

void chtab(char *i)
{
    char *tmp = strchr(i, '\x0A');
    
    while (tmp)
    {
        *tmp = ' ';
        tmp = strchr(i, '\x0A');
    }
    tmp = strchr(i, '\x0D');
    while (tmp)
    {
        *tmp = ' ';
        tmp = strchr(i, '\x0D');
    }
}

//-------------------------------------------------------

ProcInfo * ProcessCollection::LocatePID(int pid)
{
    for(int i = 0; i < Count(); i++)
    {
        ProcInfo * pInfo = (ProcInfo*)Get(i);
        if(pInfo && pInfo->iPid == pid)
            return pInfo;
    }
    return 0;
}

ProcInfo * ProcessCollection::CheckAndInsert(ProcInfo *pInfo, int& flag)
{
    DWord index = Look(pInfo);
    if(index < Count())
    {
        if(!Compare(pInfo, Get(index))) //------ Process exist ?
        {
            //------ Process already here
            delete pInfo;
            pInfo = (ProcInfo*)Get(index);
            flag = 0;
        }
        else //------ No process don't exist
        {
            Add(pInfo);
            flag = 1; //Additional info should be filled
        }
    }
    else
    {
        Add(pInfo);
        flag = 1;
    }
    return pInfo;
}

void ProcessCollection::CollectData()
{
    APIRET RC;

    RC = QuerySysInfo();
    if (!RC)
    {
        PQPROCESS pProcRecord = pqData->procdata;

        while ( pProcRecord->rectype != 3 )
        {
            PQTHREAD pThread = pProcRecord->threads;
            ProcInfo * pInfo = new ProcInfo;
            int iNeedFill = 0;

            pInfo->iPid = pProcRecord->pid;

//---- Check: if there any record found for that process

            pInfo = CheckAndInsert(pInfo, iNeedFill);

            if(iNeedFill)
            {
                //char *pszType;
                //char Title[256];
                pInfo->cProxName[0] = 0;
                pInfo->iSid = pProcRecord->sessid;
                pInfo->iType = pProcRecord->type;
                pInfo->lOldUser   =
                pInfo->lOldSystem =
                pInfo->lDeltaUser =
                pInfo->lDeltaSystem = 0;

                DosQueryModuleName(pProcRecord->hndmod, sizeof(pInfo->cProxName), pInfo->cProxName);
                if(!pInfo->cProxName[0])
                    strcpy(pInfo->cProxName, "<zombie>");

                //pszType = ppszType[(pInfo->iType <= 4 ) ? pInfo->iType:5];
                //if(pInfo->iType != 4 && !WinQueryTaskTitle(pInfo->iSid, Title, sizeof(Title)))
                //{
                //    chtab(Title);
                //    strcat(pInfo->cProxName, "(");
                //    strcat(pInfo->cProxName, Title);
                //    strcat(pInfo->cProxName, ")");
                //}
                //else
                //    if(!pInfo->cProxName[0])
                //        strcpy(pInfo->cProxName, "<zombie>");

            }
            if(pThread)
            {
                int j;
                unsigned long ulSys = 0;
                unsigned long ulUsr = 0;

                for(j = 0; j < pProcRecord->threadcnt; j++)
                {
                    ulSys += pThread->systime;
                    ulUsr += pThread->usertime;
                    pThread++;
                }
                pInfo->lDeltaUser   = ulUsr - pInfo->lOldUser;
                pInfo->lDeltaSystem = ulSys - pInfo->lOldSystem;
                pInfo->lOldUser     = ulUsr;
                pInfo->lOldSystem   = ulSys;
            }

//--- Anyway, process are here, touch it

            pInfo->iTouched = 1;

            pProcRecord = (PQPROCESS) (pThread) ;
        }
    }

//------- Ok, data filled. Cleanup database
    ulTotalSys = 0;
    ulTotalUsr = 0;
    for(int i = 0; i < Count(); i++)
    {
        ProcInfo * pInfo = (ProcInfo *)Get(i);
        if(pInfo->iTouched == 0)        // Process not touched last time
        {
            Free(Remove(i));
            i--;
        }
        else
        {
            ulTotalSys += pInfo->lDeltaSystem;
            ulTotalUsr += pInfo->lDeltaUser;
            pInfo->iTouched = 0;
        }
    }
    if((ulTotalSys+ulTotalUsr) <= 0)
    {
        ulTotalUsr = 0;
        ulTotalSys = 1;
    }
}

//void ProcessCollection::Print( char *pszBuf )
int ProcessCollection::GetCPULoad( void )
{
/*
    char cFiller[11];
    int usedLen;
    int freeLen;

    usedLen = (ulTotalUsr * 10)/(ulTotalUsr + ulTotalSys);
    if(usedLen < 0)
        usedLen = 0;

    if((((ulTotalUsr * 100)/(ulTotalUsr + ulTotalSys))%10) > 5)
        usedLen++;

    freeLen = 10 - usedLen;
    if(usedLen)
        memset(cFiller, '²', usedLen);
    if(freeLen)
        memset(&cFiller[usedLen], '°', freeLen);

    cFiller[10] = 0;

    printf("--------------\x1B[K\n"
           "[%s] %d%%\x1B[K\n"
           "--------------\x1B[K\n",
            cFiller,
           (ulTotalUsr*100)/(ulTotalUsr + ulTotalSys)
           );
*/
    return ( ulTotalUsr * 100 ) / ( ulTotalUsr + ulTotalSys );
//    if ( iLoad > 99 ) iLoad = 99;
//    if ( iLoad < 0 ) iLoad = 0;
//    sprintf( pszBuf, "%02d%%", iLoad );
//    fflush( stdout );
}

