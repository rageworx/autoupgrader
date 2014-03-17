#include <windows.h>
#include <tchar.h>
#include <time.h>
#include <assert.h>
#include "resource.h"

#include <io.h>

#include <iostream>
#include <fstream>
using namespace std;

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Progress.H>

#include "mainwindow.h"
#include "coords.h"
#include "httprequest.h"
#include "httpparser.h"
#include "base64dcd.h"
#include "winproctool.h"

////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_FONT_SIZE   12
#define COPYRIGHT_FONT_SIZE 10
#define HTTP_BUFFER_SIZE    10 * 1024 * 1024


////////////////////////////////////////////////////////////////////////////////

static MainWindow* mwInstance   = NULL;
static I_HTTPRequest* httpReq   = NULL;
static HTTPParser* parser       = NULL;

////////////////////////////////////////////////////////////////////////////////

Fl_Box*     seqPreparing        = NULL;
Fl_Box*     seqRetrieving       = NULL;
Fl_Box*     seqProcessing       = NULL;
Fl_Box*     seqUpdating         = NULL;
Fl_Box*     seqFinalizing       = NULL;

Fl_Progress*        progBar     = NULL;
Fl_Box*     mli_info            = NULL;

string      updateURL;
string      updateData;
string      updateFile;

////////////////////////////////////////////////////////////////////////////////

void window_callback(Fl_Widget*, void*);
void timer_cb(void* p);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

UpgradeThread::UpgradeThread(I_UpgradeEvent* eventHandler)
 : event(NULL),
   workSeq(0)
{
    event = eventHandler;

    Start(this);
}

void UpgradeThread::Setup()
{
    if ( event != NULL )
    {
        event->OnWorkModeChange(workSeq);
    }
}

void UpgradeThread::Execute(void* p)
{
    while ( workSeq <= 5 )
    {
        switch( workSeq )
        {
            case 0: /// Preparing ...
                if ( event != NULL )
                {
                    event->OnMessageOut("Now preparing auto updating...\n"
                                        "Hold a second for it done.");

                }

                Sleep(1000);

                if ( readUpdateInfo() == false )
                {
                    if ( event != NULL )
                    {
                        event->OnMessageOut("Failed to preparing update!\n"
                                            "Check these:\n"
                                            "- update.inf missing\n"
                                            "- update.inf broken\n");
                    }
                    workSeq = 5;
                    break;
                }
                else
                {
                    workSeq++;
                }
                break;

            case 1: /// retrieving
                if ( event != NULL )
                {
                    event->OnMessageOut("Retriving update info file ...\n"
                                        "Hold a second !");
                }

                if ( getHTTPURL( updateURL.c_str() ) == false )
                {
                    if ( event != NULL )
                    {
                        event->OnMessageOut("Failed to get update file !\n"
                                            "Check these:\n"
                                            "- check internet connection.\n"
                                            "- check update server alive.\n");
                    }
                    workSeq = 5;
                    break;
                }
                else
                {
                    workSeq++;
                }
                break;

            case 2: /// processing
                if ( event != NULL )
                {
                    event->OnMessageOut("Parsing information ...\n"
                                        "Hold a second !");
                }

                if ( parseUpdateInfo() == false )
                {
                    if ( event != NULL )
                    {
                        event->OnMessageOut("Server returned error :\n"
                                            "Check server state.");
                    }
                    workSeq = 5;
                    break;
                }

                if ( updateBaseURL.size() > 0 )
                {
                    if ( event != NULL )
                    {
                        event->OnMessageOut(updateBaseURL.c_str());
                    }
                }
                else
                {
                    if ( event != NULL )
                    {
                        event->OnMessageOut("No information found :\n"
                                            "Base URL missing!");
                    }
                    workSeq = 5;
                    break;
                }
                workSeq++;
                break;

            case 3: /// updating
                if ( event != NULL )
                {
                    event->OnMessageOut("Killing processes ...\n"
                                        "Hold a second !");
                }
                killAllProcesses();


                if ( event != NULL )
                {
                    event->OnMessageOut("Starting receive ...");
                }

                if ( receivingFiles() == false )
                {
                    workSeq = 5;
                    break;
                }

                workSeq++;
                break;

            case 4: /// finalizing
                launchExecutive();
                workSeq++;
                break;

            case 5: /// quit
                workSeq++;
                break;
        }

        if ( event != NULL )
        {
            event->OnWorkModeChange(workSeq);
        }

        Fl::flush();
        ::Sleep(1);
    }

    if ( event != NULL )
    {
        event->OnWorkDone();
    }
}

void UpgradeThread::killAllProcesses()
{
    // WARNING : this is native Windows API.

    if ( killProcesses.size() > 0 )
    {
        for ( int cnt=0; cnt<killProcesses.size(); cnt++)
        {
            HANDLE hProc = GetProcessHandle( (LPSTR)killProcesses[cnt].c_str() );

            while( hProc != NULL )
            {
                DWORD fdwExit = 0;

                GetExitCodeProcess(hProc, &fdwExit);
                TerminateProcess(hProc, fdwExit);
                CloseHandle(hProc);

                hProc = GetProcessHandle( (LPSTR)killProcesses[cnt].c_str() );
            }
        }
    }
}

bool UpgradeThread::readUpdateInfo()
{
    fstream rStrm("update.inf");

    if ( rStrm.is_open() == true )
    {
        char aline[512] = {0};
        rStrm.getline(aline, 512);
        rStrm.close();

        int alinesize = strlen(aline);
        if ( alinesize > 0 )
        {
            char* tmpBuff = new char[alinesize];
            memset(tmpBuff, 0, alinesize);
            if ( base64_decode(aline, (unsigned char*)tmpBuff, alinesize) > 0 )
            {
                updateURL = tmpBuff;

                delete[] tmpBuff;

                return true;
            }

            delete[] tmpBuff;
        }
    }

    return false;
}

bool UpgradeThread::getHTTPURL(const char* URL, bool makefile, const char* fname)
{
    httpReq = GetNewHTTPRequest( URL );

    if ( httpReq != NULL )
    {
        char* httpBuffer        = NULL;
        int   httpBufferSize    = 0;
        int   bufferQueue       = 0;

        httpBuffer = new char[ HTTP_BUFFER_SIZE ];
        memset(httpBuffer, 0, HTTP_BUFFER_SIZE);

        time_t t         = {0};
        char   buf[4096] = {0};

        while( true )
        {
            httpReq->step();

            size_t rd = httpReq->read( buf, 4096 );
            if( rd > 0 )
            {
                memcpy(&httpBuffer[bufferQueue], buf, rd);
                bufferQueue += rd;
            }
            else
            {
                if( httpReq->complete() )
                {
                    httpBufferSize = bufferQueue;
                    goto next;
                }

                if( !t )
                {
                    time( &t );
                }
                else
                {
                    time_t t2;
                    time( &t2 );
                    if( t2 > t+10 )
                    {
                        httpBufferSize = bufferQueue;
                        goto next;
                    }
                }
            }
        }

        next:

        httpReq->dispose();

        if ( httpBufferSize == 0 )
        {
            return false;
        }

        HTTPParser* parser = new HTTPParser(httpBuffer, httpBufferSize);
        if ( parser != NULL )
        {
            string realFN;
            int    clen   = 0;

            if ( parser->Parse() == true )
            {
                vector<string> httpHeader;

                parser->GetHeader(httpHeader);
                parser->GetFileName(realFN);
                clen = parser->GetContentSize();

                if ( ( realFN.size() == 0 ) && ( makefile == true ) )
                {
                    realFN = fname;
                }

                if ( access(fname, F_OK) == 0 )
                {
                    remove(fname);
                }

                if ( ( realFN.size() > 0 ) && ( clen > 0 ) )
                {
                    char* data = new char[clen];
                    memset(data, 0, clen);
                    parser->GetBody(data, clen);

                    if ( makefile == true )
                    {
                        fstream newFile(realFN.c_str(), fstream::out | fstream::binary | fstream::trunc );

                        if ( newFile.is_open() == true )
                        {
                            newFile.write(&data[0], clen);
                            newFile.flush();
                            newFile.close();
                        }
                        else
                        {
                            delete[] data;
                            delete parser;
                            return false;
                        }
                    }
                    else
                    {
                        updateData.assign(data, clen);
                    }
                }
                else
                {
                    int   size = 0;
                    char* data = new char[1024*1024];

                    size = parser->GetBody(data, clen);

                    if ( size > 0 )
                    {
                        updateData = data;
                    }

                    delete[] data;
                }
            }

            delete parser;
        }

        if ( updateData.size() > 0 )
        {
            return true;
        }
    }

    return false;
}

bool UpgradeThread::parseUpdateInfo()
{
    if ( updateData.size() > 0 )
    {
		// Check Error returned.
		if ( updateData.find("ERROR:") != string::npos )
		{
            return false;
		}

        vector<string> parseLines;

        // seperate it lines to each -
        const char* baseData = updateData.c_str();

        char* tokenStart = (char*)baseData;
        char* tokenNext  = strstr(tokenStart, "\r\n");

        while ( tokenNext != NULL )
        {
            int   tokenSize = tokenNext - tokenStart;
            if ( tokenSize > 0 )
            {
                char* tmpStr    = new char[tokenSize + 1];
                memset(tmpStr, 0, tokenSize + 1);
                memcpy(tmpStr, tokenStart, tokenSize);

                parseLines.push_back(tmpStr);

                delete[] tmpStr;
            }

            tokenStart = tokenNext + 2;
            tokenNext  = strstr(tokenStart, "\r\n");
        }

        if ( parseLines.size() > 0 )
        {
            for(int cnt=0;cnt<parseLines.size();cnt++ )
            {
                const char* pokenStr = parseLines[cnt].c_str();
                int         pokenLen = strlen(pokenStr);
                string      impStr   = parseLines[cnt];

                if ( strncmp(pokenStr, "URL=", 4 ) == 0 )
                {
                    string splStr = impStr.substr(4, pokenLen - 4);
                    updateBaseURL = splStr;
                }
                else
                if ( strncmp(pokenStr, "FILES=", 6 ) == 0 )
                {
                    string splStr = impStr.substr(6, pokenLen - 6);
                    char* splitRef = (char*)splStr.c_str();

                    updateFiles.clear();

                    char* pokenCh = strtok(splitRef, ",");
                    while( pokenCh != NULL )
                    {
                        updateFiles.push_back(pokenCh);

                        pokenCh = strtok(NULL, ",");
                    }
                }
                else
                if ( strncmp(pokenStr, "KILLS=", 6 ) == 0 )
                {
                    string splStr = impStr.substr(6, pokenLen - 6);
                    char* splitRef = (char*)splStr.c_str();

                    killProcesses.clear();

                    char* pokenCh = strtok(splitRef, ",");
                    while( pokenCh != NULL )
                    {
                        killProcesses.push_back(pokenCh);

                        pokenCh = strtok(NULL, ",");
                    }
                }
                else
                if ( strncmp(pokenStr, "EXECUTE=", 8 ) == 0 )
                {
                    string splStr = impStr.substr(8, pokenLen - 8);
                    lastExecuteFile = splStr;
                }
            }
        }

        return true;
    }

    return false;
}

bool UpgradeThread::receivingFiles()
{
    if ( updateFiles.size() > 0 )
    {
        static char tmpPrt[512] = {0};

        for (int cnt=0; cnt<updateFiles.size(); cnt++ )
        {
            if ( event != NULL )
            {
                sprintf(tmpPrt,
                        "Now receiving a file :\n"
                        "... %s\n",
                        updateFiles[cnt].c_str() );
                event->OnMessageOut(tmpPrt);
            }

            string fullURL = updateBaseURL;
            fullURL += updateFiles[cnt];

            if ( getHTTPURL( fullURL.c_str(), true, updateFiles[cnt].c_str() ) == false )
            {
                if ( event != NULL )
                {
                    sprintf(tmpPrt,
                            "Failed to receiving a file :\n"
                            "%s\n",
                            updateFiles[cnt].c_str() );
                    event->OnMessageOut(tmpPrt);

                    return false;
                }
            }
        }

        return true;
    }

    return false;
}

void UpgradeThread::launchExecutive()
{
    ExecuteFile( (LPSTR)lastExecuteFile.c_str() );
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(int argc, char** argv)
 : returnCode(0),
   stillAlive(true),
   quitable(true),
   upgrader(NULL)
{
    _argc = argc;
    _argv = argv;

    if ( mwInstance != NULL )
    {
        delete this;
        return;
    }

    mwInstance = this;

    Run(this);
}

MainWindow::~MainWindow()
{
    Fl::remove_timeout(timer_cb, this);
}

void MainWindow::createComponents()
{
    Fl_Group* grpSequence = new Fl_Group(10,20,150,120, "Sequence :");
    if ( grpSequence != NULL )
    {
        grpSequence->box(FL_DOWN_BOX);
        grpSequence->align(Fl_Align(FL_ALIGN_TOP_LEFT));
        grpSequence->labelsize(DEFAULT_FONT_SIZE);
        grpSequence->begin();

            seqPreparing = new Fl_Box(20,30,130,20,"1. Preparing update ...");
            if ( seqPreparing != NULL )
            {
                seqPreparing->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
                seqPreparing->labelsize(DEFAULT_FONT_SIZE);
                seqPreparing->deactivate();
            }

            seqRetrieving = new Fl_Box(20,50,130,20,"2. Retrieving data ...");
            if ( seqRetrieving != NULL )
            {
                seqRetrieving->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
                seqRetrieving->labelsize(DEFAULT_FONT_SIZE);
                seqRetrieving->deactivate();
            }

            seqProcessing = new Fl_Box(20,70,130,20,"3. Processing data ...");
            if ( seqProcessing != NULL )
            {
                seqProcessing->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
                seqProcessing->labelsize(DEFAULT_FONT_SIZE);
                seqProcessing->deactivate();
            }

            seqUpdating = new Fl_Box(20,90,130,20,"4. Updating ...");
            if ( seqUpdating != NULL )
            {
                seqUpdating->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
                seqUpdating->labelsize(DEFAULT_FONT_SIZE);
                seqUpdating->deactivate();
            }

            seqFinalizing = new Fl_Box(20,110,130,20,"5. Finalizing ...");
            if ( seqFinalizing != NULL )
            {
                seqFinalizing->align( FL_ALIGN_LEFT | FL_ALIGN_INSIDE );
                seqFinalizing->labelsize(DEFAULT_FONT_SIZE);
                seqFinalizing->deactivate();
            }

        grpSequence->end();

        progBar = new Fl_Progress(10,150,150,10);
        if ( progBar != NULL )
        {
            progBar->color2(FL_BLUE);
            progBar->maximum(5);
            progBar->minimum(0);
            progBar->value(0);
        }

        mli_info = new Fl_Box(170,20,220,140,"");
        if ( mli_info != NULL )
        {
            mli_info->box( FL_DOWN_BOX );
            mli_info->align( FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE );
            mli_info->labelsize(DEFAULT_FONT_SIZE);
        }

        Fl_Box* boxCopyright = new Fl_Box(10,165,380,35);
        if ( boxCopyright != NULL )
        {
            boxCopyright->box(FL_DOWN_BOX);
            boxCopyright->align( FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE );
            boxCopyright->labelsize(COPYRIGHT_FONT_SIZE);
            boxCopyright->label("Automatic Updater, (C)Copyright 2013 Rageworx freeware.\n"
                                "All rights reserved, rageworx@gmail.com");
        }

    }
}

void MainWindow::disableAllSeq()
{
    seqPreparing->deactivate();
    seqRetrieving->deactivate();
    seqProcessing->deactivate();
    seqUpdating->deactivate();
    seqFinalizing->deactivate();
}

void MainWindow::writeInfo(const char* str)
{
    if ( mli_info != NULL )
    {
        static string infoStr;
        infoStr = str;
        mli_info->label(infoStr.c_str());
        mli_info->redraw();
    }
}

void MainWindow::OnWorkModeChange(const int workmode)
{
    progBar->value(workmode+1);
    disableAllSeq();

    switch( workmode )
    {
        case 0: /// Preparing ...
            seqPreparing->activate();
            break;

        case 1: /// retrieving
            seqRetrieving->activate();
            break;

        case 2: /// processing
            seqProcessing->activate();
            break;

        case 3: /// updating
            seqUpdating->activate();
            break;

        case 4: /// finalizing
            seqFinalizing->activate();
            break;

        case 5: /// quit
            break;
    }
}

void MainWindow::OnMessageOut(const char* msg)
{
    writeInfo(msg);
}

void MainWindow::OnWorkDone()
{
    quitable = true;
#ifndef _DEBUG
    exit(0);
#endif
}

void MainWindow::OnTimer(void* p)
{
    if ( upgrader != NULL )
        return;

    upgrader = new UpgradeThread(this);

    if ( upgrader != NULL )
    {
#ifndef _DEBUG
        quitable = false;
#endif
    }
}

void MainWindow::Setup()
{

}

void MainWindow::Execute(void* p)
{
    Fl_Double_Window* window = new Fl_Double_Window(ALIAS_WINDOW);

    if ( window != NULL )
    {
        extern HINSTANCE fl_display;
        window->icon((char *)LoadIcon(fl_display, MAKEINTRESOURCE(IDI_ICON_MAIN)));

        window->label("Automatic Upgrader WIN32");
        window->labelsize(DEFAULT_FONT_SIZE);
        window->begin();

        createComponents();

        window->end();
        window->show( _argc, _argv );
        window->callback(window_callback);

        Fl::add_timeout(0.5f, timer_cb, this);

        Fl::scheme("plastic");

        returnCode = Fl::run();
    }

    stillAlive = false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void window_callback(Fl_Widget*, void*)
{
    if ( mwInstance != NULL )
    {
        if ( mwInstance->IsQuitable() == true )
        {
            exit(0);
        }
    }
}

void timer_cb(void* p)
{
    if ( mwInstance != NULL )
    {
        mwInstance->OnTimer(p);
    }
}
