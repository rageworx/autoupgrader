#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

using namespace std;

#include "httpparser.h"

////////////////////////////////////////////////////////////////////////////////

const char tokenDilim[3] = { 0x0D, 0x0A, 0x00 };

////////////////////////////////////////////////////////////////////////////////

HTTPParser::HTTPParser(const char* bytedata, int size)
 : httpData(NULL),
   dataSize(0),
   bodyStartPtr(NULL)
{
    if ( bytedata != NULL )
    {
        httpData = new char[size];
        if ( httpData != NULL )
        {
            memcpy( httpData, bytedata , size );
            dataSize = size;
        }
    }
}

HTTPParser::~HTTPParser()
{
    Flush();
}

bool HTTPParser::Parse()
{
    if ( ( httpData != NULL ) && ( dataSize > 0 ) )
    {
        headerLines.clear();

        int   curQueue     = 0;
        char* tempHeadLine = NULL;
        char* tokenStart   = httpData;
        char* tokenNext    = NULL;

        tokenNext = strstr(tokenStart, tokenDilim);

        while ( true )
        {
            int tokenSize = tokenNext - tokenStart;
            curQueue += tokenSize;

            if ( curQueue > dataSize )
            {
                break;
            }

            if ( tokenSize > 0 )
            {
                tempHeadLine = new char[tokenSize + 1];
                if ( tempHeadLine != NULL )
                {
                    memset(tempHeadLine, 0, tokenSize + 1);
                    memcpy(tempHeadLine, tokenStart, tokenSize);

                    headerLines.push_back(tempHeadLine);

                    delete[] tempHeadLine;
                }
            }
            else
            {
                // if http corrupted, this calculation failure.
                if ( curQueue + 2 >= dataSize )
                {
                    bodyStartPtr = NULL;
                    break;
                }

                // if double \r\n means end of header ..

                bodyStartPtr  = tokenNext + 2;

                return true;
            }

            // increase 0x0D,0x0A 2bytes;
            tokenStart = tokenNext + 2;

            if ( strncmp( tokenStart, tokenDilim, 2) == 0 )
            {
                bodyStartPtr = tokenStart + 2;

                return true;
            }

            tokenNext  = strstr(tokenStart, tokenDilim);
        }

        return true;
    }

    return false;
}

int HTTPParser::GetHeader(vector<string> &header)
{
    int cnt = 0;

    for ( cnt=0; cnt<headerLines.size(); cnt++ )
    {
        header.push_back( headerLines[cnt] );
    }

    return cnt;
}

int HTTPParser::GetBody(char* byteBuffer, int bufferSize)
{
    if ( byteBuffer != NULL )
    {
        int copySize = dataSize;

        if ( copySize > bufferSize )
        {
            copySize = bufferSize;
        }

        if ( copySize > 0 )
        {
            memcpy(byteBuffer, bodyStartPtr, copySize);
        }

        return copySize;
    }

    return 0;
}

void HTTPParser::Flush()
{
    if ( httpData != NULL )
    {
        delete[] httpData;
        httpData = NULL;
    }

    dataSize = 0;

    headerLines.clear();
    bodyStartPtr = NULL;
}

int HTTPParser::GetFileName(string &fname)
{
    if ( headerLines.size() > 0 )
    {
        for( int cnt=0; cnt<headerLines.size(); cnt++ )
        {
            string::size_type fnpos = headerLines[cnt].find("filename=");
            if ( fnpos != string::npos )
            {
                string::size_type subPos = fnpos + 9;
                string::size_type subLen = headerLines[cnt].size() - subPos;

                string tmpFN = headerLines[cnt].substr( subPos, subLen );
                if ( tmpFN.size() > 0 )
                {
                    // remove " ...
                    while( true )
                    {
                        string::size_type fndpos = tmpFN.find("\"");
                        if ( fndpos == string::npos )
                        {
                            break;
                        }

                        tmpFN.replace( fndpos, 1 , "");
                    }

                }

                fname = tmpFN;
                return tmpFN.size();
            }
        }
    }

    return 0;
}

int HTTPParser::GetContentSize()
{
    if ( headerLines.size() > 0 )
    {
        for( int cnt=0; cnt<headerLines.size(); cnt++ )
        {
            string::size_type clpos = headerLines[cnt].find("Content-Length: ");
            if ( clpos != string::npos )
            {
                string::size_type subPos = clpos + 16;
                string::size_type subLen = headerLines[cnt].size() - subPos;

                string tmpCL = headerLines[cnt].substr( subPos, subLen );
                if ( tmpCL.size() > 0 )
                {
                    return atoi(tmpCL.c_str());
                }
            }
        }
    }
}

