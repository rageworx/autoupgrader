#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "httprequest.h"
#include "sock_port.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define MAX_READ_RETRY          5
#define READ_COOLDOWN_SLEEP_MS  100
#define CHUNK_SIZE              128 * 1024           /// chunk data set to 128K.

////////////////////////////////////////////////////////////////////////////////

#define ERROR_REPORT( _x_ )     \
        if ( event != NULL )    \
        {                       \
            event->OnError( _x_ );  \
        }

#define ON_CONNECT()            \
        if ( event != NULL )    \
        {                       \
            event->OnConnect(); \
        }

#define ON_DISCONNECT()         \
        if ( event != NULL )    \
        {                       \
            event->OnConnect(); \
        }

#define ON_RETRY( _n_ )         \
        if ( event != NULL )    \
        {                       \
            event->OnRetry( _n_ );  \
        }

////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
static int strnicmp( char const * a, char const * b, int n )
{
    return strncasecmp( a, b, n );
}
#endif

////////////////////////////////////////////////////////////////////////////////

struct Chunk
{
    Chunk() : next_(NULL), size_(0)
    {
        memset(data_, 0, CHUNK_SIZE);
    }

    Chunk* next_;
    size_t size_;
    char data_[CHUNK_SIZE];
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class HTTPQuery : public I_HTTPRequest
{
    public:
        HTTPQuery()
         : head_(0),
           curRd_(0),
           curWr_(0),
           curOffset_(0),
           toRead_(0),
           complete_(false),
           gotLength_(false),
           socket_(BAD_SOCKET_FD),
           readretry(0)
        {
        }

        ~HTTPQuery()
        {
            if( socket_ != BAD_SOCKET_FD )
            {
                ::closesocket( socket_ );
                ON_DISCONNECT();
            }

            Chunk * ch = head_;

            while( ch != 0 )
            {
                Chunk * d = ch;
                ch = ch->next_;
                ::free( d );
            }
        }

        void setQuery( char const * host, unsigned short port, char const * url )
        {
            if( strlen( url ) > 1536 || strlen( host ) > 256 )
            {
                ERROR_REPORT("Bad host name");
                return;
            }

            struct hostent * hent = gethostbyname( host );
            if( hent == 0 )
            {
                ERROR_REPORT("Failed to get network host name");
                complete_ = true;
                return;
            }

            addr_.sin_family = AF_INET;
            addr_.sin_addr   = *(in_addr *)hent->h_addr_list[0];
            addr_.sin_port   = htons( port );

            socket_ = ::socket( AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto );
            if( socket_ == BAD_SOCKET_FD )
            {
                ERROR_REPORT("Bad TCP/IP socket");
                complete_ = true;
                return;
            }

            int r;
            r = ::connect( socket_, (sockaddr *)&addr_, sizeof( addr_ ) );

            if( r < 0 )
            {
                ERROR_REPORT("Failed to connect socket")
                complete_ = true;
                return;
            }

            MAKE_SOCKET_NONBLOCKING( socket_, r );

            if( r < 0 )
            {
                ERROR_REPORT("Failed to adjust socket to non-blocking");
                complete_ = true;
                return;
            }

            char buf[2048] = {0};
            sprintf( buf,
                     "GET %s HTTP/1.0\r\n"
                     "User-Agent: setQuery()\r\n"
                     "Accept: */*\r\n"
                     "Host: %s\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     url,
                     host );
            r = ::send( socket_, buf, int(strlen( buf )), NONBLOCK_MSG_SEND );

            if( r != strlen( buf ) )
            {
                ERROR_REPORT("Failed to send HTTP request");
                complete_ = true;
                return;
            }
        }

        void dispose()
        {
            delete this;
        }

        void step()
        {
            if( complete_ == false )
            {
                if( !curWr_ || (curWr_->size_ == sizeof( curWr_->data_ )) )
                {
                    Chunk* c = new Chunk();
                    if( !head_ )
                    {
                        head_ = c;
                        curWr_ = c;
                    }
                    else
                    {
                        curWr_->next_ = c;
                        curWr_ = c;
                    }
                }

                assert( curWr_ && (curWr_->size_ < sizeof( curWr_->data_ )) );

                int r = ::recv( socket_,
                                &curWr_->data_[curWr_->size_],
                                int(sizeof(curWr_->data_)-curWr_->size_),
                                NONBLOCK_MSG_SEND );

                if( r > 0 )
                {
                    curWr_->size_ += r;
                    assert( curWr_->size_ <= sizeof( curWr_->data_ ) );

                    if( gotLength_ > 0 )
                    {
                        if( toRead_ <= size_t(r) )
                        {
                            toRead_   = 0;
                            complete_ = true;
                        }
                        else
                        {
                            toRead_ -= r;
                        }

                        if ( event != NULL )
                        {
                            event->OnRead( curWr_->size_, toRead_ );
                        }
                    }

                    if( gotLength_ == 0 )
                    {
                        char const * end = &head_->data_[head_->size_];
                        char const * ptr = &head_->data_[1];

                        while( ptr < end-1 )
                        {
                            if( ptr[-1] == '\n' )
                            {
                                if( strnicmp( ptr, "content-length:", 15 ) == 0)
                                {
                                    ptr       += 15;
                                    toRead_    = strtol( ptr, (char **)&ptr, 10 );
                                    gotLength_ = true;
                                }
                                else
                                if( ( ptr[0] == '\r' ) && ( ptr[1] == '\n' ) )
                                {
                                    size_t haveRead = end-ptr-2;

                                    if( haveRead > toRead_ )
                                    {
                                        toRead_ = 0;
                                    }
                                    else
                                    {
                                        toRead_ -= haveRead;
                                    }

#ifdef READSTOP_WHEN_NO_CONTENT_LENGTH
                                    if( toRead_ == 0 )
                                    {
                                        complete_ = true;
                                    }
#endif
                                    break;
                                }
                            }

                            ++ptr;
                        }
                    }
                }
                else
                if( r < 0 )
                {
                    if( SOCKET_WOULDBLOCK_ERROR( SOCKET_ERRNO ) == false )
                    {
                        ON_RETRY( readretry + 1 );

                        readretry++;

                        if ( readretry > MAX_READ_RETRY )
                        {
                            complete_ = true;
                        }

                        ::Sleep(READ_COOLDOWN_SLEEP_MS);
                    }
                }
            }
        }

        bool complete()
        {
            step();
            return complete_;
        }

        void rewind()
        {
            curRd_ = head_;
            curOffset_ = 0;
        }

        size_t read( void* ptr, size_t size )
        {
            step();

            if( !head_ )
            {
                return 0;
            }

            if( !curRd_ )
            {
                curRd_ = head_;
                assert( curOffset_ == 0 );
            }

            size_t copied = 0;

            while( size > 0 )
            {
                assert( curRd_->size_ <= sizeof( curRd_->data_ ) );

                size_t toCopy = curRd_->size_ - curOffset_;

                if( toCopy > size )
                {
                    toCopy = size;
                }

                memcpy( ptr, &curRd_->data_[curOffset_], toCopy );
                curOffset_ += toCopy;

                assert( curOffset_ <= sizeof(curRd_->data_) );

                ptr     = ((char *)ptr)+toCopy;
                size   -= toCopy;
                copied += toCopy;

                if( curOffset_ == curRd_->size_ )
                {
                    if( curRd_->next_ != 0 )
                    {
                        curRd_ = curRd_->next_;
                        curOffset_ = 0;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            return copied;
        }

    protected:
        Chunk*      head_;
        Chunk*      curRd_;
        Chunk*      curWr_;
        size_t      curOffset_;
        size_t      toRead_;
        bool        complete_;
        bool        gotLength_;
        SOCKET      socket_;
        sockaddr_in addr_;
        int         readretry;
};

I_HTTPRequest* GetNewHTTPRequest( char const * url )
{
    static bool socketsInited;

    if( !socketsInited )
    {
        socketsInited = true;
        INIT_SOCKET_LIBRARY();
    }

    if( strncmp( url, "http://", 7 ) )
    {
        return NULL;
    }

    url += 7;
    char const * path = strchr( url, '/' );

    if( !path )
    {
        return NULL;
    }

    char name[ 256 ];
    if( path-url > 255 )
    {
        return 0;
    }

    strncpy( name, url, path-url );
    name[path-url] = 0;
    char* port = strrchr( name, ':' );
    unsigned short iport = 80;

    if( port )
    {
        *port = 0;
        iport = (unsigned short)( strtol( port+1, &port, 10 ) );
    }

    HTTPQuery* q = new HTTPQuery();
    q->setQuery( name, iport, path );

    return q;
}

void UnittestHTTPRequest()
{
    I_HTTPRequest* r = GetNewHTTPRequest( "http://www.cisco.com/" );
    char buf[1024] = {0};

    while( !r->complete() )
    {
        r->step();
        while( r->read( buf, sizeof( buf ) ) );
    }

    char buf2[100000];
    r->rewind();
    while( r->read( buf2, sizeof( buf2 ) ) );
    r->dispose();
}
