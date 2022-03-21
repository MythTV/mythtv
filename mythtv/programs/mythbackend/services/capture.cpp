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

// Qt
#include <QList>
#include <QFile>
#include <QMutex>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythversion.h"
#include "libmythtv/cardutil.h"

// MythBackend
#include "capture.h"
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

    QString str = "SELECT cardid, parentid, videodevice, audiodevice, vbidevice, "
                  "cardtype, defaultinput, audioratelimit, hostname, "
                  "dvb_swfilter, dvb_sat_type, dvb_wait_for_seqstart, "
                  "skipbtaudio, dvb_on_demand, dvb_diseqc_type, "
                  "firewire_speed, firewire_model, firewire_connection, "
                  "signal_timeout, channel_timeout, dvb_tuning_delay, "
                  "contrast, brightness, colour, hue, diseqcid, dvb_eitscan, "
                  "inputname, sourceid, externalcommand, changer_device, "
                  "changer_model, tunechan, startchan, displayname, "
                  "dishnet_eit, recpriority, quicktune, schedorder, "
                  "livetvorder, reclimit, schedgroup "
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

    auto* pList = new DTC::CaptureCardList();

    while (query.next())
    {

        DTC::CaptureCard *pCaptureCard = pList->AddNewCaptureCard();

        pCaptureCard->setCardId            ( query.value(0).toInt()       );
        pCaptureCard->setParentId          ( query.value(1).toInt()       );
        pCaptureCard->setVideoDevice       ( query.value(2).toString()    );
        pCaptureCard->setAudioDevice       ( query.value(3).toString()    );
        pCaptureCard->setVBIDevice         ( query.value(4).toString()    );
        pCaptureCard->setCardType          ( query.value(5).toString()    );
        pCaptureCard->setDefaultInput      ( query.value(6).toString()    );
        pCaptureCard->setAudioRateLimit    ( query.value(7).toUInt()      );
        pCaptureCard->setHostName          ( query.value(8).toString()    );
        pCaptureCard->setDVBSWFilter       ( query.value(9).toUInt()      );
        pCaptureCard->setDVBSatType        ( query.value(10).toUInt()     );
        pCaptureCard->setDVBWaitForSeqStart( query.value(11).toBool()     );
        pCaptureCard->setSkipBTAudio       ( query.value(12).toBool()     );
        pCaptureCard->setDVBOnDemand       ( query.value(13).toBool()     );
        pCaptureCard->setDVBDiSEqCType     ( query.value(14).toUInt()     );
        pCaptureCard->setFirewireSpeed     ( query.value(15).toUInt()     );
        pCaptureCard->setFirewireModel     ( query.value(16).toString()   );
        pCaptureCard->setFirewireConnection( query.value(17).toUInt()     );
        pCaptureCard->setSignalTimeout     ( query.value(18).toUInt()     );
        pCaptureCard->setChannelTimeout    ( query.value(19).toUInt()     );
        pCaptureCard->setDVBTuningDelay    ( query.value(20).toUInt()     );
        pCaptureCard->setContrast          ( query.value(21).toUInt()     );
        pCaptureCard->setBrightness        ( query.value(22).toUInt()     );
        pCaptureCard->setColour            ( query.value(23).toUInt()     );
        pCaptureCard->setHue               ( query.value(24).toUInt()     );
        pCaptureCard->setDiSEqCId          ( query.value(25).toUInt()     );
        pCaptureCard->setDVBEITScan        ( query.value(26).toBool()     );
        pCaptureCard->setInputName         ( query.value(27).toString()   );
        pCaptureCard->setSourceId          ( query.value(28).toInt()      );
        pCaptureCard->setExternalCommand   ( query.value(29).toString()   );
        pCaptureCard->setChangerDevice     ( query.value(30).toString()   );
        pCaptureCard->setChangerModel      ( query.value(31).toString()   );
        pCaptureCard->setTuneChannel       ( query.value(32).toString()   );
        pCaptureCard->setStartChannel      ( query.value(33).toString()   );
        pCaptureCard->setDisplayName       ( query.value(34).toString()   );
        pCaptureCard->setDishnetEit        ( query.value(35).toBool()     );
        pCaptureCard->setRecPriority       ( query.value(36).toInt()      );
        pCaptureCard->setQuickTune         ( query.value(37).toBool()     );
        pCaptureCard->setSchedOrder        ( query.value(38).toUInt()     );
        pCaptureCard->setLiveTVOrder       ( query.value(39).toUInt()     );
        pCaptureCard->setRecLimit          ( query.value(40).toUInt()     );
        pCaptureCard->setSchedGroup        ( query.value(41).toBool()     );
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

    QString str = "SELECT cardid, parentid, videodevice, audiodevice, vbidevice, "
                  "cardtype, defaultinput, audioratelimit, hostname, "
                  "dvb_swfilter, dvb_sat_type, dvb_wait_for_seqstart, "
                  "skipbtaudio, dvb_on_demand, dvb_diseqc_type, "
                  "firewire_speed, firewire_model, firewire_connection, "
                  "signal_timeout, channel_timeout, dvb_tuning_delay, "
                  "contrast, brightness, colour, hue, diseqcid, dvb_eitscan, "
                  "inputname, sourceid, externalcommand, changer_device, "
                  "changer_model, tunechan, startchan, displayname, "
                  "dishnet_eit, recpriority, quicktune, schedorder, "
                  "livetvorder, reclimit, schedgroup "
                  "from capturecard WHERE cardid = :CARDID";

    query.prepare(str);
    query.bindValue(":CARDID", nCardId);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetCaptureCard()", query);
        throw( QString( "Database Error executing query." ));
    }

    auto* pCaptureCard = new DTC::CaptureCard();

    if (query.next())
    {
        pCaptureCard->setCardId            ( query.value(0).toInt()       );
        pCaptureCard->setParentId          ( query.value(1).toInt()       );
        pCaptureCard->setVideoDevice       ( query.value(2).toString()    );
        pCaptureCard->setAudioDevice       ( query.value(3).toString()    );
        pCaptureCard->setVBIDevice         ( query.value(4).toString()    );
        pCaptureCard->setCardType          ( query.value(5).toString()    );
        pCaptureCard->setDefaultInput      ( query.value(6).toString()    );
        pCaptureCard->setAudioRateLimit    ( query.value(7).toUInt()      );
        pCaptureCard->setHostName          ( query.value(8).toString()    );
        pCaptureCard->setDVBSWFilter       ( query.value(9).toUInt()      );
        pCaptureCard->setDVBSatType        ( query.value(10).toUInt()     );
        pCaptureCard->setDVBWaitForSeqStart( query.value(11).toBool()     );
        pCaptureCard->setSkipBTAudio       ( query.value(12).toBool()     );
        pCaptureCard->setDVBOnDemand       ( query.value(13).toBool()     );
        pCaptureCard->setDVBDiSEqCType     ( query.value(14).toUInt()     );
        pCaptureCard->setFirewireSpeed     ( query.value(15).toUInt()     );
        pCaptureCard->setFirewireModel     ( query.value(16).toString()   );
        pCaptureCard->setFirewireConnection( query.value(17).toUInt()     );
        pCaptureCard->setSignalTimeout     ( query.value(18).toUInt()     );
        pCaptureCard->setChannelTimeout    ( query.value(19).toUInt()     );
        pCaptureCard->setDVBTuningDelay    ( query.value(20).toUInt()     );
        pCaptureCard->setContrast          ( query.value(21).toUInt()     );
        pCaptureCard->setBrightness        ( query.value(22).toUInt()     );
        pCaptureCard->setColour            ( query.value(23).toUInt()     );
        pCaptureCard->setHue               ( query.value(24).toUInt()     );
        pCaptureCard->setDiSEqCId          ( query.value(25).toUInt()     );
        pCaptureCard->setDVBEITScan        ( query.value(26).toBool()     );
        pCaptureCard->setInputName         ( query.value(27).toString()   );
        pCaptureCard->setSourceId          ( query.value(28).toInt()      );
        pCaptureCard->setExternalCommand   ( query.value(29).toString()   );
        pCaptureCard->setChangerDevice     ( query.value(30).toString()   );
        pCaptureCard->setChangerModel      ( query.value(31).toString()   );
        pCaptureCard->setTuneChannel       ( query.value(32).toString()   );
        pCaptureCard->setStartChannel      ( query.value(33).toString()   );
        pCaptureCard->setDisplayName       ( query.value(34).toString()   );
        pCaptureCard->setDishnetEit        ( query.value(35).toBool()     );
        pCaptureCard->setRecPriority       ( query.value(36).toInt()      );
        pCaptureCard->setQuickTune         ( query.value(37).toBool()     );
        pCaptureCard->setSchedOrder        ( query.value(38).toUInt()     );
        pCaptureCard->setLiveTVOrder       ( query.value(39).toUInt()     );
        pCaptureCard->setRecLimit          ( query.value(40).toUInt()     );
        pCaptureCard->setSchedGroup        ( query.value(41).toBool()     );
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

    bool bResult = CardUtil::DeleteInput(nCardId);

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
                      sFirewireModel, nFirewireConnection, std::chrono::milliseconds(nSignalTimeout),
                      std::chrono::milliseconds(nChannelTimeout), nDVBTuningDelay, nContrast, nBrightness,
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

    return set_on_input(sSetting, nCardId, sValue);
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
    if ( nCardId < 1 || nSourceId < 1 ||
         sInputName.isEmpty() || sInputName == "None" )
        throw( QString( "This API requires at least a card ID, a source ID, "
                        "and an input name." ));

    if ( !CardUtil::IsUniqueDisplayName(sDisplayName, 0 ))
        throw QString(" DisplayName is not set or is not unique.");

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
