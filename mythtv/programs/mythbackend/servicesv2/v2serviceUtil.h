#ifndef V2SERVICEUTIL_H
#define V2SERVICEUTIL_H

#include "videometadatalistmanager.h"
#include "v2videoMetadataInfo.h"
#include "v2programList.h"
#include "v2channelGroup.h"
#include "v2recRule.h"
#include "channelinfo.h"
#include "channelgroup.h"
#include "recordingrule.h"

#define HAS_PARAM(p) m_parsedParams.contains(p)

const QStringList KnownServices = { "Capture", "Channel", "Content", \
                                    "Dvr",     "Guide",   "Music",   \
                                    "Myth",    "Video" };
void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails);

void V2FillGenreList( V2GenreList *pGenreList, int videoID);

void V2FillProgramInfo( V2Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel = true,
                      bool          bDetails    = true,
                      bool          bIncCast    = true);

bool V2FillChannelInfo( V2ChannelInfo *pChannel,
                      uint              nChanID,
                      bool              bDetails = true );

bool V2FillChannelInfo( V2ChannelInfo *pChannel,
                      const ChannelInfo &channelInfo,
                      bool              bDetails = true );

void V2FillChannelGroup( V2ChannelGroup *pGroup, const ChannelGroupItem& pGroupItem);

void V2FillRecRuleInfo( V2RecRule  *pRecRule,
                      RecordingRule *pRule              );

void V2FillCastMemberList( V2CastMemberList *pCastMemberList,
                         ProgramInfo  *pInfo);

void V2FillArtworkInfoList( V2ArtworkInfoList *pArtworkInfoList,
                          const QString        &sInetref,
                          uint                  nSeason );


#endif //V2SERVICEUTIL_H
