//////////////////////////////////////////////////////////////////////////////
// Program Name: guide.h
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

#ifndef V2GUIDE_H
#define V2GUIDE_H

// Qt
#include <QFileInfo>

// MythTV
#include "libmythbase/http/mythhttpservice.h"
#include "libmythbase/programinfo.h"

// MythBackend
#include "v2channelGroupList.h"
#include "v2programAndChannel.h"
#include "v2programGuide.h"
#include "v2programList.h"
#include "v2serviceUtil.h"

#define GUIDE_SERVICE QString("/Guide/")
#define GUIDE_HANDLE  QString("Guide")

class V2Guide : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "2.4")
    Q_CLASSINFO("AddToChannelGroup",      "methods=POST;name=bool")
    Q_CLASSINFO("RemoveFromChannelGroup", "methods=POST;name=bool")
    Q_CLASSINFO("AddChannelGroup",        "methods=POST;name=int")
    Q_CLASSINFO("RemoveChannelGroup",     "methods=POST;name=bool")
    Q_CLASSINFO("UpdateChannelGroup",     "methods=POST;name=bool")

    public:
        V2Guide();
        ~V2Guide() override  = default;
        static void RegisterCustomTypes();

    public slots:

        static V2ProgramGuide*  GetProgramGuide ( const QDateTime &StartTime  ,
                                                  const QDateTime &EndTime    ,
                                                  bool             Details,
                                                  int              ChannelGroupId,
                                                  int              StartIndex,
                                                  int              Count,
                                                  bool             WithInvisible);

        static V2ProgramList*   GetProgramList  ( int              StartIndex,
                                                  int              Count,
                                                  const QDateTime &StartTime  ,
                                                  const QDateTime &EndTime    ,
                                                  int              ChanId,
                                                  const QString   &TitleFilter,
                                                  const QString   &CategoryFilter,
                                                  const QString   &PersonFilter,
                                                  const QString   &KeywordFilter,
                                                  bool             OnlyNew,
                                                  bool             Details,
                                                  const QString   &Sort,
                                                  bool             Descending,
                                                  bool             WithInvisible,
                                                  const QString   &CatType);

        static V2Program*   GetProgramDetails   ( int              ChanId,
                                                  const QDateTime &StartTime );

        static QFileInfo    GetChannelIcon      ( int              ChanId,
                                                  int              Width ,
                                                  int              Height,
                                                  const QString   &FileName );

        static V2ChannelGroupList*  GetChannelGroupList ( bool         IncludeEmpty );

        static QStringList  GetCategoryList     ( );

        static QStringList  GetStoredSearches( const QString   &Type );

        static bool         AddToChannelGroup   ( int              ChannelGroupId,
                                                  int              ChanId );

        static bool      RemoveFromChannelGroup ( int           ChannelGroupId,
                                                  int           ChanId );

        static int          AddChannelGroup     ( const QString &Name);

        static bool         RemoveChannelGroup  ( const QString &Name);

        static bool         UpdateChannelGroup  ( const QString &OldName,
                                                  const QString &NewName);

    private:
        Q_DISABLE_COPY(V2Guide)

};


#endif
