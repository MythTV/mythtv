//////////////////////////////////////////////////////////////////////////////
// Program Name: bufferedsocketdevice.h
// Created     : Oct. 1, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BUFFEREDSOCKETDEVICE_H
#define BUFFEREDSOCKETDEVICE_H

// C++ headers
#include <chrono>
#include <deque>

// Qt headers
#include <QString>
#include <QByteArray>
#include <QHostAddress>

// MythTV headers
#include "libmythbase/compat.h"

#include "mmembuf.h"
#include "msocketdevice.h"

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

        MSocketDevice          *m_pSocket             {nullptr};

        qulonglong              m_nMaxReadBufferSize  {0};
        qint64                  m_nWriteSize          {0}; ///< write total buf size
        qint64                  m_nWriteIndex         {0}; ///< write index

        bool                    m_bHandleSocketDelete {true};

        QHostAddress            m_destHostAddress;
        quint16                 m_nDestPort           {0};

        MMembuf                 m_bufRead;
        std::deque<QByteArray*> m_bufWrite;

        int     ReadBytes      ( );
        bool    ConsumeWriteBuf( qulonglong nbytes );

    public:

        explicit BufferedSocketDevice( int nSocket );
        explicit BufferedSocketDevice( MSocketDevice *pSocket = nullptr,
                                       bool bTakeOwnership    = false );

        virtual ~BufferedSocketDevice( );

        MSocketDevice      *SocketDevice        ();
        void                SetSocketDevice     ( MSocketDevice *pSocket );

        void                SetDestAddress      ( QHostAddress hostAddress,
                                                  quint16      nPort );

        bool                Connect             ( const QHostAddress &addr,
                                                  quint16             port );
        void                Close               ();
        void                Flush               ();
        qint64              Size                ();
        static qint64       At                  () ; 
        bool                At                  ( qlonglong index );
        bool                AtEnd               ();

        qulonglong          BytesAvailable      (); 
        qulonglong          WaitForMore         ( std::chrono::milliseconds msecs,
                                                  bool *timeout = nullptr );

        qulonglong          BytesToWrite        () const;
        void                ClearPendingData    ();
        void                ClearReadBuffer     ();

        qlonglong           ReadBlock           ( char *data,
                                                  qulonglong maxlen );
        qlonglong           WriteBlock          ( const char *data,
                                                  qulonglong len );
        qlonglong           WriteBlockDirect    ( const char *data,
                                                  qulonglong len );

        int                 Getch               ();
        int                 Putch               ( int ch );
        int                 Ungetch             ( int ch );

        bool                CanReadLine         ();
        QString             ReadLine            ();
        QString             ReadLine            ( std::chrono::milliseconds msecs );
        qlonglong           ReadLine            ( char *data,
                                                  qulonglong maxlen );

        quint16             Port                () const;
        quint16             PeerPort            () const;
        QHostAddress        Address             () const;
        QHostAddress        PeerAddress         () const;

        void                SetReadBufferSize   ( qulonglong bufSize );
        qulonglong             ReadBufferSize      () const;

        bool                IsValid             () { return( ( m_pSocket ) ? m_pSocket->isValid() : false ); }
        int                 socket              () { return( ( m_pSocket ) ? m_pSocket->socket() : 0 ); }
};

#endif // BUFFEREDSOCKETDEVICE_H
