//////////////////////////////////////////////////////////////////////////////
// Program Name: music.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2MUSIC_H
#define V2MUSIC_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2musicMetadataInfoList.h"

#define MUSIC_SERVICE QString("/Music/")
#define MUSIC_HANDLE  QString("Music")


class V2Music : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")

  public:
    V2Music();
   ~V2Music() override  = default;
    static void RegisterCustomTypes();

  public slots:

    /* V2Music Metadata Methods */

    static V2MusicMetadataInfoList*  GetTrackList ( int      StartIndex,
                                                    int      Count      );

    static V2MusicMetadataInfo*      GetTrack     ( int      Id         );

  private:
    Q_DISABLE_COPY(V2Music)

};

#endif
