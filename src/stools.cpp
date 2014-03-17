#include <windows.h>
#include <tchar.h>

char *ConvertFromUnicode(wchar_t *src)
{
    static char *dest = NULL;

    if(dest)
        free(dest);

    int len = WideCharToMultiByte( CP_ACP, 0, src, -1, NULL, 0, NULL, NULL );
    dest = (char*)calloc(len+1,1);

    WideCharToMultiByte(CP_ACP,0,src,-1,dest,len,NULL,NULL);

    return dest;
}

wchar_t *ConvertFromMBCS(char *src)
{
    static wchar_t *dest = NULL;

    if(dest)
        free(dest);

    int len = strlen(src) + 1;
    dest = (wchar_t*)calloc(len * 2,1);
    MultiByteToWideChar(CP_ACP,0,src,-1,dest,len);

    return dest;

}

WORD     SwapWORD(WORD nWord)
{
    WORD nRet = 0;

    BYTE *pA1 = (BYTE*)&nWord;
    BYTE *pA2 = pA1+1;
    nRet = (*pA1 << 8 ) + *pA2;

    return nRet;
}

DWORD    SwapDWORD(DWORD nDWord)
{
    DWORD nRet = 0;

    BYTE *pA1 = (BYTE*)&nDWord;
    BYTE *pA2 = pA1+1;
    BYTE *pA3 = pA1+2;
    BYTE *pA4 = pA1+3;
    nRet = (*pA2 << 24 ) +
           (*pA1 << 16 ) +
           (*pA4 << 8  ) +
            *pA3;

    return nRet;
}

