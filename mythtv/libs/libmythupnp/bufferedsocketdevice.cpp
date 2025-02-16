//////////////////////////////////////////////////////////////////////////////
// Program Name: bufferedsocketdevice.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "bufferedsocketdevice.h"

#include <algorithm>
#include <array>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for
#include <utility>

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDevice::BufferedSocketDevice( int nSocket  )
  : m_pSocket(new MSocketDevice())
{
    m_pSocket->setSocket         ( nSocket, MSocketDevice::Stream );
    m_pSocket->setBlocking       ( false );
    m_pSocket->setAddressReusable( true );

    struct linger ling = {1, 1};

    if ( setsockopt(socket(), SOL_SOCKET, SO_LINGER, (const char *)&ling,
                    sizeof(ling)) < 0) 
        LOG(VB_GENERAL, LOG_ERR, 
            "BufferedSocketDevice: setsockopt - SO_LINGER: " + ENO);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDevice::BufferedSocketDevice( MSocketDevice *pSocket /* = nullptr*/,
                                            bool bTakeOwnership /* = false */ )
  : m_pSocket(pSocket),
    m_bHandleSocketDelete(bTakeOwnership)
{}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDevice::~BufferedSocketDevice()
{
    Close();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::Close()
{
    Flush();
    ReadBytes();

    m_bufRead.clear();
    ClearPendingData();

    if (m_pSocket != nullptr)
    {
        if (m_pSocket->isValid())
            m_pSocket->close();

        if (m_bHandleSocketDelete)
            delete m_pSocket;

        m_pSocket = nullptr;
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::Connect( const QHostAddress &addr, quint16 port )
{
    if (m_pSocket == nullptr)
        return false;

    return m_pSocket->connect( addr, port );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MSocketDevice *BufferedSocketDevice::SocketDevice()
{
    return( m_pSocket );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetSocketDevice( MSocketDevice *pSocket )
{
    if ((m_bHandleSocketDelete) && (m_pSocket != nullptr))
        delete m_pSocket;
    
    m_bHandleSocketDelete = false;

    m_pSocket = pSocket;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetDestAddress(
    QHostAddress hostAddress, quint16 nPort)
{
    m_destHostAddress = std::move(hostAddress);
    m_nDestPort       = nPort;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetReadBufferSize( qulonglong bufSize )
{
    m_nMaxReadBufferSize = bufSize;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qulonglong BufferedSocketDevice::ReadBufferSize(void) const
{
    return m_nMaxReadBufferSize;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int BufferedSocketDevice::ReadBytes()
{
    if (m_pSocket == nullptr)
        return m_bufRead.size();

    qlonglong maxToRead = 0;

    if ( m_nMaxReadBufferSize > 0 ) 
    {
        maxToRead = m_nMaxReadBufferSize - m_bufRead.size();

        if ( maxToRead <= 0 ) 
            return m_bufRead.size();
    }

    qlonglong nbytes = m_pSocket->bytesAvailable();

    QByteArray *a = nullptr;

    if ( nbytes > 0 )
    {
        a = new QByteArray();
        a->resize(nbytes);

        qlonglong nread = m_pSocket->readBlock(
            a->data(), maxToRead ? std::min(nbytes, maxToRead) : nbytes);

        if (( nread > 0 ) && ( nread != a->size() ))
        {
            // unexpected
            a->resize( nread );
        }
    }

    if (a)
    {
#if 0
        QString msg;
        for( long n = 0; n < a->count(); n++ )
            msg += QString("%1").arg(a->at(n));
        LOG(VB_GENERAL, LOG_DEBUG, msg);
#endif

        m_bufRead.append( a );
    }

    return m_bufRead.size();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::ConsumeWriteBuf( qulonglong nbytes )
{
    if ( !nbytes || ((qlonglong)nbytes > m_nWriteSize) )
        return false;

    m_nWriteSize -= nbytes;

    for ( ;; ) 
    {
        QByteArray *a = m_bufWrite.front();

        if ( m_nWriteIndex + nbytes >= (qulonglong)a->size() ) 
        {
            nbytes -= a->size() - m_nWriteIndex;
            m_bufWrite.pop_front();
            delete a;

            m_nWriteIndex = 0;

            if ( nbytes == 0 )
                break;
        } 
        else 
        {
            m_nWriteIndex += nbytes;
            break;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::Flush()
{

    if ((m_pSocket == nullptr) || !m_pSocket->isValid())
        return;

    bool osBufferFull = false;
    //int  consumed     = 0;

    while ( !osBufferFull && ( m_nWriteSize > 0 ) && m_pSocket->isValid())
    {
        auto it = m_bufWrite.begin();
        QByteArray *a = *it;

        int nwritten = 0;
        int i = 0;

        if ( a->size() - m_nWriteIndex < 1460 )
        {
            QByteArray out;
            out.resize(65536);

            int j = m_nWriteIndex;
            int s = a->size() - j;

            while ( a && i+s < out.size() )
            {
                memcpy( out.data()+i, a->data()+j, s );
                j = 0;
                i += s;
                ++it;
                a = *it;
                s = a ? a->size() : 0;
            }

            if (m_nDestPort != 0)
                nwritten = m_pSocket->writeBlock( out.data(), i, m_destHostAddress, m_nDestPort );
            else
                nwritten = m_pSocket->writeBlock( out.data(), i );
        } 
        else 
        {
            // Big block, write it immediately
            i = a->size() - m_nWriteIndex;

            if (m_nDestPort != 0)
                nwritten = m_pSocket->writeBlock( a->data() + m_nWriteIndex, i, m_destHostAddress, m_nDestPort );
            else
                nwritten = m_pSocket->writeBlock( a->data() + m_nWriteIndex, i );
        }

        if ( nwritten > 0 ) 
        {
            if ( ConsumeWriteBuf( nwritten ) )
            {
                //consumed += nwritten;
            }
        }

        if ( nwritten < i )
            osBufferFull = true;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDevice::Size()
{
    return (qlonglong)BytesAvailable();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDevice::At()
{
    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::At( qlonglong index )
{
    ReadBytes();

    if ( index > m_bufRead.size() )
        return false;

    // throw away data 0..index-1
    m_bufRead.consumeBytes( (qulonglong)index, nullptr );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::AtEnd()
{
    if ( !m_pSocket->isValid() )
        return true;

    ReadBytes();

    return m_bufRead.size() == 0;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qulonglong BufferedSocketDevice::BytesAvailable(void)
{
    if ( !m_pSocket->isValid() )
        return 0;

    return ReadBytes();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qulonglong BufferedSocketDevice::WaitForMore(
    std::chrono::milliseconds msecs, bool *pTimeout /* = nullptr*/ )
{
    bool bTimeout = false;

    if ( !m_pSocket->isValid() )
        return 0;

    qlonglong nBytes = BytesAvailable();

    if (nBytes == 0)
    {
/*
        The following code is a possible workaround to the lost request problem
        I just hate looping too much to put it in.  I believe there is something
        I'm missing that is causing the lost packets... Just need to find it.

        bTimeout      = true;
        int    nCount = 0;
        int    msWait = msecs / 100;
        
        while (((nBytes = ReadBytes()) == 0 ) && 
               (nCount++              < 100 ) &&
                bTimeout                      && 
                m_pSocket->isValid()         )
        {
            // give up control

            // should be some multiple of msWait.
            std::this_thread::sleep_for(1ms);

        }
    }
*/
        // -=>TODO: Override the timeout to 1 second... Closes connection sooner
        //          to help recover from lost requests.  (hack until better fix found)

        msecs  = 1s;

        nBytes = m_pSocket->waitForMore( msecs, &bTimeout );

        if (pTimeout != nullptr)
            *pTimeout = bTimeout;
    }
            
    return nBytes; // nBytes  //m_bufRead.size();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qulonglong BufferedSocketDevice::BytesToWrite(void) const
{
    return m_nWriteSize;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::ClearPendingData()
{
    while (!m_bufWrite.empty())
    {
        delete m_bufWrite.back();
        m_bufWrite.pop_back();
    }
    m_nWriteIndex = m_nWriteSize = 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::ClearReadBuffer()
{
    m_bufRead.clear();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDevice::ReadBlock( char *data, qulonglong maxlen )
{
    if ( data == nullptr && maxlen != 0 ) 
        return -1;

    if ( !m_pSocket->isOpen() ) 
        return -1;

    ReadBytes();

    if ( maxlen >= (qulonglong)m_bufRead.size() )
        maxlen = m_bufRead.size();

    m_bufRead.consumeBytes( maxlen, data );

    return maxlen;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDevice::WriteBlock( const char *data, qulonglong len )
{
    if ( len == 0 )
        return 0;

    QByteArray *a = m_bufWrite.back();

    bool writeNow = ( (m_nWriteSize + len >= 1400) || (len > 512) );

    if ( a && (a->size() + len < 128) ) 
    {
        // small buffer, resize
        int i = a->size();

        a->resize( i+len );
        memcpy( a->data()+i, data, len );
    } 
    else 
    {
        // append new buffer
        m_bufWrite.push_back(new QByteArray(data, len));
    }

    m_nWriteSize += len;

    if ( writeNow )
        Flush();

    return len;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDevice::WriteBlockDirect(
    const char *data, qulonglong len)
{
    qlonglong nWritten = 0;
        
    // must Flush data just in case caller is mixing buffered & un-buffered calls

    Flush();

    if (m_nDestPort != 0)
        nWritten = m_pSocket->writeBlock( data, len, m_destHostAddress , m_nDestPort );
    else
        nWritten = m_pSocket->writeBlock( data, len );

    return nWritten;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int BufferedSocketDevice::Getch()
{
    if ( m_pSocket->isOpen() )
    {
        ReadBytes();

        if (m_bufRead.size() > 0 ) 
        {
            uchar c = '\0';

            m_bufRead.consumeBytes( 1, (char*)&c );
        
            return c;
        }
    }

    return -1;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int BufferedSocketDevice::Putch( int ch )
{
    std::array <char,2> buf  { static_cast<char>(ch), 0 };

    return WriteBlock(buf.data(), 1) == 1 ? ch : -1;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int BufferedSocketDevice::Ungetch(int ch )
{
    return m_bufRead.ungetch( ch );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::CanReadLine()
{
    ReadBytes();

    return ( BytesAvailable() > 0 ) && m_bufRead.scanNewline( nullptr );
}
                               
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDevice::ReadLine()
{
    QByteArray a;
    a.resize(256);

    ReadBytes();

    bool nl = m_bufRead.scanNewline( &a );

    QString s;

    if ( nl ) 
    {
        At( a.size() );     // skips the data read

        s = QString( a );
    }

    return s;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDevice::ReadLine( std::chrono::milliseconds msecs )
{
    MythTimer timer;
    QString   sLine;

    if ( CanReadLine() )
        return( ReadLine() );
        
    // ----------------------------------------------------------------------
    // If the user supplied a timeout, lets loop until we can read a line 
    // or timeout.
    // ----------------------------------------------------------------------

    if ( msecs > 0ms)
    {
        bool bTimeout = false;

        timer.start();

        while ( !CanReadLine() && !bTimeout )
        {
#if 0
            LOG(VB_HTTP, LOG_DEBUG, "Can't Read Line... Waiting for more." );
#endif

            WaitForMore( msecs, &bTimeout );

            if ( timer.elapsed() >= msecs )
            {
                bTimeout = true;
                LOG(VB_HTTP, LOG_INFO, "Exceeded Total Elapsed Wait Time." );
            }
        }
            
        if (CanReadLine())
            sLine = ReadLine();
    }

    return( sLine );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

quint16 BufferedSocketDevice::Port(void) const
{
    if (m_pSocket)
        return( m_pSocket->port() );

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

quint16 BufferedSocketDevice::PeerPort(void) const
{
    if (m_pSocket)
        return( m_pSocket->peerPort() );

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QHostAddress BufferedSocketDevice::Address() const
{
    if (m_pSocket)
        return( m_pSocket->address() );

    QHostAddress tmp;

    return tmp;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QHostAddress BufferedSocketDevice::PeerAddress() const
{
    if (m_pSocket)
        return( m_pSocket->peerAddress() );

    QHostAddress tmp;

    return tmp;
}

