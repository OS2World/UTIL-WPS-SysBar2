/*
** Module   :COLLECT.CPP
** Abstract :Base Collection class implemetation
**
** Update : Sun  13-03-94 Updated
**          Fri  05/09/97 Last cleanup before sending it.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include "collect.h"

Collection::Collection(DWord aCount, DWord aDelta)
{
    dwCount = aCount;
    dwLast  = 0;
    dwDelta = aDelta;
    ppData  = new Ptr[dwCount];
}

Collection::~Collection()
{
    for(DWord i=0; i < dwLast;i++)
        Free(ppData[i]);
    delete ppData;
}

Ptr Collection::Get(DWord index)
{
    if (index < dwLast)
        return ppData[index];
	else
        return 0;
}

void Collection::Add(Ptr newitem)
{
    if (dwLast < dwCount)
        ppData[dwLast++] = newitem;
	else
	{
        Ptr * tmp = new Ptr[dwCount + dwDelta];
        memmove(tmp, ppData, sizeof(Ptr) * dwCount);
        dwCount += dwDelta;
        delete ppData;
        ppData = tmp;
        ppData[dwLast++] = newitem;
	}
}
void Collection::At(Ptr p, DWord pos)
{
    if(dwLast < dwCount)
    {
        if(pos > dwLast)
            pos = dwLast;
        if(pos == dwLast)
            ppData[dwLast++] = p;
        else
        {
            memmove(&ppData[pos+1], &ppData[pos], sizeof(Ptr)*(dwLast - pos));
            ppData[pos] = p;
            dwLast++;
        }
    }
    else
    {
        Ptr* tmp = new Ptr[dwCount+dwDelta];
        if(pos)
            memmove(tmp, ppData, sizeof(Ptr) * pos);
        if(pos < dwLast)
            memmove(&tmp[pos+1], &ppData[pos], sizeof(Ptr) * (dwLast - pos));
        tmp[pos] = p;
        dwCount += dwDelta;
        dwLast++;
        delete ppData;
        ppData = tmp;
    }
}

Ptr Collection::Remove(DWord item)
{
    Ptr tmp = Get(item);
    if(tmp)
    {
        memmove(&ppData[item], &ppData[item+1], sizeof(Ptr) * (dwCount-item-1));
        dwLast--;
    }
    return tmp;
}

void Collection::ForEach(ForEachFunc func)
{
    for(int i = 0; i < dwLast; i++)
        func(ppData[i]);
}

void Collection::RemoveAll()
{
    for(int i = 0; i < dwLast; i++)
        Free(ppData[i]);
}

//컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�

void SortedCollection::Add(Ptr p)
{
    if(!dwLast)
    {
        Collection::Add(p);
        return;
    }
    DWord pos = Look(p);
    int rc = 0;

    if(pos < dwLast)
        rc = Compare(p, Get(pos));
    else
    {
        Collection::Add(p);
        return;
    }
    if(( !rc && bDuplicates) || rc)
        At(p, pos);
}

DWord SortedCollection::Look(Ptr p)
{
    int l, m, h;

    l = m = 0;
    h = dwLast-1;

    while(l <= h)
    {
        m = (l+h) >> 1;
        int rc = Compare(p, Get(m));
        if(!rc)
            return m;
        if(rc < 0)
            h = m-1;
        if(rc > 0)
            l = m+1;
    }
    return l;
}

