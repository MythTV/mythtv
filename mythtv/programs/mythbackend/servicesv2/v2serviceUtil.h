#ifndef V2SERVICEUTIL_H
#define V2SERVICEUTIL_H

#include "videometadatalistmanager.h"
#include "v2videoMetadataInfo.h"

void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails);

void V2FillGenreList( V2GenreList *pGenreList, int videoID);


#endif //V2SERVICEUTIL_H