//////////////////////////////////////////////////////////////////////////////
// Program Name: music.cpp
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

// C++
#include <cmath>
#include <unistd.h>

// MythTV
#include "libmythbase/mythversion.h"
#include "libmythmetadata/musicmetadata.h"

// MythBackend
#include "music.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::MusicMetadataInfoList* Music::GetTrackList(int nStartIndex,
                                                int nCount)
{
    auto *all_music = new AllMusic();

    while (!all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    MetadataPtrList *musicList = all_music->getAllMetadata();

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pMusicMetadataInfos = new DTC::MusicMetadataInfoList();

    auto musicListCount = static_cast<int>(musicList->count());
    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, musicListCount ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, musicListCount ) : musicListCount;
    int nEndIndex = std::min((nStartIndex + nCount), musicListCount );

    for( int n = nStartIndex; n < nEndIndex; n++ )
    {
        DTC::MusicMetadataInfo *pMusicMetadataInfo = pMusicMetadataInfos->AddNewMusicMetadataInfo();

        MusicMetadata *metadata = musicList->at(n);

        if (metadata)
            FillMusicMetadataInfo ( pMusicMetadataInfo, metadata, true );
    }

    int curPage = 0;
    int totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)std::ceil((float)musicList->count() / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)std::ceil((float)nStartIndex / nCount) + 1;
    }

    pMusicMetadataInfos->setStartIndex    ( nStartIndex     );
    pMusicMetadataInfos->setCount         ( nCount          );
    pMusicMetadataInfos->setCurrentPage   ( curPage         );
    pMusicMetadataInfos->setTotalPages    ( totalPages      );
    pMusicMetadataInfos->setTotalAvailable( musicList->count()  );
    pMusicMetadataInfos->setAsOf          ( MythDate::current() );
    pMusicMetadataInfos->setVersion       ( MYTH_BINARY_VERSION );
    pMusicMetadataInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    delete all_music;

    return pMusicMetadataInfos;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::MusicMetadataInfo* Music::GetTrack(int Id)
{
    auto *all_music = new AllMusic();

    while (!all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    MusicMetadata *metadata = all_music->getMetadata(Id);

    if (!metadata)
    {
        delete all_music;
        throw(QString("No metadata found for selected ID!."));
    }

    auto *pMusicMetadataInfo = new DTC::MusicMetadataInfo();

    FillMusicMetadataInfo(pMusicMetadataInfo, metadata, true);

    delete all_music;

    return pMusicMetadataInfo;
}
