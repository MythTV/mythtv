//////////////////////////////////////////////////////////////////////////////
// Program Name: serviceUtil.h
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

#ifndef _SERVICEUTIL_H_
#define _SERVICEUTIL_H_

#include "datacontracts/programAndChannel.h"
#include "datacontracts/recRule.h"
#include "datacontracts/artworkInfoList.h"
#include "datacontracts/videoMetadataInfo.h"
#include "datacontracts/musicMetadataInfo.h"
#include "datacontracts/channelGroup.h"
#include "datacontracts/input.h"
#include "datacontracts/castMemberList.h"
#include "datacontracts/cutList.h"
#include "datacontracts/genreList.h"

#include "programinfo.h"
#include "recordingrule.h"
#include "videometadatalistmanager.h"
#include "channelgroup.h"
#include "inputinfo.h"
#include "channelinfo.h"
#include "recordinginfo.h"
#include "musicmetadata.h"

#define ADD_SQL(settings_var, bindvar, col, api_param, val) { \
    settings_var += QString("%1=:%2, ").arg(col).arg(api_param); \
    bindvar[QString(":").append(api_param)] = val; \
    }

#define HAS_PARAM(p) m_parsedParams.contains(p)

const QStringList KnownServices = { "Capture", "Channel", "Content", \
                                    "Dvr",     "Guide",   "Music",   \
                                    "Myth",    "Video" };

void FillProgramInfo( DTC::Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel = true,
                      bool          bDetails    = true,
                      bool          bIncCast    = true);

bool FillChannelInfo( DTC::ChannelInfo *pChannel,
                      uint              nChanID,
                      bool              bDetails = true );

bool FillChannelInfo( DTC::ChannelInfo *pChannel,
                      const ChannelInfo &channelInfo,
                      bool              bDetails = true );

void FillChannelGroup( DTC::ChannelGroup *pGroup, const ChannelGroupItem& pGroupItem);

void FillRecRuleInfo( DTC::RecRule  *pRecRule,
                      RecordingRule *pRule              );

void FillArtworkInfoList( DTC::ArtworkInfoList *pArtworkInfoList,
                          const QString        &sInetref,
                          uint                  nSeason );

void FillGenreList( DTC::GenreList *pGenreList, int videoID);

void FillVideoMetadataInfo (
                      DTC::VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails);

void FillMusicMetadataInfo (DTC::MusicMetadataInfo *pVideoMetadataInfo,
                            MusicMetadata *pMetadata, bool bDetails);

void FillInputInfo( DTC::Input *input, const InputInfo& inputInfo);

void FillCastMemberList( DTC::CastMemberList *pCastMemberList,
                         ProgramInfo  *pInfo);

void FillCutList( DTC::CutList* pCutList, RecordingInfo* rInfo, int marktype);

void FillCommBreak( DTC::CutList* pCutList, RecordingInfo* rInfo, int marktype);

void FillSeek(DTC::CutList* pCutList, RecordingInfo* rInfo, MarkTypes marktype);


#endif
