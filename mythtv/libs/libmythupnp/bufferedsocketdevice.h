//////////////////////////////////////////////////////////////////////////////
// Program Name: bufferedsocketdevice.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __BUFFEREDSOCKETDEVICE_H__
#define __BUFFEREDSOCKETDEVICE_H__

// Qt headers
#include <Q3PtrList>
#include <Q3SocketDevice>


#include <q3socketdevice.h>
//#include <q3membuf_p.h>

// MythTV headers
#include "compat.h"

// file q3membuf_p.h not installed on my system using ubuntu Qt4 Dev Package.
// Including class here.

class Q_COMPAT_EXPORT Q3Membuf
{
public:
    Q3Membuf();
    ~Q3Membuf();

    void append(QByteArray *ba);
    void clear();

    bool consumeBytes(Q_ULONG nbytes, char *sink);
    QByteArray readAll();
    bool scanNewline(QByteArray *store);
    bool canReadLine() const;

    int ungetch(int ch);

    qint64 size() const;

private:

    QList<QByteArray *> buf;
    qint64 _size;
    qint64 _index;
};

inline void Q3Membuf::append(QByteArray *ba)
{ buf.append(ba); _size += ba->size(); }

inline void Q3Membuf::clear()
{ qDeleteAll(buf); buf.clear(); _size=0; _index=0; }

inline QByteArray Q3Membuf::readAll()
{ QByteArray ba; ba.resize(_size); consumeBytes(_size,ba.data()); return ba; }

inline bool Q3Membuf::canReadLine() const
{ return const_cast<Q3Membuf*>(this)->scanNewline(0); }

inline qint64 Q3Membuf::size() const
{ return _size; }

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class BufferedSocketDevice 
{
    protected:

        Q3SocketDevice          *m_pSocket;

        Q_ULONG                 m_nMaxReadBufferSize; 
        QIODevice::Offset       m_nWriteSize;           // write total buf size
        QIODevice::Offset       m_nWriteIndex;          // write index

        bool                    m_bHandleSocketDelete;

        QHostAddress            m_DestHostAddress;
        Q_UINT16                m_nDestPort;

        Q3Membuf                m_bufRead;
        Q3PtrList<QByteArray>   m_bufWrite;

        int     ReadBytes      ( );
        bool    ConsumeWriteBuf( Q_ULONG nbytes );

    public:

        BufferedSocketDevice( int nSocket );
        BufferedSocketDevice( Q3SocketDevice *pSocket = NULL,
                              bool    bTakeOwnership = false );

        virtual ~BufferedSocketDevice( );

        Q3SocketDevice      *SocketDevice        ();
        void                SetSocketDevice     ( Q3SocketDevice *pSocket );

        void                SetDestAddress      ( QHostAddress hostAddress,
                                                  Q_UINT16     nPort );

        bool                Connect             ( const QHostAddress & addr, Q_UINT16 port );
        void                Close               ();
        void                Flush               ();
        QIODevice::Offset   Size                ();
        QIODevice::Offset   At                  (); 
        bool                At                  ( qlonglong );
        bool                AtEnd               ();

        Q_ULONG             BytesAvailable      (); 
        Q_ULONG             WaitForMore         ( int msecs, bool *timeout = NULL );

        Q_ULONG             BytesToWrite        () const;
        void                ClearPendingData    ();
        void                ClearReadBuffer     ();

        Q_LONG              ReadBlock           ( char *data, Q_ULONG maxlen );
        Q_LONG              WriteBlock          ( const char *data, Q_ULONG len );
        Q_LONG              WriteBlockDirect    ( const char *data, Q_ULONG len );

        int                 Getch               ();
        int                 Putch               ( int );
        int                 Ungetch             (int);

        bool                CanReadLine         ();
        QString             ReadLine            ();
        QString             ReadLine            ( int msecs );
        Q_LONG              ReadLine            ( char *data, Q_ULONG maxlen );

        Q_UINT16            Port                () const;
        Q_UINT16            PeerPort            () const;
        QHostAddress        Address             () const;
        QHostAddress        PeerAddress         () const;

        void                SetReadBufferSize   ( Q_ULONG );
        Q_ULONG             ReadBufferSize      () const;

        bool                IsValid             () { return( ( m_pSocket ) ? m_pSocket->isValid() : false ); }
        int                 socket              () { return( ( m_pSocket ) ? m_pSocket->socket() : 0 ); }
};

#endif
