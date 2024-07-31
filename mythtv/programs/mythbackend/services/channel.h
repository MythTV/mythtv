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

#include "libmythbase/mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif

#include "libmythservicecontracts/services/channelServices.h"

class Channel : public ChannelServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit Channel( QObject */*parent*/ = nullptr ) {}

    public:

        /* Channel Methods */

        DTC::ChannelInfoList*  GetChannelInfoList  ( uint      SourceID,
                                                     uint      ChannelGroupID,
                                                     uint      StartIndex,
                                                     uint      Count,
                                                     bool      OnlyVisible,
                                                     bool      Details,
                                                     bool      OrderByName,
                                                     bool      GroupByCallsign,
                                                     bool      OnlyTunable ) override; // ChannelServices

        DTC::ChannelInfo*      GetChannelInfo      ( uint     ChanID     ) override; // ChannelServices

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
                                                     bool          Visible,
                                                     const QString &ExtendedVisible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority,
                                                     uint          ServiceType ) override; // ChannelServices

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
                                                     bool          Visible,
                                                     const QString &ExtendedVisible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority,
                                                     uint          ServiceType ) override; // ChannelServices

        bool                   RemoveDBChannel     ( uint          ChannelID ) override; // ChannelServices

        /* Video Source Methods */

        DTC::VideoSourceList*     GetVideoSourceList     ( void ) override; // ChannelServices

        DTC::VideoSource*         GetVideoSource         ( uint SourceID ) override; // ChannelServices

        bool                      UpdateVideoSource      ( uint          SourceID,
                                                           const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId,
                                                           uint          BouquetId,
                                                           uint          RegionId,
                                                           uint          ScanFrequency,
                                                           uint          LCNOffset ) override; // ChannelServices

        int                       AddVideoSource         ( const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId,
                                                           uint          BouquetId,
                                                           uint          RegionId,
                                                           uint          ScanFrequency,
                                                           uint          LCNOffset ) override; // ChannelServices

        bool                      RemoveVideoSource      ( uint SourceID ) override; // ChannelServices

        DTC::LineupList*          GetDDLineupList        ( const QString &/*Source*/,
                                                           const QString &/*UserId*/,
                                                           const QString &/*Password*/ ) override; // ChannelServices

        int                       FetchChannelsFromSource( uint       SourceId,
                                                           uint       CardId,
                                                           bool       WaitForFinish ) override; // ChannelServices

        /* Multiplex Methods */

        DTC::VideoMultiplexList*  GetVideoMultiplexList  ( uint SourceID,
                                                           uint StartIndex,
                                                           uint Count      ) override; // ChannelServices

        DTC::VideoMultiplex*      GetVideoMultiplex      ( uint MplexID    ) override; // ChannelServices

        QStringList               GetXMLTVIdList         ( uint SourceID ) override; // ChannelServices

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

#if CONFIG_QTSCRIPT
class ScriptableChannel : public QObject
{
    Q_OBJECT

    private:

        Channel        m_obj;
        QScriptEngine *m_pEngine;

    public:

        Q_INVOKABLE explicit ScriptableChannel( QScriptEngine *pEngine, QObject *parent = nullptr )
          : QObject( parent ), m_pEngine(pEngine)
        {
        }

    public slots:

        QObject* GetChannelInfoList  ( int      SourceID = 0,
                                       int      ChannelGroupID = 0,
                                       int      StartIndex = 0,
                                       int      Count = 0,
                                       bool     OnlyVisible = false,
                                       bool     Details = false,
                                       bool     OrderByName = false,
                                       bool     GroupByCallsign = false,
                                       bool     OnlyTunable = false
                                     )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetChannelInfoList( SourceID, ChannelGroupID, StartIndex, Count, OnlyVisible, Details, OrderByName, GroupByCallsign, OnlyTunable );
            )
        }

        QObject* GetChannelInfo      ( int      ChanID     )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetChannelInfo( ChanID );
            )
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
                                   bool          Visible,
                                   const QString &ExtendedVisible,
                                   const QString &FrequencyID,
                                   const QString &Icon,
                                   const QString &Format,
                                   const QString &XMLTVID,
                                   const QString &DefaultAuthority,
                                   uint          ServiceType )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.UpdateDBChannel(MplexID, SourceID, ChannelID,
                                         CallSign, ChannelName, ChannelNumber,
                                         ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                                         UseEIT, Visible, ExtendedVisible, FrequencyID, Icon, Format,
                                         XMLTVID, DefaultAuthority, ServiceType);
            )
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
                                   bool          Visible,
                                   const QString &ExtendedVisible,
                                   const QString &FrequencyID,
                                   const QString &Icon,
                                   const QString &Format,
                                   const QString &XMLTVID,
                                   const QString &DefaultAuthority,
                                   uint          ServiceType )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.AddDBChannel(MplexID, SourceID, ChannelID,
                                      CallSign, ChannelName, ChannelNumber,
                                      ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                                      UseEIT, Visible, ExtendedVisible, FrequencyID, Icon, Format,
                                      XMLTVID, DefaultAuthority, ServiceType);
            )
        }

        bool RemoveDBChannel     ( uint          ChannelID )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveDBChannel(ChannelID);
            )
        }

        QObject* GetVideoSourceList ( void )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoSourceList();
            )
        }

        QObject* GetVideoSource ( uint SourceID )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoSource( SourceID );
            )
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
                                 int           NITId,
                                 uint          BouquetId,
                                 uint          RegionId,
                                 uint          ScanFrequency,
                                 uint          LCNOffset )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.UpdateVideoSource( SourceID, SourceName, Grabber,
                                            UserId, FreqTable, LineupId, Password,
                                            UseEIT, ConfigPath, NITId, BouquetId, RegionId,
                                            ScanFrequency, LCNOffset );
            )
        }

        bool AddVideoSource    ( const QString &SourceName,
                                 const QString &Grabber,
                                 const QString &UserId,
                                 const QString &FreqTable,
                                 const QString &LineupId,
                                 const QString &Password,
                                 bool          UseEIT,
                                 const QString &ConfigPath,
                                 int           NITId,
                                 uint          BouquetId,
                                 uint          RegionId,
                                 uint          ScanFrequency,
                                 uint          LCNOffset )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.AddVideoSource( SourceName, Grabber, UserId,
                                         FreqTable, LineupId, Password,
                                         UseEIT, ConfigPath, NITId, BouquetId, RegionId,
                                         ScanFrequency, LCNOffset );
            )
        }

        bool RemoveVideoSource ( uint SourceID )
        {
            SCRIPT_CATCH_EXCEPTION( false,
                return m_obj.RemoveVideoSource( SourceID );
            )
        }

        QObject* GetVideoMultiplexList  ( int      SourceID,
                                          int      StartIndex,
                                          int      Count      )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoMultiplexList( SourceID, StartIndex, Count );
            )
        }

        QObject* GetVideoMultiplex  ( int      MplexID )
        {
            SCRIPT_CATCH_EXCEPTION( nullptr,
                return m_obj.GetVideoMultiplex( MplexID );
            )
        }

        QStringList GetXMLTVIdList ( int SourceID )
        {
            SCRIPT_CATCH_EXCEPTION( QStringList(),
                return m_obj.GetXMLTVIdList(SourceID);
            )
        }
};


// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV( ScriptableChannel, QObject*);
#endif

#endif
