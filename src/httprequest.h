#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

class I_HTTPEvent
{
    public:
        virtual void OnConnect() = 0;
        virtual void OnDisconnect() = 0;
        virtual void OnRead( const size_t readsize, const size_t maxsize ) = 0;
        virtual void OnError( const char* errorMsg );
        virtual void OnRetry( const int retrycount );
};

class I_HTTPRequest
{
    public:
        I_HTTPRequest() : event(NULL) {};

    public:
        virtual void dispose() = 0;
        virtual void step() = 0;
        virtual bool complete() = 0;
        virtual void rewind() = 0;
        virtual size_t read( void * ptr, size_t data ) = 0;

    public:
        I_HTTPEvent*    event;
};

I_HTTPRequest* GetNewHTTPRequest( char const * url );

#endif /// __HTTPREQUEST_H__
