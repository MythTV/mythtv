#ifndef V2SERVICEUTIL_H
#define V2SERVICEUTIL_H

#include "videometadatalistmanager.h"
#include "v2videoMetadataInfo.h"
#include "v2programList.h"
#include "v2channelGroup.h"
#include "v2recRule.h"
#include "v2cutList.h"
#include "v2input.h"
#include "v2musicMetadataInfoList.h"
#include "channelinfo.h"
#include "channelgroup.h"
#include "recordingrule.h"
#include "recordinginfo.h"
#include "programdata.h"
#include "inputinfo.h"
#include "musicmetadata.h"

#define ADD_SQL(settings_var, bindvar, col, api_param, val) { \
    (settings_var) += QString("%1=:%2, ").arg(col, api_param); \
    (bindvar)[QString(":").append(api_param)] = val; \
    }

#define HAS_PARAM(p) m_request->m_queries.contains(QString(p).toLower())

const QStringList KnownServices = { "Capture", "Channel", "Content", \
                                    "Dvr",     "Guide",   "Music",   \
                                    "Myth",    "Video" };
void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails);

void V2FillMusicMetadataInfo (V2MusicMetadataInfo *pVideoMetadataInfo,
                            MusicMetadata *pMetadata, bool bDetails);

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

DBCredits * V2jsonCastToCredits(const QJsonObject &cast);

void V2FillCutList( V2CutList* pCutList, RecordingInfo* rInfo, int marktype);

void V2FillCommBreak( V2CutList* pCutList, RecordingInfo* rInfo, int marktype);

void V2FillSeek(V2CutList* pCutList, RecordingInfo* rInfo, MarkTypes marktype);

void V2FillInputInfo( V2Input *input, const InputInfo& inputInfo);

int V2CreateRecordingGroup(const QString& groupName);

void FillEncoderList(QVariantList& list, QObject* parent);

int FillUpcomingList(QVariantList& list, QObject* parent,
                                        int& nStartIndex,
                                        int& nCount,
                                        bool bShowAll,
                                        int  nRecordId,
                                        int  nRecStatus );

void FillFrontendList(QVariantList &list, QObject* parent, bool OnLine);

#endif //V2SERVICEUTIL_H
