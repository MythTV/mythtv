/////////////////////////////////////////////////////////////////////////////
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
#include "v2playGroup.h"
#include "v2powerPriority.h"

#define DVR_SERVICE QString("/Dvr/")
#define DVR_HANDLE  QString("Dvr")

class V2Dvr : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "7.1")
    Q_CLASSINFO("RemoveOldRecorded",  "methods=POST;name=bool")
    Q_CLASSINFO("UpdateOldRecorded",  "methods=POST;name=bool")
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
    Q_CLASSINFO("GetLastPlayPos",      "name=long")
    Q_CLASSINFO("SetSavedBookmark",    "name=bool")
    Q_CLASSINFO("SetLastPlayPos",      "name=bool")
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
    Q_CLASSINFO("UpdateRecordedMetadata", "methods=POST")
    Q_CLASSINFO("AddPlayGroup",         "methods=POST")
    Q_CLASSINFO("UpdatePlayGroup",      "methods=POST")
    Q_CLASSINFO("RemovePlayGroup",      "methods=POST")
    Q_CLASSINFO("RemovePowerPriority",  "methods=POST")
    Q_CLASSINFO("AddPowerPriority",     "methods=POST")
    Q_CLASSINFO("UpdatePowerPriority",  "methods=POST")
    Q_CLASSINFO("CheckPowerQuery",      "methods=GET,POST,HEAD")

  public:
    V2Dvr();
   ~V2Dvr() override  = default;
    static void RegisterCustomTypes();

  public slots:

    static V2ProgramList* GetExpiringList(  int             StartIndex,
                                            int             Count      );

    V2ProgramList* GetRecordedList(         bool            Descending,
                                            int             StartIndex,
                                            int             Count,
                                            const QString   &TitleRegEx,
                                            const QString   &RecGroup,
                                            const QString   &StorageGroup,
                                            const QString   &Category,
                                            const QString   &Sort,
                                            bool             IgnoreLiveTV,
                                            bool             IgnoreDeleted,
                                            bool             IncChannel,
                                            bool             Details,
                                            bool             IncCast,
                                            bool             IncArtWork,
                                            bool             IncRecording);

    static V2ProgramList* GetOldRecordedList( bool             Descending,
                                            int              StartIndex,
                                            int              Count,
                                            const QDateTime &StartTime,
                                            const QDateTime &EndTime,
                                            const QString   &Title,
                                            const QString   &TitleRegEx,
                                            const QString   &SubtitleRegEx,
                                            const QString   &SeriesId,
                                            int              RecordId,
                                            const QString   &Sort);

    bool       RemoveOldRecorded         ( int              ChanId,
                                            const QDateTime &StartTime,
                                            bool            Reschedule );

    bool       UpdateOldRecorded         ( int              ChanId,
                                            const QDateTime &StartTime,
                                            bool            Duplicate,
                                            bool            Reschedule );

    static V2Program* GetRecorded         ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime  );

    static bool       AddRecordedCredits  ( int RecordedId,
                                            const QString & Cast);

    static int        AddRecordedProgram  ( const QString & Program);

    static bool       RemoveRecorded      ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            bool             ForceDelete,
                                            bool             AllowRerecord  );

    static bool       DeleteRecording     ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            bool             ForceDelete,
                                            bool             AllowRerecord  );

    static bool       UnDeleteRecording   ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime );

    static bool       StopRecording       ( int              RecordedId );

    static bool       ReactivateRecording ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            int              RecordId );

    static bool       RescheduleRecordings( void );

    static bool       AllowReRecord       ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime);

    static bool       UpdateRecordedWatchedStatus ( int   RecordedId,
                                                    int   ChanId,
                                                    const QDateTime &StartTime,
                                                    bool  Watched);

    static long       GetSavedBookmark    ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType );

    static long       GetLastPlayPos      ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType );

    static bool       SetSavedBookmark    ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType,
                                            long             Offset
                                            );

    static bool       SetLastPlayPos      ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType,
                                            long             Offset
                                            );

    static V2CutList* GetRecordedCutList  ( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType,
                                            bool IncludeFps );

    static V2CutList* GetRecordedCommBreak( int              RecordedId,
                                            int              ChanId,
                                            const QDateTime &StartTime,
                                            const QString   &OffsetType,
                                            bool IncludeFps );

    static V2CutList* GetRecordedSeek     ( int              RecordedId,
                                            const QString   &OffsetType );

    static V2MarkupList* GetRecordedMarkup( int              RecordedId );

    static bool    SetRecordedMarkup      ( int              RecordedId,
                                            const QString   &MarkupList);

    static V2ProgramList* GetConflictList ( int              StartIndex,
                                            int              Count,
                                            int              RecordId,
                                            const QString   &Sort);

    static V2ProgramList* GetUpcomingList ( int              StartIndex,
                                            int              Count,
                                            bool             ShowAll,
                                            int              RecordId,
                                            const QString &  RecStatus,
                                            const QString   &Sort,
                                            const QString &  RecGroup);

    static V2EncoderList*    GetEncoderList      ( );

    static V2InputList*      GetInputList        ( );

    static QStringList       GetRecGroupList     ( const QString   &UsedBy );

    static QStringList       GetProgramCategories   ( bool OnlyRecorded );

    static QStringList       GetRecStorageGroupList ( );

    static QStringList       GetPlayGroupList    ( );

    static V2PlayGroup*      GetPlayGroup    ( const QString & Name );

    static bool              RemovePlayGroup    ( const QString & Name );

    static bool              AddPlayGroup    ( const QString & Name,
                                               const QString & TitleMatch,
                                               int             SkipAhead,
                                               int             SkipBack,
                                               int             TimeStretch,
                                               int             Jump );

    bool                     UpdatePlayGroup ( const QString & Name,
                                               const QString & TitleMatch,
                                               int             SkipAhead,
                                               int             SkipBack,
                                               int             TimeStretch,
                                               int             Jump );

    static V2RecRuleFilterList* GetRecRuleFilterList ( );

    static QStringList       GetTitleList        ( const QString   &RecGroup );

    static V2TitleInfoList*  GetTitleInfoList  ( );

    // Recording Rules

    static uint       AddRecordSchedule   ( const QString&   Title,
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
                                            int       Transcoder,
                                            const QString&   AutoExtend);

    static bool        UpdateRecordSchedule ( uint    RecordId,
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
                                              int       Transcoder,
                                              const QString&   AutoExtend);

    static bool       RemoveRecordSchedule ( uint             RecordId   );

    static bool       AddDontRecordSchedule( int              ChanId,
                                             const QDateTime &StartTime,
                                             bool             NeverRecord );

    static V2RecRuleList* GetRecordScheduleList( int          StartIndex,
                                             int              Count,
                                             const            QString  &Sort,
                                             bool             Descending );

    static V2RecRule* GetRecordSchedule    ( uint             RecordId,
                                             const QString&   Template,
                                             int              RecordedId,
                                             int              ChanId,
                                             const QDateTime& StartTime,
                                             bool             MakeOverride );

    static bool       EnableRecordSchedule ( uint             RecordId   );

    static bool       DisableRecordSchedule( uint             RecordId   );

    static int        RecordedIdForKey     ( int              ChanId,
                                             const QDateTime &StartTime );

    static int        RecordedIdForPathname( const QString   &Pathname  );

    static QString    RecStatusToString    ( const QString  & RecStatus );

    static QString    RecStatusToDescription (const QString  & RecStatus,
                                               int            RecType,
                                               const QDateTime &StartTime );

    static QString    RecTypeToString      ( const QString&   RecType   );

    static QString    RecTypeToDescription ( const QString&   RecType   );

    static QString    DupMethodToString    ( const QString&   DupMethod );

    static QString    DupMethodToDescription ( const QString& DupMethod );

    static QString    DupInToString        ( const QString&   DupIn     );

    static QString    DupInToDescription   ( const QString&   DupIn     );

    int               ManageJobQueue       ( const QString   &Action,
                                             const QString   &JobName,
                                             int              JobId,
                                             int              RecordedId,
                                             QDateTime        JobStartTime,
                                             QString          RemoteHost,
                                             QString          JobArgs   );

    bool              UpdateRecordedMetadata ( uint             RecordedId,
                                               bool             AutoExpire,
                                               long             BookmarkOffset,
                                               const QString   &BookmarkOffsetType,
                                               bool             Damaged,
                                               const QString   &Description,
                                               uint             Episode,
                                               const QString   &Inetref,
                                               long             LastPlayOffset,
                                               const QString   &LastPlayOffsetType,
                                               QDate            OriginalAirDate,
                                               bool             Preserve,
                                               uint             Season,
                                               uint             Stars,
                                               const QString   &SubTitle,
                                               const QString   &Title,
                                               bool             Watched,
                                               const QString   &RecGroup );

    static V2PowerPriorityList* GetPowerPriorityList (const QString &PriorityName );

    static bool       RemovePowerPriority ( const QString & PriorityName );

    static bool       AddPowerPriority    ( const QString & PriorityName,
                                            int             RecPriority,
                                            const QString & SelectClause );

    bool              UpdatePowerPriority ( const QString & PriorityName,
                                            int             RecPriority,
                                            const QString & SelectClause );

    static QString    CheckPowerQuery( const QString & SelectClause );

  private:
    Q_DISABLE_COPY(V2Dvr)

};

#endif
