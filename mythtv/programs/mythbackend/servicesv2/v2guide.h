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

#include "v2serviceUtil.h"

#include "libmythbase/http/mythhttpservice.h"
#include "v2programAndChannel.h"
#include "v2channelGroupList.h"
#include "v2programList.h"
#include "v2programGuide.h"
#include "programinfo.h"

#define GUIDE_SERVICE QString("/Guide/")
#define GUIDE_HANDLE  QString("Guide")

class V2Guide : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("AddToChannelGroup",      "methods=POST;name=bool")
    Q_CLASSINFO("RemoveFromChannelGroup", "methods=POST;name=bool")

    public:
        V2Guide();
        ~V2Guide()  = default;
        static void RegisterCustomTypes();

    public slots:

        V2ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                  const QDateTime &EndTime    ,
                                                  bool             Details,
                                                  int              ChannelGroupId,
                                                  int              StartIndex,
                                                  int              Count,
                                                  bool             WithInvisible);

        V2ProgramList*   GetProgramList      ( int              StartIndex,
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
                                                  bool             WithInvisible);

        V2Program*       GetProgramDetails   ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetChannelIcon      ( int              ChanId,
                                                  int              Width ,
                                                  int              Height );

        V2ChannelGroupList*  GetChannelGroupList ( bool         IncludeEmpty );

        QStringList         GetCategoryList     ( );

        QStringList         GetStoredSearches( const QString   &Type );

        bool                AddToChannelGroup   ( int              ChannelGroupId,
                                                  int              ChanId );

        bool                RemoveFromChannelGroup ( int           ChannelGroupId,
                                                     int           ChanId );
    private:
        Q_DISABLE_COPY(V2Guide)

};


#endif
