//////////////////////////////////////////////////////////////////////////////
// Program Name: bufferedsocketdevice.h
// Created     : Oct. 1, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __BUFFEREDSOCKETDEVICE_H__
#define __BUFFEREDSOCKETDEVICE_H__

// C++ headers
#include <deque>
using namespace std;

// Qt headers
#include <QString>
#include <QByteArray>
#include <QHostAddress>

// MythTV headers
#include "mmembuf.h"
#include "msocketdevice.h"
#include "compat.h"

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

        MSocketDevice          *m_pSocket;

        qulonglong              m_nMaxReadBufferSize; 
        qint64                  m_nWriteSize;          ///< write total buf size
        qint64                  m_nWriteIndex;         ///< write index

        bool                    m_bHandleSocketDelete;

        QHostAddress            m_DestHostAddress;
        quint16                 m_nDestPort;

        MMembuf                 m_bufRead;
        deque<QByteArray*>      m_bufWrite;

        int     ReadBytes      ( );
        bool    ConsumeWriteBuf( qulonglong nbytes );

    public:

        BufferedSocketDevice( int nSocket );
        explicit BufferedSocketDevice( MSocketDevice *pSocket = NULL,
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
        qint64              At                  () const; 
        bool                At                  ( qlonglong );
        bool                AtEnd               ();

        qulonglong          BytesAvailable      (); 
        qulonglong          WaitForMore         ( int msecs,
                                                  bool *timeout = NULL );

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
        int                 Putch               ( int );
        int                 Ungetch             (int);

        bool                CanReadLine         ();
        QString             ReadLine            ();
        QString             ReadLine            ( int msecs );
        qlonglong           ReadLine            ( char *data,
                                                  qulonglong maxlen );

        quint16             Port                () const;
        quint16             PeerPort            () const;
        QHostAddress        Address             () const;
        QHostAddress        PeerAddress         () const;

        void                SetReadBufferSize   ( qulonglong );
        qulonglong             ReadBufferSize      () const;

        bool                IsValid             () { return( ( m_pSocket ) ? m_pSocket->isValid() : false ); }
        int                 socket              () { return( ( m_pSocket ) ? m_pSocket->socket() : 0 ); }
};

#endif
