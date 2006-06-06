//////////////////////////////////////////////////////////////////////////////
// Program Name: bufferedsocketdevice.cpp
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "bufferedsocketdevice.h"
#include "upnpglobal.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDevice::BufferedSocketDevice( int nSocket  )
{
    m_pSocket = new QSocketDevice();

    m_pSocket->setSocket( nSocket, QSocketDevice::Stream );
    m_pSocket->setBlocking( true );

    m_bufWrite.setAutoDelete( TRUE );

    m_nDestPort          = 0;

    m_nMaxReadBufferSize = 0; 
    m_nWriteSize         = 0;
    m_nWriteIndex        = 0;
    m_bHandleSocketDelete= true;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDevice::BufferedSocketDevice( QSocketDevice *pSocket /*= NULL*/ )
{

    m_bufWrite.setAutoDelete( TRUE );

    m_pSocket            = pSocket;

    m_nDestPort          = 0;

    m_nMaxReadBufferSize = 0; 
    m_nWriteSize         = 0;
    m_nWriteIndex        = 0;
    m_bHandleSocketDelete= false;

}

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
    m_bufWrite.clear();

    if (m_pSocket != NULL)
    {
        if (m_pSocket->isValid())
            m_pSocket->close();

        if (m_bHandleSocketDelete)
            delete m_pSocket;

        m_pSocket = NULL;
    }

}                      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QSocketDevice *BufferedSocketDevice::SocketDevice()
{
    return( m_pSocket );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetSocketDevice( QSocketDevice *pSocket )
{
    if ((m_bHandleSocketDelete) && (m_pSocket != NULL))
        delete m_pSocket;
    
    m_bHandleSocketDelete = false;

    m_pSocket = pSocket;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetDestAddress( QHostAddress hostAddress, Q_UINT16 nPort )
{
    m_DestHostAddress = hostAddress;
    m_nDestPort       = nPort;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::SetReadBufferSize( Q_ULONG bufSize )
{
    m_nMaxReadBufferSize = bufSize;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG BufferedSocketDevice::ReadBufferSize() const
{
    return m_nMaxReadBufferSize;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::ReadBytes()
{
    if (m_pSocket == NULL)
        return;

    Q_LONG maxToRead = 0;

    if ( m_nMaxReadBufferSize > 0 ) 
    {
        maxToRead = m_nMaxReadBufferSize - m_bufRead.size();

        if ( maxToRead <= 0 ) 
            return;
    }

    Q_LONG nbytes = m_pSocket->bytesAvailable();
    Q_LONG nread;

    QByteArray *a = 0;

    if ( nbytes > 0 )
    {
        a = new QByteArray( nbytes );

        nread = m_pSocket->readBlock( a->data(), maxToRead ? QMIN( nbytes, maxToRead ) : nbytes );

        if (( nread > 0 ) && ( nread != (int)a->size() ))
        {
            // unexpected
            a->resize( nread );
        }
    }

    if (a)
        m_bufRead.append( a );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::ConsumeWriteBuf( Q_ULONG nbytes )
{
    if ( nbytes <= 0 || nbytes > m_nWriteSize )
        return FALSE;

    m_nWriteSize -= nbytes;

    for ( ;; ) 
    {
        QByteArray *a = m_bufWrite.first();

        if ( m_nWriteIndex + nbytes >= a->size() ) 
        {
            nbytes -= a->size() - m_nWriteIndex;
            m_bufWrite.remove();

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

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::Flush()
{

    if ((m_pSocket == NULL) || !m_pSocket->isValid())
        return;

    bool osBufferFull = FALSE;
    int  consumed     = 0;

    while ( !osBufferFull && ( m_nWriteSize > 0 ))
    {
        QByteArray *a = m_bufWrite.first();

        int nwritten;
        int i = 0;

        if ( (int)a->size() - m_nWriteIndex < 1460 ) 
        {
            QByteArray out( 65536 );

            int j = m_nWriteIndex;
            int s = a->size() - j;

            while ( a && i+s < (int)out.size() ) 
            {
                memcpy( out.data()+i, a->data()+j, s );
                j = 0;
                i += s;
                a = m_bufWrite.next();
                s = a ? a->size() : 0;
            }

            if (m_nDestPort != 0)
                nwritten = m_pSocket->writeBlock( out.data(), i, m_DestHostAddress, m_nDestPort );
            else
                nwritten = m_pSocket->writeBlock( out.data(), i );
        } 
        else 
        {
            // Big block, write it immediately
            i = a->size() - m_nWriteIndex;

            if (m_nDestPort != 0)
                nwritten = m_pSocket->writeBlock( a->data() + m_nWriteIndex, i, m_DestHostAddress, m_nDestPort );
            else
                nwritten = m_pSocket->writeBlock( a->data() + m_nWriteIndex, i );
        }

        if ( nwritten > 0 ) 
        {
            if ( ConsumeWriteBuf( nwritten ) )

            consumed += nwritten;
        }

        if ( nwritten < i )
            osBufferFull = TRUE;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QIODevice::Offset BufferedSocketDevice::Size() 
{
    return (QIODevice::Offset)BytesAvailable();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QIODevice::Offset BufferedSocketDevice::At()
{
    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::At( QIODevice::Offset index )
{
    ReadBytes();

    if ( index > m_bufRead.size() )
        return FALSE;

    m_bufRead.consumeBytes( (Q_ULONG)index, 0 );    // throw away data 0..index-1

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDevice::AtEnd()
{
    if ( !m_pSocket->isValid() )
        return TRUE;

    ReadBytes();

    return m_bufRead.size() == 0;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG BufferedSocketDevice::BytesAvailable()  
{
    if ( !m_pSocket->isValid() )
        return 0;

    ReadBytes();

    return m_bufRead.size();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG BufferedSocketDevice::WaitForMore( int msecs, bool *pTimeout /* = NULL*/ ) 
{
    if ( !m_pSocket->isValid() )
        return 0;

    Q_LONG nBytes = BytesAvailable();

    if (nBytes == 0)
    {
        if ( pTimeout == NULL )
            nBytes = m_pSocket->waitForMore( msecs );
        else
            nBytes = m_pSocket->waitForMore( msecs, pTimeout );

        if ( nBytes > 0 )
            ReadBytes();
    }

    return BytesAvailable();         //m_bufRead.size();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG BufferedSocketDevice::BytesToWrite() const
{
    return m_nWriteSize;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDevice::ClearPendingData()
{
    m_bufWrite.clear();
    m_nWriteIndex = m_nWriteSize = 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG BufferedSocketDevice::ReadBlock( char *data, Q_ULONG maxlen )
{
    if ( data == 0 && maxlen != 0 ) 
        return -1;

    if ( !m_pSocket->isOpen() ) 
        return -1;

    ReadBytes();

    if ( maxlen >= m_bufRead.size() )
        maxlen = m_bufRead.size();

    m_bufRead.consumeBytes( maxlen, data );

    return maxlen;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG BufferedSocketDevice::WriteBlock( const char *data, Q_ULONG len )
{
    if ( len == 0 )
        return 0;

    QByteArray *a = m_bufWrite.last();

    bool writeNow = ( m_nWriteSize + len >= 1400 || len > 512 );

    if ( a && a->size() + len < 128 ) 
    {
        // small buffer, resize
        int i = a->size();

        a->resize( i+len );
        memcpy( a->data()+i, data, len );
    } 
    else 
    {
        // append new buffer
        a = new QByteArray( len );
        memcpy( a->data(), data, len );

        m_bufWrite.append( a );
    }

    m_nWriteSize += len;

    if ( writeNow )
        Flush();

    return len;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG BufferedSocketDevice::WriteBlockDirect( const char *data, Q_ULONG len )
{
    Q_LONG nWritten = 0;
        
    // must Flush data just in case caller is mixing buffered & un-buffered calls

    Flush();

    if (m_nDestPort != 0)
        nWritten = m_pSocket->writeBlock( data, len, m_DestHostAddress, m_nDestPort );
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
            uchar c;

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
    char buf[2];

    buf[0] = ch;

    return WriteBlock(buf, 1) == 1 ? ch : -1;
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

    if (( BytesAvailable() > 0 ) && m_bufRead.scanNewline( 0 ) )
        return TRUE;

    return FALSE;
}
                               
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDevice::ReadLine()
{
    QByteArray a(256);

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

Q_UINT16 BufferedSocketDevice::Port() const
{
    if (m_pSocket)
        return( m_pSocket->port() );

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_UINT16 BufferedSocketDevice::PeerPort() const
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

