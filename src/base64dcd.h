#ifndef __BASE64DCD_H__
#define __BASE64DCD_H__

int base64_encode(const char *text, int numBytes, char **encodedText);
int base64_decode(const char *text, unsigned char *dst, int numBytes );

#endif // of __BASE64DCD_H__
