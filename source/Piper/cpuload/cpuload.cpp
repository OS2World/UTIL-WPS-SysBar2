/*
** Module   :CPULOAD.CPP
** Abstract :Main routine for text-mode CPU usage meter.
**
** Copyright (C) Sergey I. Yevtushenko
** Log: Fri  05/09/97   Last cleanup before sending it.
*/

#define INCL_BASE
#define INCL_MODULEMGR
#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_ERRORS
#include <os2.h>
#include "pscoll.h"
#include <stdio.h>

ULONG APIENTRY BugHandler(PEXCEPTIONREPORTRECORD p1,
						  PEXCEPTIONREGISTRATIONRECORD p2,
						  PCONTEXTRECORD p3,
						  PVOID pv);

main()
{
    EXCEPTIONREGISTRATIONRECORD RegRec = { 0 };
	APIRET rc = NO_ERROR;

	RegRec.ExceptionHandler = (ERR) BugHandler;
	rc = DosSetExceptionHandler(&RegRec);
	if (rc != NO_ERROR)
	{
		printf("DosSetExceptionHandler error: return code = %u\n", rc);
		return 1;
	}

	ProcessCollection pcList;

	for (;;)
	{
//		printf("\x1B[H\x1B[K");
		pcList.CollectData();
		pcList.Print();
		DosSleep(2000);
	}

}

ULONG APIENTRY BugHandler(PEXCEPTIONREPORTRECORD p1,
						  PEXCEPTIONREGISTRATIONRECORD p2,
						  PCONTEXTRECORD p3,
						  PVOID pv)
{
    if(   p1->ExceptionNum     == XCPT_ACCESS_VIOLATION
       && p1->ExceptionInfo[1] == 0xFFFFFFFF
       && p1->ExceptionInfo[0] == 0)
    {
            return XCPT_CONTINUE_EXECUTION;
    }
    return XCPT_CONTINUE_SEARCH;
}

