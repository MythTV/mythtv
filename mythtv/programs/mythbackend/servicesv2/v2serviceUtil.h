#ifndef V2SERVICEUTIL_H
#define V2SERVICEUTIL_H

// Qt
#include <QDir>

// MythTV
#include "libmythmetadata/musicmetadata.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythtv/channelgroup.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/inputinfo.h"
#include "libmythtv/programdata.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingrule.h"

// MythBackend
#include "v2channelGroup.h"
#include "v2cutList.h"
#include "v2input.h"
#include "v2musicMetadataInfoList.h"
#include "v2programList.h"
#include "v2recRule.h"
#include "v2videoMetadataInfo.h"
#include "v2captureCardList.h"

template <typename T>
static inline void
ADD_SQLv2(QString& settings_var, MSqlBindings& bindvar,
          const QString& col, const QString& api_param, const T& val)
{
    settings_var += QString("%1=:%2, ").arg(col, api_param);
    bindvar[QString(":").append(api_param)] = val;
}

const QStringList KnownServicesV2 = { "Capture", "Channel", "Content", \
                                      "Dvr",     "Guide",   "Music",   \
                                      "Myth",    "Video",   "Config" };
void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails);

void V2FillMusicMetadataInfo (V2MusicMetadataInfo *pVideoMetadataInfo,
                            MusicMetadata *pMetadata, bool bDetails);

void V2FillGenreList( V2GenreList *pGenreList, int videoID);

void V2FillProgramInfo( V2Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel   = true,
                      bool          bDetails      = true,
                      bool          bIncCast      = true,
                      bool          bIncArtWork   = true,
                      bool          bIncRecording = true);

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

void V2FillCutList( V2CutList* pCutList, ProgramInfo* rInfo, int marktype, bool includeFps=false);

void V2FillCommBreak( V2CutList* pCutList, ProgramInfo* rInfo, int marktype, bool includeFps=false);

void V2FillSeek(V2CutList* pCutList, RecordingInfo* rInfo, MarkTypes marktype);

void V2FillInputInfo( V2Input *input, const InputInfo& inputInfo);

int V2CreateRecordingGroup(const QString& groupName);

void FillEncoderList(QVariantList& list, QObject* parent);

int FillUpcomingList(QVariantList& list, QObject* parent,
                                        int& nStartIndex,
                                        int& nCount,
                                        bool bShowAll,
                                        int  nRecordId,
                                        int  nRecStatus,
                                        const QString  &Sort = QString());

void FillFrontendList(QVariantList &list, QObject* parent, bool OnLine);

V2CaptureDeviceList* getV4l2List  ( const QRegularExpression &driver, const QString & cardType );

uint fillSelectionsFromDir(const QDir& dir,
                            uint minor_min, uint minor_max,
                            const QString& card, const QRegularExpression& driver,
                            bool allow_duplicates, V2CaptureDeviceList *pList,
                            const QString & cardType);

V2CaptureDeviceList* getFirewireList (const QString & cardType);

#endif //V2SERVICEUTIL_H
