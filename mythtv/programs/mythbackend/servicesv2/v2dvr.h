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

#ifndef V2DVR_H
#define V2DVR_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2programList.h"
#include "v2cutList.h"
#include "v2markupList.h"
#include "v2encoderList.h"
#include "v2inputList.h"
#include "v2recRuleFilterList.h"
#include "v2titleInfoList.h"
#include "v2recRuleList.h"

#define DVR_SERVICE QString("/Dvr/")
#define DVR_HANDLE  QString("Dvr")

class V2Dvr : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("AddRecordedCredits",  "methods=POST;name=bool")
    Q_CLASSINFO("AddRecordedProgram",  "methods=POST;name=int")
    Q_CLASSINFO("RemoveRecorded",      "methods=POST;name=bool")
    Q_CLASSINFO("DeleteRecording",     "methods=POST;name=bool")
    Q_CLASSINFO("UnDeleteRecording",   "methods=POST;name=bool")
    Q_CLASSINFO("StopRecording",       "methods=POST;name=bool")
    Q_CLASSINFO("ReactivateRecording", "methods=POST;name=bool")
    Q_CLASSINFO("RescheduleRecordings","methods=POST;name=bool")
    Q_CLASSINFO("AllowReRecord",       "methods=POST;name=bool")
    Q_CLASSINFO("UpdateRecordedWatchedStatus","methods=POST;name=bool")
    Q_CLASSINFO("GetSavedBookmark",    "name=long")
    Q_CLASSINFO("SetSavedBookmark",    "name=bool")
    Q_CLASSINFO("SetRecordedMarkup",   "name=bool")
    Q_CLASSINFO("AddRecordSchedule",   "methods=POST;name=uint")
    Q_CLASSINFO("UpdateRecordSchedule","methods=POST;name=bool")
    Q_CLASSINFO("RemoveRecordSchedule","methods=POST;name=bool")
    Q_CLASSINFO("AddDontRecordSchedule","methods=POST;name=bool")
    Q_CLASSINFO("EnableRecordSchedule", "methods=POST;name=bool")
    Q_CLASSINFO("DisableRecordSchedule","methods=POST;name=bool")
    Q_CLASSINFO("RecordedIdForKey",     "methods=GET,POST,HEAD;name=int")
    Q_CLASSINFO("RecordedIdForPathname","methods=GET,POST,HEAD;name=int")
    Q_CLASSINFO("RecStatusToString",    "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("RecStatusToDescription","methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("RecTypeToString",      "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("RecTypeToDescription", "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("DupMethodToString",    "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("DupMethodToDescription","methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("DupInToString",        "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("DupInToDescription",   "methods=GET,POST,HEAD;name=String")
    Q_CLASSINFO("ManageJobQueue",       "methods=POST;name=int")

  public:
    V2Dvr();
   ~V2Dvr()  = default;
    static void RegisterCustomTypes();

  public slots:

    V2ProgramList* GetExpiringList     ( int              StartIndex,
                                            int              Count      );

    V2ProgramList* GetRecordedList     ( bool           Descending,
                                         int            StartIndex,
                                         int            Count,
                                         const QString &TitleRegEx,
                                         const QString &RecGroup,
                                         const QString &StorageGroup,
                                         const QString &Category,
                                         const QString &Sort,
                                         bool           IgnoreLiveTV,
                                         bool           IgnoreDeleted);

    V2ProgramList* GetOldRecordedList  ( bool             Descending,
                                            int              StartIndex,
                                            int              Count,
                                            const QDateTime &StartTime,
                                            const QDateTime &EndTime,
                                            const QString   &Title,
                                            const QString   &SeriesId,
                                            int              RecordId,
                                            const QString   &Sort);

    V2Program*     GetRecorded         ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw  );

    bool              AddRecordedCredits  ( int RecordedId,
                                            const QJsonObject & json);

    int               AddRecordedProgram  ( const QJsonObject & json);

    bool              RemoveRecorded      ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw,
                                            bool             ForceDelete,
                                            bool             AllowRerecord  );

    bool              DeleteRecording     ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw,
                                            bool             ForceDelete,
                                            bool             AllowRerecord  );

    bool              UnDeleteRecording   ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw );

    bool              StopRecording       ( int              RecordedId );

    bool              ReactivateRecording ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw );

    bool              RescheduleRecordings( void );

    bool              AllowReRecord       ( int             RecordedId );

    bool              UpdateRecordedWatchedStatus ( int   RecordedId,
                                                    int   ChanId,
                                                    const QDateTime &recstarttsRaw,
                                                    bool  Watched);

    long              GetSavedBookmark     ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw,
                                            const QString   &OffsetType );

    bool              SetSavedBookmark     ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw,
                                            const QString   &OffsetType,
                                            long             Offset
                                            );

    V2CutList*     GetRecordedCutList  ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &recstarttsRaw,
                                            const QString   &OffsetType );

    V2CutList*     GetRecordedCommBreak ( int              RecordedId,
                                              int              ChanId,
                                              const QDateTime &recstarttsRaw,
                                              const QString   &OffsetType );

    V2CutList*     GetRecordedSeek      ( int              RecordedId,
                                              const QString   &OffsetType );

    V2MarkupList*  GetRecordedMarkup ( int                RecordedId );

    bool              SetRecordedMarkup     ( int            RecordedId,
                                              const QJsonObject & json );

    V2ProgramList* GetConflictList     ( int              StartIndex,
                                            int              Count,
                                            int              RecordId );

    V2ProgramList* GetUpcomingList     ( int              StartIndex,
                                            int              Count,
                                            bool             ShowAll,
                                            int              RecordId,
                                            int              RecStatus );

    V2EncoderList* GetEncoderList      ( );

    V2InputList*   GetInputList        ( );

    QStringList       GetRecGroupList     ( );

    QStringList       GetProgramCategories   ( bool OnlyRecorded );

    QStringList       GetRecStorageGroupList ( );

    QStringList       GetPlayGroupList    ( );

    V2RecRuleFilterList* GetRecRuleFilterList ( );

    QStringList       GetTitleList        ( const QString   &RecGroup );

    V2TitleInfoList* GetTitleInfoList  ( );

    // Recording Rules

    uint              AddRecordSchedule   ( const QString&   Title,
                                            const QString&   Subtitle,
                                            const QString&   Description,
                                            const QString&   Category,
                                            const QDateTime& StartTime,
                                            const QDateTime& EndTime,
                                            const QString&   SeriesId,
                                            const QString&   ProgramId,
                                            int       ChanId,
                                            const QString&   Station,
                                            int       FindDay,
                                            QTime     FindTime,
                                            int       ParentId,
                                            bool      Inactive,
                                            uint      Season,
                                            uint      Episode,
                                            const QString&   Inetref,
                                            QString   Type,
                                            QString   SearchType,
                                            int       RecPriority,
                                            uint      PreferredInput,
                                            int       StartOffset,
                                            int       EndOffset,
                                            const QDateTime& LastRecorded,
                                            QString   DupMethod,
                                            QString   DupIn,
                                            bool      NewEpisOnly,
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
                                              const QString&   Title,
                                              const QString&   Subtitle,
                                              const QString&   Description,
                                              const QString&   Category,
                                              const QDateTime& StartTime,
                                              const QDateTime& EndTime,
                                              const QString&   SeriesId,
                                              const QString&   ProgramId,
                                              int       ChanId,
                                              const QString&   Station,
                                              int       FindDay,
                                              QTime     FindTime,
                                              bool      Inactive,
                                              uint      Season,
                                              uint      Episode,
                                              const QString&   Inetref,
                                              QString   Type,
                                              QString   SearchType,
                                              int       RecPriority,
                                              uint      PreferredInput,
                                              int       StartOffset,
                                              int       EndOffset,
                                              QString   DupMethod,
                                              QString   DupIn,
                                              bool      NewEpisOnly,
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

    V2RecRuleList* GetRecordScheduleList( int              StartIndex,
                                              int              Count,
                                              const            QString  &Sort,
                                              bool             Descending );

    V2RecRule*     GetRecordSchedule    ( uint             RecordId,
                                              const QString&   Template,
                                              int              nRecordedId,
                                              int              ChanId,
                                              const QDateTime& dStartTimeRaw,
                                              bool             MakeOverride );

    bool              EnableRecordSchedule ( uint             RecordId   );

    bool              DisableRecordSchedule( uint             RecordId   );

    int               RecordedIdForKey( int              ChanId,
                                        const QDateTime &recstarttsRaw );

    int               RecordedIdForPathname( const QString   &pathname );

    QString           RecStatusToString    ( int              RecStatus );

    QString           RecStatusToDescription ( int            RecStatus,
                                                int            RecType,
                                                const QDateTime &StartTime );

    QString           RecTypeToString      ( const QString&   RecType   );

    QString           RecTypeToDescription ( const QString&   RecType   );

    QString           DupMethodToString    ( const QString&   DupMethod );

    QString           DupMethodToDescription ( const QString& DupMethod );

    QString           DupInToString        ( const QString&   DupIn     );

    QString           DupInToDescription   ( const QString&   DupIn     );

    int               ManageJobQueue       ( const QString   &Action,
                                              const QString   &JobName,
                                              int              JobId,
                                              int              RecordedId,
                                                    QDateTime  jobstarttsRaw,
                                                    QString    RemoteHost,
                                                    QString    JobArgs   );



  private:
    Q_DISABLE_COPY(V2Dvr)

};

#endif
