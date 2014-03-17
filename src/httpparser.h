#ifndef __HTTPPARSER_H__
#define __HTTPPARSER_H__

class HTTPParser
{
    public:
        HTTPParser(const char* bytedata, int size);
        virtual ~HTTPParser();

    public:
        bool Parse();
        int  GetHeader(vector<string> &header);
        int  GetBody(char* byteBuffer, int bufferSize);
        void Flush();

    public:
        int  GetFileName(string &fname);
        int  GetContentSize();

    private:
        char* httpData;
        int   dataSize;
        char* bodyStartPtr;

    protected:
        vector<string> headerLines;
};

#endif /// of __HTTPPARSER_H__
