//////////////////////////////////////////////////////////////////////////////
// Program Name: channel.h
// Created     : Apr. 8, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CHANNEL_H
#define CHANNEL_H

#include <QScriptEngine>

#include "services/channelServices.h"

class Channel : public ChannelServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Channel( QObject *parent = 0 ) {}

    public:

        /* Channel Methods */

        DTC::ChannelInfoList*  GetChannelInfoList  ( uint      SourceID,
                                                     uint      StartIndex,
                                                     uint      Count      );

        DTC::ChannelInfo*      GetChannelInfo      ( uint     ChanID     );

        bool                   UpdateDBChannel     ( uint          MplexID,
                                                     uint          SourceID,
                                                     uint          ChannelID,
                                                     const QString &CallSign,
                                                     const QString &ChannelName,
                                                     const QString &ChannelNumber,
                                                     uint          ServiceID,
                                                     uint          ATSCMajorChannel,
                                                     uint          ATSCMinorChannel,
                                                     bool          UseEIT,
                                                     bool          visible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority );

        bool                   AddDBChannel        ( uint          MplexID,
                                                     uint          SourceID,
                                                     uint          ChannelID,
                                                     const QString &CallSign,
                                                     const QString &ChannelName,
                                                     const QString &ChannelNumber,
                                                     uint          ServiceID,
                                                     uint          ATSCMajorChannel,
                                                     uint          ATSCMinorChannel,
                                                     bool          UseEIT,
                                                     bool          visible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority );

        bool                   RemoveDBChannel     ( uint          ChannelID );

        /* Video Source Methods */

        DTC::VideoSourceList*     GetVideoSourceList     ( void );

        DTC::VideoSource*         GetVideoSource         ( uint SourceID );

        bool                      UpdateVideoSource      ( uint          SourceID,
                                                           const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId );

        int                       AddVideoSource         ( const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId );

        bool                      RemoveVideoSource      ( uint SourceID );

        DTC::LineupList*          GetDDLineupList        ( const QString &Source,
                                                           const QString &UserId,
                                                           const QString &Password );

        int                       FetchChannelsFromSource( const uint SourceId,
                                                           const uint CardId,
                                                           bool       WaitForFinish );

        /* Multiplex Methods */

        DTC::VideoMultiplexList*  GetVideoMultiplexList  ( uint SourceID,
                                                           uint StartIndex,
                                                           uint Count      );

        DTC::VideoMultiplex*      GetVideoMultiplex      ( uint MplexID    );

        QStringList               GetXMLTVIdList         ( uint SourceID );

};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

class ScriptableChannel : public QObject
{
    Q_OBJECT

    private:

        Channel    m_obj;

    public:

        Q_INVOKABLE ScriptableChannel( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetChannelInfoList  ( int      SourceID,
                                       int      StartIndex,
                                       int      Count      )
        {
            return m_obj.GetChannelInfoList( SourceID, StartIndex, Count );
        }

        QObject* GetChannelInfo      ( int      ChanID     )
        {
            return m_obj.GetChannelInfo( ChanID );
        }

        bool UpdateDBChannel     ( uint          MplexID,
                                   uint          SourceID,
                                   uint          ChannelID,
                                   const QString &CallSign,
                                   const QString &ChannelName,
                                   const QString &ChannelNumber,
                                   uint          ServiceID,
                                   uint          ATSCMajorChannel,
                                   uint          ATSCMinorChannel,
                                   bool          UseEIT,
                                   bool          visible,
                                   const QString &FrequencyID,
                                   const QString &Icon,
                                   const QString &Format,
                                   const QString &XMLTVID,
                                   const QString &DefaultAuthority )
        {
            return m_obj.UpdateDBChannel(MplexID, SourceID, ChannelID,
                                         CallSign, ChannelName, ChannelNumber,
                                         ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                                         UseEIT, visible, FrequencyID, Icon, Format,
                                         XMLTVID, DefaultAuthority);
        }

        bool AddDBChannel        ( uint          MplexID,
                                   uint          SourceID,
                                   uint          ChannelID,
                                   const QString &CallSign,
                                   const QString &ChannelName,
                                   const QString &ChannelNumber,
                                   uint          ServiceID,
                                   uint          ATSCMajorChannel,
                                   uint          ATSCMinorChannel,
                                   bool          UseEIT,
                                   bool          visible,
                                   const QString &FrequencyID,
                                   const QString &Icon,
                                   const QString &Format,
                                   const QString &XMLTVID,
                                   const QString &DefaultAuthority )
        {
            return m_obj.AddDBChannel(MplexID, SourceID, ChannelID,
                                      CallSign, ChannelName, ChannelNumber,
                                      ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                                      UseEIT, visible, FrequencyID, Icon, Format,
                                      XMLTVID, DefaultAuthority);
        }

        bool RemoveDBChannel     ( uint          ChannelID )
        {
            return m_obj.RemoveDBChannel(ChannelID);
        }

        QObject* GetVideoSourceList ( void )
        {
            return m_obj.GetVideoSourceList();
        }

        QObject* GetVideoSource ( uint SourceID )
        {
            return m_obj.GetVideoSource( SourceID );
        }

        bool UpdateVideoSource ( uint          SourceID,
                                 const QString &SourceName,
                                 const QString &Grabber,
                                 const QString &UserId,
                                 const QString &FreqTable,
                                 const QString &LineupId,
                                 const QString &Password,
                                 bool          UseEIT,
                                 const QString &ConfigPath,
                                 int           NITId )
        {
            return m_obj.UpdateVideoSource( SourceID, SourceName, Grabber,
                                            UserId, FreqTable, LineupId, Password,
                                            UseEIT, ConfigPath, NITId );
        }

        bool AddVideoSource    ( const QString &SourceName,
                                 const QString &Grabber,
                                 const QString &UserId,
                                 const QString &FreqTable,
                                 const QString &LineupId,
                                 const QString &Password,
                                 bool          UseEIT,
                                 const QString &ConfigPath,
                                 int           NITId )
        {
            return m_obj.AddVideoSource( SourceName, Grabber, UserId,
                                         FreqTable, LineupId, Password,
                                         UseEIT, ConfigPath, NITId );
        }

        bool RemoveVideoSource ( uint SourceID )
        {
            return m_obj.RemoveVideoSource( SourceID );
        }

        QObject* GetVideoMultiplexList  ( int      SourceID,
                                          int      StartIndex,
                                          int      Count      )
        {
            return m_obj.GetVideoMultiplexList( SourceID, StartIndex, Count );
        }

        QObject* GetVideoMultiplex  ( int      MplexID )
        {
            return m_obj.GetVideoMultiplex( MplexID );
        }

        QStringList GetXMLTVIdList ( int SourceID )
        {
            return m_obj.GetXMLTVIdList(SourceID);
        }
};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableChannel, QObject*);

#endif
