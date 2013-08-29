//////////////////////////////////////////////////////////////////////////////
// Program Name: capture.cpp
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#include <QList>
#include <QFile>
#include <QMutex>

#include "capture.h"

#include "cardutil.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "mythdate.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CaptureCardList* Capture::GetCaptureCardList( const QString &sHostName,
                                                   const QString &sCardType )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Capture Cards."));

    QString str = "SELECT cardid, videodevice, audiodevice, vbidevice, "
                  "cardtype, audioratelimit, hostname, "
                  "dvb_swfilter, dvb_sat_type, dvb_wait_for_seqstart, "
                  "skipbtaudio, dvb_on_demand, dvb_diseqc_type, "
                  "firewire_speed, firewire_model, firewire_connection, "
                  "signal_timeout, channel_timeout, dvb_tuning_delay, "
                  "contrast, brightness, colour, hue, diseqcid, dvb_eitscan "
                  "from capturecard";

    if (!sHostName.isEmpty())
        str += " WHERE hostname = :HOSTNAME";
    else if (!sCardType.isEmpty())
        str += " WHERE cardtype = :CARDTYPE";

    if (!sHostName.isEmpty() && !sCardType.isEmpty())
        str += " AND cardtype = :CARDTYPE";

    query.prepare(str);

    if (!sHostName.isEmpty())
        query.bindValue(":HOSTNAME", sHostName);
    if (!sCardType.isEmpty())
        query.bindValue(":CARDTYPE", sCardType);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetCaptureCardList()", query);
        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    DTC::CaptureCardList* pList = new DTC::CaptureCardList();

    while (query.next())
    {

        DTC::CaptureCard *pCaptureCard = pList->AddNewCaptureCard();

        pCaptureCard->setCardId            ( query.value(0).toInt()       );
        pCaptureCard->setVideoDevice       ( query.value(1).toString()    );
        pCaptureCard->setAudioDevice       ( query.value(2).toString()    );
        pCaptureCard->setVBIDevice         ( query.value(3).toString()    );
        pCaptureCard->setCardType          ( query.value(4).toString()    );
        pCaptureCard->setAudioRateLimit    ( query.value(5).toUInt()      );
        pCaptureCard->setHostName          ( query.value(6).toString()    );
        pCaptureCard->setDVBSWFilter       ( query.value(7).toUInt()      );
        pCaptureCard->setDVBSatType        ( query.value(8).toUInt()      );
        pCaptureCard->setDVBWaitForSeqStart( query.value(9).toBool()     );
        pCaptureCard->setSkipBTAudio       ( query.value(10).toBool()     );
        pCaptureCard->setDVBOnDemand       ( query.value(11).toBool()     );
        pCaptureCard->setDVBDiSEqCType     ( query.value(12).toUInt()     );
        pCaptureCard->setFirewireSpeed     ( query.value(13).toUInt()     );
        pCaptureCard->setFirewireModel     ( query.value(14).toString()   );
        pCaptureCard->setFirewireConnection( query.value(15).toUInt()     );
        pCaptureCard->setSignalTimeout     ( query.value(16).toUInt()     );
        pCaptureCard->setChannelTimeout    ( query.value(17).toUInt()     );
        pCaptureCard->setDVBTuningDelay    ( query.value(18).toUInt()     );
        pCaptureCard->setContrast          ( query.value(19).toUInt()     );
        pCaptureCard->setBrightness        ( query.value(20).toUInt()     );
        pCaptureCard->setColour            ( query.value(21).toUInt()     );
        pCaptureCard->setHue               ( query.value(22).toUInt()     );
        pCaptureCard->setDiSEqCId          ( query.value(23).toUInt()     );
        pCaptureCard->setDVBEITScan        ( query.value(24).toBool()     );
    }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::CaptureCard* Capture::GetCaptureCard( int nCardId )
{
    if ( nCardId < 1 )
        throw( QString( "The Card ID is invalid."));

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Capture Cards."));

    QString str = "SELECT cardid, videodevice, audiodevice, vbidevice, "
                  "cardtype, audioratelimit, hostname, "
                  "dvb_swfilter, dvb_sat_type, dvb_wait_for_seqstart, "
                  "skipbtaudio, dvb_on_demand, dvb_diseqc_type, "
                  "firewire_speed, firewire_model, firewire_connection, "
                  "signal_timeout, channel_timeout, dvb_tuning_delay, "
                  "contrast, brightness, colour, hue, diseqcid, dvb_eitscan "
                  "from capturecard WHERE cardid = :CARDID";

    query.prepare(str);
    query.bindValue(":CARDID", nCardId);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetCaptureCard()", query);
        throw( QString( "Database Error executing query." ));
    }

    DTC::CaptureCard* pCaptureCard = new DTC::CaptureCard();

    if (query.next())
    {
        pCaptureCard->setCardId            ( query.value(0).toInt()       );
        pCaptureCard->setVideoDevice       ( query.value(1).toString()    );
        pCaptureCard->setAudioDevice       ( query.value(2).toString()    );
        pCaptureCard->setVBIDevice         ( query.value(3).toString()    );
        pCaptureCard->setCardType          ( query.value(4).toString()    );
        pCaptureCard->setAudioRateLimit    ( query.value(5).toUInt()      );
        pCaptureCard->setHostName          ( query.value(6).toString()    );
        pCaptureCard->setDVBSWFilter       ( query.value(7).toUInt()      );
        pCaptureCard->setDVBSatType        ( query.value(8).toUInt()      );
        pCaptureCard->setDVBWaitForSeqStart( query.value(9).toBool()     );
        pCaptureCard->setSkipBTAudio       ( query.value(10).toBool()     );
        pCaptureCard->setDVBOnDemand       ( query.value(11).toBool()     );
        pCaptureCard->setDVBDiSEqCType     ( query.value(12).toUInt()     );
        pCaptureCard->setFirewireSpeed     ( query.value(13).toUInt()     );
        pCaptureCard->setFirewireModel     ( query.value(14).toString()   );
        pCaptureCard->setFirewireConnection( query.value(15).toUInt()     );
        pCaptureCard->setSignalTimeout     ( query.value(16).toUInt()     );
        pCaptureCard->setChannelTimeout    ( query.value(17).toUInt()     );
        pCaptureCard->setDVBTuningDelay    ( query.value(18).toUInt()     );
        pCaptureCard->setContrast          ( query.value(19).toUInt()     );
        pCaptureCard->setBrightness        ( query.value(20).toUInt()     );
        pCaptureCard->setColour            ( query.value(21).toUInt()     );
        pCaptureCard->setHue               ( query.value(22).toUInt()     );
        pCaptureCard->setDiSEqCId          ( query.value(23).toUInt()     );
        pCaptureCard->setDVBEITScan        ( query.value(24).toBool()     );
    }

    return pCaptureCard;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Capture::RemoveCaptureCard( int nCardId )
{
    if ( nCardId < 1 )
        throw( QString( "The Card ID is invalid."));

    bool bResult = CardUtil::DeleteCard(nCardId);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int Capture::AddCaptureCard     ( const QString    &sVideoDevice,
                                  const QString    &sAudioDevice,
                                  const QString    &sVBIDevice,
                                  const QString    &sCardType,
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
    if ( sVideoDevice.isEmpty() || sCardType.isEmpty() || sHostName.isEmpty() )
        throw( QString( "This API requires at least a video device node, a card type, "
                        "and a hostname." ));

    int nResult = CardUtil::CreateCaptureCard(sVideoDevice, sAudioDevice,
                      sVBIDevice, sCardType, nAudioRateLimit,
                      sHostName, nDVBSWFilter, nDVBSatType, bDVBWaitForSeqStart,
                      bSkipBTAudio, bDVBOnDemand, nDVBDiSEqCType, nFirewireSpeed,
                      sFirewireModel, nFirewireConnection, nSignalTimeout,
                      nChannelTimeout, nDVBTuningDelay, nContrast, nBrightness,
                      nColour, nHue, nDiSEqCId, bDVBEITScan);

    if ( nResult < 1 )
        throw( QString( "Unable to create capture device." ));

    return nResult;
}

bool Capture::UpdateCaptureCard  ( int              nCardId,
                                   const QString    &sSetting,
                                   const QString    &sValue )
{
    if ( nCardId < 1 || sSetting.isEmpty() || sValue.isEmpty() )
        throw( QString( "Card ID, Setting Name, and Value are required." ));

    return set_on_source(sSetting, nCardId, 0, sValue);
}

// Card Inputs

bool Capture::RemoveCardInput( int nCardInputId )
{
    if ( nCardInputId < 1 )
        throw( QString( "The Input ID is invalid."));

    bool bResult = CardUtil::DeleteInput(nCardInputId);

    return bResult;
}

int Capture::AddCardInput       ( const uint nCardId,
                                  const uint nSourceId,
                                  const QString &sInputName,
                                  const QString &sExternalCommand,
                                  const QString &sChangerDevice,
                                  const QString &sChangerModel,
                                  const QString &sHostName,
                                  const QString &sTuneChan,
                                  const QString &sStartChan,
                                  const QString &sDisplayName,
                                  bool          bDishnetEIT,
                                  const uint nRecPriority,
                                  const uint nQuicktune,
                                  const uint nSchedOrder,
                                  const uint nLiveTVOrder)
{
    if ( nCardId < 1 || nSourceId < 1 || sInputName.isEmpty() )
        throw( QString( "This API requires at least a card ID, a source ID, "
                        "and an input name." ));

    int nResult = CardUtil::CreateCardInput(nCardId, nSourceId, sInputName,
                      sExternalCommand, sChangerDevice, sChangerModel,
                      sHostName, sTuneChan, sStartChan, sDisplayName,
                      bDishnetEIT, nRecPriority, nQuicktune, nSchedOrder, 
                      nLiveTVOrder);

    return nResult;
}

bool Capture::UpdateCardInput    ( int              nCardInputId,
                                   const QString    &sSetting,
                                   const QString    &sValue )
{
    if ( nCardInputId < 1 || sSetting.isEmpty() || sValue.isEmpty() )
        throw( QString( "Input ID, Setting Name, and Value are required." ));

    return set_on_input(sSetting, nCardInputId, sValue);
}

