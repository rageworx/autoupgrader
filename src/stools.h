#ifndef _STOOLS_H_
#define _STOOLS_H_

#ifdef _cplusplus
extern "C" {
#endif

char    *ConvertFromUnicode(wchar_t *src);
wchar_t *ConvertFromMBCS(char *src);

WORD     SwapWORD(WORD nWord);
DWORD    SwapDWORD(DWORD nDWord);

#ifdef _cplusplus
};
#endif

#endif // of _STOOLS_H_
