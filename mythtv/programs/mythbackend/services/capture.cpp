//////////////////////////////////////////////////////////////////////////////
// Program Name: capture.cpp
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QList>
#include <QFile>
#include <QMutex>

#include "capture.h"

#include "cardutil.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "util.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Capture::RemoveCaptureCard( int Id )
{
    bool bResult = CardUtil::DeleteCard(Id);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int Capture::AddCaptureCard     ( const QString    &sVideoDevice,
                                  const QString    &sAudioDevice,
                                  const QString    &sVBIDevice,
                                  const QString    &sCardType,
                                  const QString    &sDefaultInput,
                                  const uint       nAudioRateLimit,
                                  const QString    &sHostName,
                                  const uint       nDVBSWFilter,
                                  const uint       nDVBSatType,
                                  bool             bDVBWaitForSeqStart,
                                  bool             bSkipBTAudio,
                                  bool             bDVBOnDemand,
                                  const uint       nDVBDiSEqCType,
                                  const uint       nFirewireSpeed,
                                  const QString    &sFirewireModel,
                                  const uint       nFirewireConnection,
                                  const uint       nSignalTimeout,
                                  const uint       nChannelTimeout,
                                  const uint       nDVBTuningDelay,
                                  const uint       nContrast,
                                  const uint       nBrightness,
                                  const uint       nColour,
                                  const uint       nHue,
                                  const uint       nDiSEqCId,
                                  bool             bDVBEITScan)
{
    int nResult = CardUtil::CreateCaptureCard(sVideoDevice, sAudioDevice,
                      sVBIDevice, sCardType, sDefaultInput, nAudioRateLimit,
                      sHostName, nDVBSWFilter, nDVBSatType, bDVBWaitForSeqStart,
                      bSkipBTAudio, bDVBOnDemand, nDVBDiSEqCType, nFirewireSpeed,
                      sFirewireModel, nFirewireConnection, nSignalTimeout,
                      nChannelTimeout, nDVBTuningDelay, nContrast, nBrightness,
                      nColour, nHue, nDiSEqCId, bDVBEITScan);

    if ( nResult < 1 )
        throw( QString( "Unable to create capture device." ));

    return nResult;
}
