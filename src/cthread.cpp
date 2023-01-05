#include "cthread.h"

typedef unsigned (WINAPI *PBEGINTHREADEX_THREADFUNC) (LPVOID lpThreadParameter);

//constructor
CustomThread::CustomThread()
 : idThread(0),
   hThread(NULL),
   suspended(false)
{}

CustomThread::~CustomThread()
{
    if ( hThread != NULL )
    {
        _endthread();
        CloseHandle(hThread);
        hThread = NULL;
    }
}

bool CustomThread::Start(void * arg)
{
    Arg(arg);
    hThread = (HANDLE) _beginthreadex(NULL,0, (PBEGINTHREADEX_THREADFUNC)CustomThread::EntryPoint,this,0,&idThread);
    if(hThread)
        return true;

    return false;
}

int CustomThread::Run(void *arg)
{
   Setup();
   Execute( arg );
   CloseHandle(hThread);
   hThread = NULL;
   return 0;
}

void CustomThread::EntryPoint(void *pthis)
{
    if(pthis)
    {
        CustomThread *pt = (class CustomThread*)pthis;

        pt->Run( pt->Arg() );

        _endthread();
    }
}

void CustomThread::SetSuspended(bool suspended)
{
    this->suspended = suspended;
}

void CustomThread::Setup()
{
    // may inherits.
}

void CustomThread::Execute(void* arg)
{
    // may inherits.
}
