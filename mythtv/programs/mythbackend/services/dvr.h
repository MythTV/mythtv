//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.h
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DVR_H
#define DVR_H

#include <QScriptEngine>

#include "services/dvrServices.h"

class Dvr : public DvrServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Dvr( QObject *parent = 0 ) {}

    public:

        DTC::ProgramList* GetExpiringList     ( int              StartIndex, 
                                                int              Count      );

        DTC::ProgramList* GetRecordedList     ( bool             Descending,
                                                int              StartIndex,
                                                int              Count      );

        DTC::ProgramList* GetFilteredRecordedList ( bool             Descending,
                                                    int              StartIndex,
                                                    int              Count,
                                                    const QString   &TitleRegEx,
                                                    const QString   &RecGroup,
                                                    const QString   &StorageGroup );

        DTC::Program*     GetRecorded         ( int              ChanId,
                                                const QDateTime &StartTime  );

        bool              RemoveRecorded      ( int              ChanId,
                                                const QDateTime &StartTime  );

        DTC::ProgramList* GetConflictList     ( int              StartIndex,
                                                int              Count      );

        DTC::ProgramList* GetUpcomingList     ( int              StartIndex,
                                                int              Count,
                                                bool             ShowAll );

        DTC::EncoderList* GetEncoderList      ( );

        QStringList       GetRecGroupList     ( );

        QStringList       GetTitleList        ( );


        // Recording Rules

        int               AddRecordSchedule   ( int       ChanId,
                                                QDateTime StartTime,
                                                int       ParentId,
                                                bool      Inactive,
                                                uint      Season,
                                                uint      Episode,
                                                QString   Inetref,
                                                int       FindId,
                                                QString   Type,
                                                QString   SearchType,
                                                int       RecPriority,
                                                uint      PreferredInput,
                                                int       StartOffset,
                                                int       EndOffset,
                                                QString   DupMethod,
                                                QString   DupIn,
                                                uint      Filter,
                                                QString   RecProfile,
                                                QString   RecGroup,
                                                QString   StorageGroup,
                                                QString   PlayGroup,
                                                bool      AutoExpire,
                                                int       MaxEpisodes,
                                                bool      MaxNewest,
                                                bool      AutoCommflag,
                                                bool      AutoTranscode,
                                                bool      AutoMetaLookup,
                                                bool      AutoUserJob1,
                                                bool      AutoUserJob2,
                                                bool      AutoUserJob3,
                                                bool      AutoUserJob4,
                                                int       Transcoder);

        bool              RemoveRecordSchedule ( uint             RecordId   );

        DTC::RecRuleList* GetRecordScheduleList( int              StartIndex,
                                                 int              Count      );

        DTC::RecRule*     GetRecordSchedule    ( uint             RecordId   );

        bool              EnableRecordSchedule ( uint             RecordId   );

        bool              DisableRecordSchedule( uint             RecordId   );
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

class ScriptableDvr : public QObject
{
    Q_OBJECT

    private:

        Dvr    m_obj;

    public:
    
        Q_INVOKABLE ScriptableDvr( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetExpiringList      ( int              StartIndex, 
                                       int              Count      )
        {
            return m_obj.GetExpiringList( StartIndex, Count );
        }

        QObject* GetRecordedList     ( bool             Descending,
                                       int              StartIndex,
                                       int              Count      )
        {
            return m_obj.GetRecordedList( Descending, StartIndex, Count );
        }

        QObject* GetFilteredRecordedList ( bool             Descending,
                                           int              StartIndex,
                                           int              Count,
                                           const QString   &TitleRegEx,
                                           const QString   &RecGroup,
                                           const QString   &StorageGroup)
        {
            return m_obj.GetFilteredRecordedList( Descending, StartIndex, Count,
                                                  TitleRegEx, RecGroup,
                                                  StorageGroup);
        }

        QObject* GetRecorded         ( int              ChanId,
                                       const QDateTime &StartTime  )
        {
            return m_obj.GetRecorded( ChanId, StartTime );
        }

        QObject* GetConflictList    ( int              StartIndex,
                                       int              Count      )
        {
            return m_obj.GetConflictList( StartIndex, Count );
        }

        QObject* GetEncoderList     () { return m_obj.GetEncoderList(); }

        QStringList GetRecGroupList () { return m_obj.GetRecGroupList(); }

        QStringList GetTitleList    () { return m_obj.GetTitleList(); }

};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableDvr, QObject*);

#endif 
