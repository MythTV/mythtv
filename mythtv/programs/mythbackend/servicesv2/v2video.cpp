#include "v2video.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "videometadata.h"
#include "v2serviceUtil.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (VIDEO_HANDLE, V2Video::staticMetaObject, std::bind(&V2Video::RegisterCustomTypes)))

void V2Video::RegisterCustomTypes()
{
    qRegisterMetaType<V2VideoMetadataInfo*>("V2VideoMetadataInfo");
}


V2Video::V2Video()
  : MythHTTPService(s_service)
{
}


V2VideoMetadataInfo* V2Video::GetVideo( int Id )
{
    VideoMetadataListManager::VideoMetadataPtr metadata =
                          VideoMetadataListManager::loadOneFromDatabase(Id);

    if ( !metadata )
        throw( QString( "No metadata found for selected ID!." ));

    auto *pVideoMetadataInfo = new V2VideoMetadataInfo();

    V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}
