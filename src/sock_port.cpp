#include "sock_port.h"

#include <sys/types.h>
#include <sys/timeb.h>
#include <math.h>
#include <assert.h>

class init_gettimeofday
{
    public:
        init_gettimeofday()
        {
            timeBeginPeriod( 2 );

            __int64 rr = 0;

            QueryPerformanceFrequency( (LARGE_INTEGER *)&rr );
            ticksPerSecInv_ = 1.0 / (double)((DWORD)rr & 0xffffffff);
            int watchdog    = 0;

        again:
            lastTicks_ = timeGetTime();
            QueryPerformanceCounter( (LARGE_INTEGER *)&lastRead_ );

            timeb tb = {0};
            ftime( &tb );

            timeOffset_ = tb.time + tb.millitm * 0.001 - lastRead_ * ticksPerSecInv_;
            lastTime_   = timeOffset_;

            //  make sure it didn't take too long
            if( watchdog++ < 10 && (timeGetTime() != lastTicks_) )
            {
                goto again;
            }
        }

        ~init_gettimeofday()
        {
            timeEndPeriod( 2 );
        }

        void get( timeval * tv )
        {
            __int64 nu          = 0;
            int     watchdog    = 0;
        again:
            DWORD m = timeGetTime();
            QueryPerformanceCounter( (LARGE_INTEGER *)&nu );
            DWORD n = timeGetTime();

            if( (watchdog++ < 10) && (n != m) )
            {
                goto again;
            }

            double nuTime = nu * ticksPerSecInv_ + timeOffset_;
            if( (nu - lastRead_) & 0x7fffffff80000000ULL )
            {
                double adjust = (nuTime - lastTime_ - (n - lastTicks_) * 0.001);

                if( adjust > 0.1f )
                {
                    timeOffset_ -= adjust;
                    nuTime -= adjust;
                    assert( nuTime >= lastTime_ );
                }
            }
            lastRead_ = nu;
            lastTicks_ = n;
            lastTime_ = nuTime;
            tv->tv_sec = (ulong)floor( nuTime );
            tv->tv_usec = (ulong)(1000000 * (nuTime - tv->tv_sec));
        }

    private:
        double  ticksPerSecInv_;
        double  timeOffset_;
        double  lastTime_;
        __int64 lastRead_;
        DWORD   lastTicks_;
};

void gettimeofday( timeval * tv, int )
{
    static init_gettimeofday data;
    data.get( tv );
}

#include <assert.h>
#include <winsock2.h>
#include <windows.h>

//  This is somewhat less than ideal -- better would be if we could
//  abstract pollfd enough that it's non-copying on Windows.
int poll( pollfd * iofds, size_t count, int ms )
{
    FD_SET rd, wr, ex;
    FD_ZERO( &rd );
    FD_ZERO( &wr );
    FD_ZERO( &ex );
    SOCKET m = 0;

    for( size_t ix = 0; ix < count; ++ix )
    {
        iofds[ix].revents = 0;
        if( iofds[ix].fd >= m )
        {
            m = iofds[ix].fd + 1;
        }

        if( iofds[ix].events & (POLLIN | POLLPRI) )
        {
            assert( rd.fd_count < FD_SETSIZE );
            rd.fd_array[ rd.fd_count++ ] = iofds[ix].fd;
        }

        if( iofds[ix].events & (POLLOUT) )
        {
            assert( wr.fd_count < FD_SETSIZE );
            wr.fd_array[ wr.fd_count++ ] = iofds[ix].fd;
        }

        assert( ex.fd_count < FD_SETSIZE );
        ex.fd_array[ ex.fd_count++ ] = iofds[ix].fd;
    }

    timeval tv;
    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms - (tv.tv_sec * 1000)) * 1000;
    int r = 0;
    if( m == 0 )
    {
        ::Sleep( ms );
    }
    else
    {
        r = ::select( (int)m, (rd.fd_count ? &rd : 0), (wr.fd_count ? &wr : 0), (ex.fd_count ? &ex : 0), &tv );
    }

    if( r < 0 )
    {
        int err = WSAGetLastError();
        errno = err;
        return r;
    }

    r = 0;

    for( size_t ix = 0; ix < count; ++ix )
    {
        for( size_t iy = 0; iy < rd.fd_count; ++iy )
        {
            if( rd.fd_array[ iy ] == iofds[ix].fd )
            {
                iofds[ix].revents |= POLLIN;
                ++r;
                break;
            }
        }

        for( size_t iy = 0; iy < wr.fd_count; ++iy )
        {
            if( wr.fd_array[ iy ] == iofds[ix].fd )
            {
                iofds[ix].revents |= POLLOUT;
                ++r;
                break;
            }
        }

        for( size_t iy = 0; iy < ex.fd_count; ++iy )
        {
            if( ex.fd_array[ iy ] == iofds[ix].fd )
            {
                iofds[ix].revents |= POLLERR;
                ++r;
                break;
            }
        }
    }

    return r;
}
