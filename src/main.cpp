#include <windows.h>
#include <tchar.h>

#include "mainwindow.h"

int main(int argc, char ** argv)
{
    int         retCode = 0;
    MainWindow* wMain   = new MainWindow(argc, argv);

    if ( wMain != NULL )
    {
        while ( true )
        {
            if ( wMain->IsAlived() == false )
            {
                break;
            }

            ::Sleep(10);
        }

        retCode = wMain->GetReturnCode();

        delete wMain;
    }

    return retCode;
}
