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
    Q_CLASSINFO("UpdateRecordedWatchedStatus",  "methods=POST;name=bool")
    Q_CLASSINFO("GetSavedBookmark",    "name=long")
    Q_CLASSINFO("SetSavedBookmark",    "name=bool")

  public:
    V2Dvr();
   ~V2Dvr()  = default;
    static void RegisterCustomTypes();

  public slots:

    V2ProgramList* GetExpiringList     ( int              StartIndex,
                                            int              Count      );

    V2ProgramList* GetRecordedList     ( bool             Descending,
                                            int              StartIndex,
                                            int              Count,
                                            const QString   &TitleRegEx,
                                            const QString   &RecGroup,
                                            const QString   &StorageGroup,
                                            const QString   &Category,
                                            const QString   &Sort);

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

  private:
    Q_DISABLE_COPY(V2Dvr)

};

#endif
