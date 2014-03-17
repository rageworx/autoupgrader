#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <vector>
#include <string>
using namespace std;

#include "cthread.h"

class I_UpgradeEvent
{
    public:
        virtual void OnWorkModeChange(const int workmode) = 0;
        virtual void OnMessageOut(const char* msg) = 0;
        virtual void OnWorkDone() = 0;
};

class UpgradeThread : CustomThread
{
    public:
        UpgradeThread(I_UpgradeEvent* eventHandler);

    public:
        I_UpgradeEvent* event;

    protected:
        void Setup();
        void Execute(void* p);

    protected:
        bool readUpdateInfo();
        bool getHTTPURL(const char* URL, bool makefile = false, const char* fname = NULL );
        bool parseUpdateInfo();
        void killAllProcesses();
        bool receivingFiles();
        void launchExecutive();

    protected:
        int             workSeq;
        string          updateBaseURL;
        vector<string>  updateFiles;
        vector<string>  killProcesses;
        string          lastExecuteFile;

};

class MainWindow : CustomThread, I_UpgradeEvent
{
    public:
        MainWindow(int argc, char** argv);
        virtual ~MainWindow();

    public:
        bool IsAlived() { return stillAlive; }
        int  GetReturnCode() { return returnCode; }
        bool IsQuitable() { return quitable; }
        void OnTimer(void* p);

    public:     /// inherited from I_UpgradeEvent;
        void OnWorkModeChange(const int workmode);
        void OnMessageOut(const char* msg);
        void OnWorkDone();

    protected:
        void Setup();
        void Execute(void* p);

    protected:
        void createComponents();
        void disableAllSeq();
        void writeInfo(const char* str);

    protected:
        int     _argc;
        char**  _argv;

    protected:
        int     returnCode;
        bool    stillAlive;
        bool    quitable;

    protected:
        UpgradeThread*  upgrader;
};

#endif /// of __MAINWINDOW_H__
