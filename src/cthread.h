////////////////////////////////////////////////////////////////////////////////
// CustomThread
//                                                           .... version 0.2
// ============================================================================
// (C)Copyright 2011 , rage.kim
//
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef __CUSTOMTHREAD_H__
#define __CUSTOMTHREAD_H__

#include <windows.h>
#include <process.h>

class CustomThread
{
    public:
        CustomThread();
        virtual ~CustomThread();

    public:
        bool Start(void * arg);
        HANDLE getHandle() { return hThread; }
        void SetSuspended(bool suspended);

    public:
        static void Sleep(unsigned ms) { ::Sleep(ms); }

    protected:
        static void EntryPoint(void*);

    protected:
        int Run(void * arg);
        virtual void Setup();
        virtual void Execute(void*) = 0;

        void *Arg() const {return Arg_;}
        void Arg(void* a){Arg_ = a;}

    private:
        unsigned    idThread;
        HANDLE      hThread;
        void*       Arg_;
        bool        suspended;

};

#endif // __CUSTOMTHREAD_H__
