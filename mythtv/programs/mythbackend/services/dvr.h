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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
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
                                                int              Count,
                                                const QString   &TitleRegEx,
                                                const QString   &RecGroup,
                                                const QString   &StorageGroup );

        DTC::Program*     GetRecorded         ( int              ChanId,
                                                const QDateTime &StartTime  );

        bool              RemoveRecorded      ( int              ChanId,
                                                const QDateTime &StartTime,
                                                bool             ForceDelete,
                                                bool             AllowRerecord  );

        bool              DeleteRecording     ( int              ChanId,
                                                const QDateTime &StartTime,
                                                bool             ForceDelete,
                                                bool             AllowRerecord  );

        bool              UnDeleteRecording   ( int              ChanId,
                                                const QDateTime &StartTime );

        DTC::ProgramList* GetConflictList     ( int              StartIndex,
                                                int              Count      );

        DTC::ProgramList* GetUpcomingList     ( int              StartIndex,
                                                int              Count,
                                                bool             ShowAll );

        DTC::EncoderList* GetEncoderList      ( );

        DTC::InputList*   GetInputList        ( );

        QStringList       GetRecGroupList     ( );

        QStringList       GetRecStorageGroupList ( );

        QStringList       GetPlayGroupList    ( );

        DTC::RecRuleFilterList* GetRecRuleFilterList ( );

        QStringList       GetTitleList        ( const QString   &RecGroup );

        DTC::TitleInfoList* GetTitleInfoList  ( );

        // Recording Rules

        uint              AddRecordSchedule   ( QString   Title,
                                                QString   Subtitle,
                                                QString   Description,
                                                QString   Category,
                                                QDateTime StartTime,
                                                QDateTime EndTime,
                                                QString   SeriesId,
                                                QString   ProgramId,
                                                int       ChanId,
                                                QString   Station,
                                                int       FindDay,
                                                QTime     FindTime,
                                                int       ParentId,
                                                bool      Inactive,
                                                uint      Season,
                                                uint      Episode,
                                                QString   Inetref,
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

        bool               UpdateRecordSchedule ( uint    RecordId,
                                                  QString   Title,
                                                  QString   Subtitle,
                                                  QString   Description,
                                                  QString   Category,
                                                  QDateTime StartTime,
                                                  QDateTime EndTime,
                                                  QString   SeriesId,
                                                  QString   ProgramId,
                                                  int       ChanId,
                                                  QString   Station,
                                                  int       FindDay,
                                                  QTime     FindTime,
                                                  bool      Inactive,
                                                  uint      Season,
                                                  uint      Episode,
                                                  QString   Inetref,
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

        bool              AddDontRecordSchedule( int              ChanId,
                                                 const QDateTime &StartTime,
                                                 bool             NeverRecord );

        DTC::RecRuleList* GetRecordScheduleList( int              StartIndex,
                                                 int              Count      );

        DTC::RecRule*     GetRecordSchedule    ( uint             RecordId,
                                                 QString          Template,
                                                 int              ChanId,
                                                 QDateTime        StartTime,
                                                 bool             MakeOverride );

        bool              EnableRecordSchedule ( uint             RecordId   );

        bool              DisableRecordSchedule( uint             RecordId   );

        QString           RecStatusToString    ( int              RecStatus );

        QString           RecStatusToDescription ( int            RecStatus,
                                                   int            RecType,
                                                   const QDateTime &StartTime );

        QString           RecTypeToString      ( QString          RecType   );

        QString           RecTypeToDescription ( QString          RecType   );

        QString           DupMethodToString    ( QString          DupMethod );

        QString           DupMethodToDescription ( QString        DupMethod );

        QString           DupInToString        ( QString          DupIn     );

        QString           DupInToDescription   ( QString          DupIn     );
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
                                       int              Count,
                                       const QString   &TitleRegEx,
                                       const QString   &RecGroup,
                                       const QString   &StorageGroup)
        {
            return m_obj.GetRecordedList( Descending, StartIndex, Count,
                                          TitleRegEx, RecGroup,
                                          StorageGroup);
        }

        QObject* GetRecorded         ( int              ChanId,
                                       const QDateTime &StartTime  )
        {
            return m_obj.GetRecorded( ChanId, StartTime );
        }

        bool RemoveRecorded           ( int              ChanId,
                                        const QDateTime &StartTime,
                                        bool             ForceDelete,
                                        bool             AllowRerecord )
        {
            return m_obj.RemoveRecorded( ChanId, StartTime,
                                         ForceDelete, AllowRerecord);
        }

        bool DeleteRecording          ( int              ChanId,
                                        const QDateTime &StartTime,
                                        bool             ForceDelete,
                                        bool             AllowRerecord  )
        {
            return m_obj.DeleteRecording(ChanId, StartTime,
                                         ForceDelete, AllowRerecord);
        }

        bool UnDeleteRecording        ( int              ChanId,
                                        const QDateTime &StartTime )
        {
            return m_obj.UnDeleteRecording(ChanId, StartTime);
        }

        QObject* GetConflictList    ( int              StartIndex,
                                      int              Count      )
        {
            return m_obj.GetConflictList( StartIndex, Count );
        }

        QObject* GetUpcomingList    ( int              StartIndex,
                                      int              Count,
                                      bool             ShowAll )
        {
            return m_obj.GetUpcomingList( StartIndex, Count, ShowAll );
        }

        QObject* GetEncoderList     () { return m_obj.GetEncoderList(); }

        QObject* GetInputList       () { return m_obj.GetInputList();   }

        QStringList GetRecGroupList        () { return m_obj.GetRecGroupList(); }

        QStringList GetRecStorageGroupList () { return m_obj.GetRecStorageGroupList(); }

        QStringList GetPlayGroupList       () { return m_obj.GetPlayGroupList(); }

        QObject* GetRecRuleFilterList      () { return m_obj.GetRecRuleFilterList(); }

        QStringList GetTitleList    ( const QString   &RecGroup )
        {
            return m_obj.GetTitleList( RecGroup );
        }

        QObject* GetTitleInfoList   () { return m_obj.GetTitleInfoList(); }

        uint AddRecordSchedule ( DTC::RecRule *rule )
        {
            return m_obj.AddRecordSchedule(
                                    rule->Title(),          rule->SubTitle(),
                                    rule->Description(),    rule->Category(),
                                    rule->StartTime(),      rule->EndTime(),
                                    rule->SeriesId(),       rule->ProgramId(),
                                    rule->ChanId(),         rule->CallSign(),
                                    rule->FindDay(),        rule->FindTime(),
                                    rule->ParentId(),       rule->Inactive(),
                                    rule->Season(),         rule->Episode(),
                                    rule->Inetref(),        rule->Type(),
                                    rule->SearchType(),     rule->RecPriority(),
                                    rule->PreferredInput(), rule->StartOffset(),
                                    rule->EndOffset(),      rule->DupMethod(),
                                    rule->DupIn(),          rule->Filter(),
                                    rule->RecProfile(),     rule->RecGroup(),
                                    rule->StorageGroup(),   rule->PlayGroup(),
                                    rule->AutoExpire(),     rule->MaxEpisodes(),
                                    rule->MaxNewest(),      rule->AutoCommflag(),
                                    rule->AutoTranscode(),  rule->AutoMetaLookup(),
                                    rule->AutoUserJob1(),   rule->AutoUserJob2(),
                                    rule->AutoUserJob3(),   rule->AutoUserJob4(),
                                    rule->Transcoder());
        }

        bool UpdateRecordSchedule ( DTC::RecRule *rule )
        {
            if (rule->Id() <= 0)
                throw QString("Record ID cannot be <= zero");

            return m_obj.UpdateRecordSchedule(
                                    static_cast<uint>(rule->Id()),
                                    rule->Title(),          rule->SubTitle(),
                                    rule->Description(),    rule->Category(),
                                    rule->StartTime(),      rule->EndTime(),
                                    rule->SeriesId(),       rule->ProgramId(),
                                    rule->ChanId(),         rule->CallSign(),
                                    rule->FindDay(),        rule->FindTime(),
                                    rule->Inactive(),
                                    rule->Season(),         rule->Episode(),
                                    rule->Inetref(),        rule->Type(),
                                    rule->SearchType(),     rule->RecPriority(),
                                    rule->PreferredInput(), rule->StartOffset(),
                                    rule->EndOffset(),      rule->DupMethod(),
                                    rule->DupIn(),          rule->Filter(),
                                    rule->RecProfile(),     rule->RecGroup(),
                                    rule->StorageGroup(),   rule->PlayGroup(),
                                    rule->AutoExpire(),     rule->MaxEpisodes(),
                                    rule->MaxNewest(),      rule->AutoCommflag(),
                                    rule->AutoTranscode(),  rule->AutoMetaLookup(),
                                    rule->AutoUserJob1(),   rule->AutoUserJob2(),
                                    rule->AutoUserJob3(),   rule->AutoUserJob4(),
                                    rule->Transcoder());
        }

        bool RemoveRecordSchedule ( uint RecordId )
        {
            return m_obj.RemoveRecordSchedule(RecordId);
        }

        bool AddDontRecordSchedule( int              ChanId,
                                    const QDateTime &StartTime,
                                    bool             NeverRecord )
        {
            return m_obj.AddDontRecordSchedule(ChanId, StartTime, NeverRecord);
        }

        QObject* GetRecordScheduleList( int StartIndex, int Count )
        {
            return m_obj.GetRecordScheduleList(StartIndex, Count);
        }

        QObject* GetRecordSchedule ( uint      RecordId,
                                     QString   Template,
                                     int       ChanId,
                                     QDateTime StartTime,
                                     bool      MakeOverride )
        {
            return m_obj.GetRecordSchedule( RecordId,  Template, ChanId,
                                            StartTime, MakeOverride);
        }

        bool EnableRecordSchedule ( uint RecordId )
        {
            return m_obj.EnableRecordSchedule(RecordId);
        }

        bool DisableRecordSchedule( uint RecordId )
        {
            return m_obj.DisableRecordSchedule(RecordId);
        }

        QString RecStatusToString( int RecStatus )
        {
            return m_obj.RecStatusToString(RecStatus);
        }

        QString RecStatusToDescription( int RecStatus,
                                        int RecType,
                                        const QDateTime &StartTime )
        {
            return m_obj.RecStatusToDescription(RecStatus,
                                                RecType,
                                                StartTime );
        }

        QString RecTypeToString( QString RecType )
        {
            return m_obj.RecTypeToString( RecType );
        }

        QString RecTypeToDescription( QString RecType )
        {
            return m_obj.RecTypeToDescription( RecType );
        }

        QString DupMethodToString( QString DupMethod )
        {
            return m_obj.DupMethodToString( DupMethod );
        }

        QString DupMethodToDescription( QString DupMethod )
        {
            return m_obj.DupMethodToDescription( DupMethod );
        }

        QString DupInToString( QString DupIn )
        {
            return m_obj.DupInToString( DupIn );
        }

        QString DupInToDescription( QString DupIn )
        {
            return m_obj.DupInToDescription( DupIn );
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableDvr, QObject*);

#endif 
