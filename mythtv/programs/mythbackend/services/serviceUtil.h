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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include "programinfo.h"
#include "recordingrule.h"
#include "videometadatalistmanager.h"

void FillProgramInfo( DTC::Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel = true,
                      bool          bDetails    = true );

void FillChannelInfo( DTC::ChannelInfo *pChannel, 
                      ProgramInfo      *pInfo,
                      bool              bDetails = true );

void FillRecRuleInfo( DTC::RecRule  *pRecRule,
                      RecordingRule *pRule              );

void FillArtworkInfoList( DTC::ArtworkInfoList *pArtworkInfoList,
                          const QString        &sInetref,
                          uint                  nSeason );

void FillVideoMetadataInfo (
                      DTC::VideoMetadataInfo *pVideoMetadataInfo,
                      VideoMetadataListManager::VideoMetadataPtr pMetadata,
                      bool          bDetails);
#endif
