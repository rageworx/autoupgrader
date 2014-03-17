#include <windows.h>
#include <TlHelp32.h>
#include <tchar.h>

#include <string>
using namespace std;

#include "stools.h"

HANDLE GetProcessHandle(LPSTR szExeName)
{
    PROCESSENTRY32 Pc = { sizeof(PROCESSENTRY32) };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);

    if(Process32First(hSnapshot, &Pc))
    {
        do
        {
#ifdef _UNICODE
            TCHAR* diffNamePtr = ConvertFromMBCS( szExeName );
#else
            TCHAR* diffNamePtr = szExeName;
#endif

            if( _tcscmp(Pc.szExeFile, diffNamePtr) == 0 )
            {
                return OpenProcess(PROCESS_ALL_ACCESS, TRUE, Pc.th32ProcessID);
            }
        }
        while(Process32Next(hSnapshot, &Pc));
    }

    return NULL;
}


void ExecuteFile(LPSTR szExeName)
{
    TCHAR path[_MAX_PATH] = {0};

    GetModuleFileName(NULL, path, sizeof path);

 #ifdef _UNICODE
    TCHAR* exeNamePtr = ConvertFromMBCS( szExeName );

    wstring tmpStr = path;
    wstring::size_type rPos = tmpStr.rfind(L"\\") + 1;
    wstring exePath = tmpStr.substr(0, rPos);
    exePath += exeNamePtr;

#else
    TCHAR* exeNamePtr = szExeName;

    string tmpStr = path;
    string::size_type rPos = tmpStr.rfind("\\") + 1;
    string exePath = tmpStr.substr(0, rPos);
    exePath += exeNamePtr;
#endif

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    CreateProcess(NULL,
                  (TCHAR*)exePath.c_str(),
                  NULL,
                  NULL,
                  FALSE,
                  0,
                  NULL,
                  NULL,
                  &si,
                  &pi);



}
