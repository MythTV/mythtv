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
#include "libmythui/standardsettings.h"
#include "libmythbase/compat.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythversion.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/recordingprofile.h"
#include "libmythtv/recorders/satiputils.h"

// MythBackend
#include "v2capture.h"
#include "v2serviceUtil.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (CAPTURE_HANDLE, V2Capture::staticMetaObject, &V2Capture::RegisterCustomTypes))

void V2Capture::RegisterCustomTypes()
{
    qRegisterMetaType<V2CaptureCardList*>("V2CaptureCardList");
    qRegisterMetaType<V2CaptureCard*>("V2CaptureCard");
    qRegisterMetaType<V2CardTypeList*>("V2CardTypeList");
    qRegisterMetaType<V2CardType*>("V2CardType");
    qRegisterMetaType<V2CaptureDeviceList*>("V2CaptureDeviceList");
    qRegisterMetaType<V2CaptureDevice*>("V2CaptureDevice");
    qRegisterMetaType<V2DiseqcTree*>("V2DiseqcTree");
    qRegisterMetaType<V2DiseqcTreeList*>("V2DiseqcTreeList");
    qRegisterMetaType<V2InputGroupList*>("V2InputGroupList");
    qRegisterMetaType<V2InputGroup*>("V2InputGroup");
    qRegisterMetaType<V2DiseqcConfig*>("V2DiseqcConfig");
    qRegisterMetaType<V2DiseqcConfigList*>("V2DiseqcConfigList");
    qRegisterMetaType<V2RecProfParam*>("V2RecProfParam");
    qRegisterMetaType<V2RecProfile*>("V2RecProfile");
    qRegisterMetaType<V2RecProfileGroup*>("V2RecProfileGroup");
    qRegisterMetaType<V2RecProfileGroupList*>("V2RecProfileGroupList");
    qRegisterMetaType<V2CardSubType*>("V2CardSubType");
}

V2Capture::V2Capture()
  : MythHTTPService(s_service)
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2CaptureCardList* V2Capture::GetCaptureCardList( const QString &sHostName,
                                                  const QString &CardType )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "V2Capture Cards."));

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
    else if (!CardType.isEmpty())
        str += " WHERE cardtype = :CARDTYPE";

    if (!sHostName.isEmpty() && !CardType.isEmpty())
        str += " AND cardtype = :CARDTYPE";

    query.prepare(str);

    if (!sHostName.isEmpty())
        query.bindValue(":HOSTNAME", sHostName);
    if (!CardType.isEmpty())
        query.bindValue(":CARDTYPE", CardType);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetCaptureCardList()", query);
        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto* pList = new V2CaptureCardList();

    while (query.next())
    {

        V2CaptureCard *pCaptureCard = pList->AddNewCaptureCard();

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

V2CaptureCard* V2Capture::GetCaptureCard( int nCardId )
{
    if ( nCardId < 1 )
        throw( QString( "The Card ID is invalid."));

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "V2Capture Cards."));

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

    auto* pCaptureCard = new V2CaptureCard();

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

V2CardSubType* V2Capture::GetCardSubType     ( int CardId     )
{
    auto* pCardType = new V2CardSubType();
    QString subtype = CardUtil::ProbeSubTypeName(CardId);
    CardUtil::INPUT_TYPES cardType = CardUtil::toInputType(subtype);

#ifdef USING_SATIP
    if (cardType == CardUtil::INPUT_TYPES::SATIP)
        cardType = SatIP::toDVBInputType(CardUtil::GetVideoDevice(CardId));
#endif // USING_SATIP

    bool HDHRdoesDVBC = false;
    bool HDHRdoesDVB = false;

#ifdef USING_HDHOMERUN
    if (cardType == CardUtil::INPUT_TYPES::HDHOMERUN)
    {
        QString device = CardUtil::GetVideoDevice(CardId);
        HDHRdoesDVBC = CardUtil::HDHRdoesDVBC(device);
        HDHRdoesDVB = CardUtil::HDHRdoesDVB(device);
    }
#endif // USING_HDHOMERUN

    pCardType->setCardId(CardId);
    pCardType->setSubType (subtype);
    // This enum is handled differently from others because it has
    // two names for the same value in a few cases. We choose
    // the name that starts with "DV" when this happens
    QMetaEnum meta = QMetaEnum::fromType<CardUtil::INPUT_TYPES>();
    QString key = meta.valueToKeys(static_cast<uint>(cardType));
    QStringList keyList = key.split("|");
    key = keyList[0];
    if (keyList.length() > 1 && keyList[1].startsWith("DV"))
        key = keyList[1];
    // pCardType->setInputType (cardType);
    pCardType->setInputType (key);
    pCardType->setHDHRdoesDVBC (HDHRdoesDVBC);
    pCardType->setHDHRdoesDVB (HDHRdoesDVB);
    return pCardType;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Capture::RemoveAllCaptureCards( void )
{
    return CardUtil::DeleteAllInputs();
}

bool V2Capture::RemoveCaptureCard( int nCardId )
{
    if ( nCardId < 1 )
        throw( QString( "The Card ID is invalid."));

    bool bResult = CardUtil::DeleteInput(nCardId);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int V2Capture::AddCaptureCard     ( const QString    &sVideoDevice,
                                  const QString    &sAudioDevice,
                                  const QString    &sVBIDevice,
                                  const QString    &CardType,
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
    if ( sVideoDevice.isEmpty() || CardType.isEmpty() || sHostName.isEmpty() )
        throw( QString( "This API requires at least a video device node, a card type, "
                        "and a hostname." ));

    int nResult = CardUtil::CreateCaptureCard(sVideoDevice, sAudioDevice,
                      sVBIDevice, CardType, nAudioRateLimit,
                      sHostName, nDVBSWFilter, nDVBSatType, bDVBWaitForSeqStart,
                      bSkipBTAudio, bDVBOnDemand, nDVBDiSEqCType, nFirewireSpeed,
                      sFirewireModel, nFirewireConnection, std::chrono::milliseconds(nSignalTimeout),
                      std::chrono::milliseconds(nChannelTimeout), nDVBTuningDelay, nContrast, nBrightness,
                      nColour, nHue, nDiSEqCId, bDVBEITScan);

    if ( nResult < 1 )
        throw( QString( "Unable to create capture device." ));

    return nResult;
}

bool V2Capture::UpdateCaptureCard  ( int              nCardId,
                                   const QString    &sSetting,
                                   const QString    &sValue )
{
    if ( nCardId < 1 || sSetting.isEmpty() || sValue.isEmpty() )
        throw( QString( "Card ID, Setting Name, and Value are required." ));

    return set_on_input(sSetting, nCardId, sValue);
}

// Card Inputs

bool V2Capture::RemoveCardInput( int nCardInputId )
{
    if ( nCardInputId < 1 )
        throw( QString( "The Input ID is invalid."));

    bool bResult = CardUtil::DeleteInput(nCardInputId);

    return bResult;
}

int V2Capture::AddCardInput       ( const uint nCardId,
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

bool V2Capture::UpdateCardInput    ( int              nCardInputId,
                                   const QString    &sSetting,
                                   const QString    &sValue )
{
    if ( nCardInputId < 1 || sSetting.isEmpty() || sValue.isEmpty() )
        throw( QString( "Input ID, Setting Name, and Value are required." ));

    return set_on_input(sSetting, nCardInputId, sValue);
}

V2CardTypeList*  V2Capture::GetCardTypeList ( )
{
    auto* pCardTypeList = new V2CardTypeList();

#ifdef USING_DVB
    pCardTypeList->AddCardType(
        QObject::tr("DVB-T/S/C, ATSC or ISDB-T tuner card"), "DVB");
#endif // USING_DVB

#ifdef USING_V4L2
    pCardTypeList->AddCardType(
        QObject::tr("V4L2 encoder"), "V4L2ENC");
    pCardTypeList->AddCardType(
        QObject::tr("HD-PVR H.264 encoder"), "HDPVR");
#endif // USING_V4L2

#ifdef USING_HDHOMERUN
    pCardTypeList->AddCardType(
        QObject::tr("HDHomeRun networked tuner"), "HDHOMERUN");
#endif // USING_HDHOMERUN

#ifdef USING_SATIP
    pCardTypeList->AddCardType(
        QObject::tr("Sat>IP networked tuner"), "SATIP");
#endif // USING_SATIP

#ifdef USING_VBOX
    pCardTypeList->AddCardType(
        QObject::tr("V@Box TV Gateway networked tuner"), "VBOX");
#endif // USING_VBOX

#ifdef USING_FIREWIRE
    pCardTypeList->AddCardType(
        QObject::tr("FireWire cable box"), "FIREWIRE");
#endif // USING_FIREWIRE

#ifdef USING_CETON
    pCardTypeList->AddCardType(
        QObject::tr("Ceton Cablecard tuner"), "CETON");
#endif // USING_CETON

#ifdef USING_IPTV
    pCardTypeList->AddCardType(QObject::tr("IPTV recorder"), "FREEBOX");
#endif // USING_IPTV

#ifdef USING_V4L2
    pCardTypeList->AddCardType(
        QObject::tr("Analog to MPEG-2 encoder card (PVR-150/250/350, etc)"), "MPEG");
    pCardTypeList->AddCardType(
        QObject::tr("Analog to MJPEG encoder card (Matrox G200, DC10, etc)"), "MJPEG");
    pCardTypeList->AddCardType(
        QObject::tr("Analog to MPEG-4 encoder (Plextor ConvertX USB, etc)"),
        "GO7007");
    pCardTypeList->AddCardType(
        QObject::tr("Analog capture card"), "V4L");
#endif // USING_V4L2

#ifdef USING_ASI
    pCardTypeList->AddCardType(QObject::tr("DVEO ASI recorder"), "ASI");
#endif

    pCardTypeList->AddCardType(QObject::tr("Import test recorder"), "IMPORT");
    pCardTypeList->AddCardType(QObject::tr("Demo test recorder"),   "DEMO");
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    pCardTypeList->AddCardType(QObject::tr("External (black box) recorder"),
                          "EXTERNAL");
#endif
    return pCardTypeList;
}

V2InputGroupList*  V2Capture::GetUserInputGroupList ( void )
{
    auto* pInputGroupList = new V2InputGroupList();

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Input Groups."));

    QString q = "SELECT cardinputid,inputgroupid,inputgroupname "
                "FROM inputgroup WHERE inputgroupname LIKE 'user:%' "
                "ORDER BY inputgroupid, cardinputid";
    query.prepare(q);

    if (!query.exec())
    {
        MythDB::DBError("GetUserInputGroupList", query);
        return pInputGroupList;
    }

    while (query.next())
    {
        pInputGroupList->AddInputGroup(query.value(0).toUInt(),
                                       query.value(1).toUInt(),
                                       query.value(2).toString());
    }

    return pInputGroupList;
}

int V2Capture::AddUserInputGroup(const QString & Name)
{
    if (Name.isEmpty())
        throw( QString( "Input group name cannot be empty." ) );

    QString new_name = QString("user:") + Name;

    uint inputgroupid = CardUtil::CreateInputGroup(new_name);

    if (inputgroupid == 0)
    {
        throw( QString( "Failed to add or retrieve %1" ).arg(new_name) );
    }

    return inputgroupid;
}

bool V2Capture::LinkInputGroup(const uint InputId,
                               const uint InputGroupId)
{
    if (!CardUtil::LinkInputGroup(InputId, InputGroupId))
        throw( QString ( "Failed to link input %1 to group %2" )
               .arg(InputId).arg(InputGroupId));
    return true;
}


bool V2Capture::UnlinkInputGroup(const uint InputId,
                                 const uint InputGroupId)
{
    if (!CardUtil::UnlinkInputGroup(InputId, InputGroupId))
        throw( QString ( "Failed to unlink input %1 from group %2" )
               .arg(InputId).arg(InputGroupId));
    return true;
}

bool V2Capture::SetInputMaxRecordings( const uint InputId,
                                       const uint Max )
{
    return CardUtil::InputSetMaxRecordings(InputId, Max);
}


#ifdef USING_DVB
static QString remove_chaff(const QString &name)
{
    // Trim off some of the chaff.
    QString short_name = name;
    if (short_name.startsWith("LG Electronics"))
        short_name = short_name.right(short_name.length() - 15);
    if (short_name.startsWith("Oren"))
        short_name = short_name.right(short_name.length() - 5);
    if (short_name.startsWith("Nextwave"))
        short_name = short_name.right(short_name.length() - 9);
    if (short_name.startsWith("frontend", Qt::CaseInsensitive))
        short_name = short_name.left(short_name.length() - 9);
    if (short_name.endsWith("VSB/QAM"))
        short_name = short_name.left(short_name.length() - 8);
    if (short_name.endsWith("VSB"))
        short_name = short_name.left(short_name.length() - 4);
    if (short_name.endsWith("DVB-T"))
        short_name = short_name.left(short_name.length() - 6);

    // It would be infinitely better if DVB allowed us to query
    // the vendor ID. But instead we have to guess based on the
    // demodulator name. This means cards like the Air2PC HD5000
    // and DViCO Fusion HDTV cards are not identified correctly.
    short_name = short_name.simplified();
    if (short_name.startsWith("or51211", Qt::CaseInsensitive))
        short_name = "pcHDTV HD-2000";
    else if (short_name.startsWith("or51132", Qt::CaseInsensitive))
        short_name = "pcHDTV HD-3000";
    else if (short_name.startsWith("bcm3510", Qt::CaseInsensitive))
        short_name = "Air2PC v1";
    else if (short_name.startsWith("nxt2002", Qt::CaseInsensitive) ||
             short_name.startsWith("nxt200x", Qt::CaseInsensitive))
        short_name = "Air2PC v2";
    else if (short_name.startsWith("lgdt3302", Qt::CaseInsensitive))
        short_name = "DViCO HDTV3";
    else if (short_name.startsWith("lgdt3303", Qt::CaseInsensitive))
        short_name = "DViCO v2 or Air2PC v3 or pcHDTV HD-5500";

    return short_name;
}
#endif // USING_DVB



V2CaptureDeviceList* V2Capture::GetCaptureDeviceList  ( const QString  &CardType )
{

    if (CardType == "V4L2ENC") {
        QRegularExpression drv { "^(?!ivtv|hdpvr|(saa7164(.*))).*$" };
        return getV4l2List(drv, CardType);
    }
    if (CardType == "HDPVR") {
        QRegularExpression drv { "^hdpvr$" };
        return getV4l2List(drv, CardType);
    }
    if (CardType == "FIREWIRE") {
        return getFirewireList(CardType);
    }

    // Get devices from system
    QStringList sdevs = CardUtil::ProbeVideoDevices(CardType);

    auto* pList = new V2CaptureDeviceList();
    for (const auto & it : std::as_const(sdevs))
    {
        auto* pDev = pList->AddCaptureDevice();
        pDev->setCardType (CardType);
        pDev->setVideoDevice (it);
#ifdef USING_DVB
        // From DVBConfigurationGroup::probeCard in Videosource.cpp
        if (CardType == "DVB")
        {
            QString frontendName = CardUtil::ProbeDVBFrontendName(it);
            pDev->setInputNames( CardUtil::ProbeDeliverySystems (it));
            pDev->setDefaultInputName( CardUtil::ProbeDefaultDeliverySystem (it));
            QString subType = CardUtil::ProbeDVBType(it);
            int signalTimeout = 0;
            int channelTimeout = 0;
            int tuningDelay = 0;
            QString err_open  = tr("Could not open card %1").arg(it);
            QString err_other = tr("Could not get card info for card %1").arg(it);

            switch (CardUtil::toInputType(subType))
            {
                case CardUtil::INPUT_TYPES::ERROR_OPEN:
                    frontendName = err_open;
                    subType = strerror(errno);
                    break;
                case CardUtil::INPUT_TYPES::ERROR_UNKNOWN:
                    frontendName = err_other;
                    subType = "Unknown error";
                    break;
                case CardUtil::INPUT_TYPES::ERROR_PROBE:
                    frontendName = err_other;
                    subType = strerror(errno);
                    break;
                case CardUtil::INPUT_TYPES::QPSK:
                    subType = "DVB-S";
                    signalTimeout = 7000;
                    channelTimeout = 10000;
                    break;
                case CardUtil::INPUT_TYPES::DVBS2:
                    subType = "DVB-S2";
                    signalTimeout = 7000;
                    channelTimeout = 10000;
                    break;
                case CardUtil::INPUT_TYPES::QAM:
                    subType = "DVB-C";
                    signalTimeout = 3000;
                    channelTimeout = 6000;
                    break;
                case CardUtil::INPUT_TYPES::DVBT2:
                    subType = "DVB-T2";
                    signalTimeout = 3000;
                    channelTimeout = 6000;
                    break;
                case CardUtil::INPUT_TYPES::OFDM:
                {
                    subType = "DVB-T";
                    signalTimeout = 3000;
                    channelTimeout = 6000;
                    if (frontendName.toLower().indexOf("usb") >= 0)
                    {
                        signalTimeout = 40000;
                        channelTimeout = 42500;
                    }

                    // slow down tuning for buggy drivers
                    if ((frontendName == "DiBcom 3000P/M-C DVB-T") ||
                        (frontendName ==
                        "TerraTec/qanu USB2.0 Highspeed DVB-T Receiver"))
                    {
                        tuningDelay = 200;
                    }
                    break;
                }
                case CardUtil::INPUT_TYPES::ATSC:
                {
                    QString short_name = remove_chaff(frontendName);
                    subType = "ATSC";
                    frontendName = short_name;
                    signalTimeout = 2000;
                    channelTimeout = 4000;

                    // According to #1779 and #1935 the AverMedia 180 needs
                    // a 3000 ms signal timeout, at least for QAM tuning.
                    if (frontendName == "Nextwave NXT200X VSB/QAM frontend")
                    {
                        signalTimeout = 3000;
                        channelTimeout = 5500;
                    }
                    break;
                }
                default:
                    break;
            }

            pDev->setFrontendName ( frontendName );
            pDev->setSubType ( subType );
            pDev->setSignalTimeout ( signalTimeout );
            pDev->setChannelTimeout ( channelTimeout );
            pDev->setTuningDelay ( tuningDelay );
        } // endif (CardType == "DVB")
#endif // USING_DVB
        if (CardType == "HDHOMERUN")
        {
            pDev->setSignalTimeout ( 3000 );
            pDev->setChannelTimeout ( 6000 );
        }
#ifdef USING_SATIP
        if (CardType == "SATIP")
        {
            pDev->setSignalTimeout ( 7000 );
            pDev->setChannelTimeout ( 10000 );
            // Split video device into its parts and populate the output
            // "deviceid friendlyname ip tunernum tunertype"
            auto word = it.split(' ');
            // VideoDevice is set as deviceid:tunertype:tunernum
            if (word.size() == 5) {
                pDev->setVideoDevice(QString("%1:%2:%3").arg(word[0], word[4], word[3]));
                pDev->setVideoDevicePrompt(QString("%1, %2, Tuner #%3").arg(word[0], word[4], word[3]));
                pDev->setDescription(word[1]);
                pDev->setIPAddress(word[2]);
                pDev->setTunerType(word[4]);
                pDev->setTunerNumber(word[3].toUInt());
            }
        }
#endif // USING_SATIP
#ifdef USING_VBOX
        if (CardType == "VBOX")
        {
            pDev->setSignalTimeout ( 7000 );
            pDev->setChannelTimeout ( 10000 );
            // Split video device into its parts and populate the output
            // "deviceid ip tunernum tunertype"
            auto word = it.split(" ");
            if (word.size() == 4) {
                QString device = QString("%1-%2-%3").arg(word[0], word[2], word[3]);
                pDev->setVideoDevice(device);
                pDev->setVideoDevicePrompt(device);
                QString desc = CardUtil::GetVBoxdesc(word[0], word[1], word[2], word[3]);
                pDev->setDescription(desc);
                pDev->setIPAddress(word[1]);
                pDev->setTunerType(word[3]);
                pDev->setTunerNumber(word[2].toUInt());
            }
        }
#endif // USING_VBOX
    } // endfor (const auto & it : std::as_const(sdevs))
    return pList;
}



V2DiseqcTreeList* V2Capture::GetDiseqcTreeList  (  )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "DiseqcTrees."));

    QString str =  "SELECT diseqcid, "
                   "parentid, "
                   "ordinal, "
                   "type, "
                   "subtype, "
                   "description, "
                   "switch_ports, "
                   "rotor_hi_speed, "
                   "rotor_lo_speed, "
                   "rotor_positions, "
                   "lnb_lof_switch, "
                   "lnb_lof_hi, "
                   "lnb_lof_lo, "
                   "cmd_repeat, "
                   "lnb_pol_inv, "
                   "address, "
                   "scr_userband, "
                   "scr_frequency, "
                   "scr_pin "
                   "FROM diseqc_tree ORDER BY diseqcid";

    query.prepare(str);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetDiseqcTreeList()", query);
        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto* pList = new V2DiseqcTreeList();

    while (query.next())
    {
        auto *pRec = pList->AddDiseqcTree();
        pRec->setDiSEqCId           ( query.value(  0 ).toUInt() );
        pRec->setParentId           ( query.value(  1 ).toUInt() );
        pRec->setOrdinal            ( query.value(  2 ).toUInt() );
        pRec->setType               ( query.value(  3 ).toString() );
        pRec->setSubType            ( query.value(  4 ).toString() );
        pRec->setDescription        ( query.value(  5 ).toString() );
        pRec->setSwitchPorts        ( query.value(  6 ).toUInt() );
        pRec->setRotorHiSpeed       ( query.value(  7 ).toFloat() );
        pRec->setRotorLoSpeed       ( query.value(  8 ).toFloat() );
        pRec->setRotorPositions     ( query.value(  9 ).toString() );
        pRec->setLnbLofSwitch       ( query.value( 10 ).toInt() );
        pRec->setLnbLofHi           ( query.value( 11 ).toInt() );
        pRec->setLnbLofLo           ( query.value( 12 ).toInt() );
        pRec->setCmdRepeat          ( query.value( 13 ).toInt() );
        pRec->setLnbPolInv          ( query.value( 14 ).toBool() );
        pRec->setAddress            ( query.value( 15 ).toInt() );
        pRec->setScrUserband        ( query.value( 16 ).toUInt() );
        pRec->setScrFrequency       ( query.value( 17 ).toUInt() );
        pRec->setScrPin             ( query.value( 18 ).toInt() );
    }

    return pList;
}

int  V2Capture::AddDiseqcTree ( uint           ParentId,
                                uint           Ordinal,
                                const QString& Type,
                                const QString& SubType,
                                const QString& Description,
                                uint           SwitchPorts,
                                float          RotorHiSpeed,
                                float          RotorLoSpeed,
                                const QString& RotorPositions,
                                int            LnbLofSwitch,
                                int            LnbLofHi,
                                int            LnbLofLo,
                                int            CmdRepeat,
                                bool           LnbPolInv,
                                int            Address,
                                uint           ScrUserband,
                                uint           ScrFrequency,
                                int            ScrPin)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
             "INSERT INTO diseqc_tree "
            "(parentid, "
            "ordinal, "
            "type, "
            "subtype, "
            "description, "
            "switch_ports, "
            "rotor_hi_speed, "
            "rotor_lo_speed, "
            "rotor_positions, "
            "lnb_lof_switch, "
            "lnb_lof_hi, "
            "lnb_lof_lo, "
            "cmd_repeat, "
            "lnb_pol_inv, "
            "address, "
            "scr_userband, "
            "scr_frequency, "
            "scr_pin) "
            "VALUES "
            "(:PARENTID, "
            ":ORDINAL, "
            ":TYPE, "
            ":SUBTYPE, "
            ":DESCRIPTION, "
            ":SWITCH_PORTS, "
            ":ROTOR_HI_SPEED, "
            ":ROTOR_LO_SPEED, "
            ":ROTOR_POSITIONS, "
            ":LNB_LOF_SWITCH, "
            ":LNB_LOF_HI, "
            ":LNB_LOF_LO, "
            ":CMD_REPEAT, "
            ":LNB_POL_INV, "
            ":ADDRESS, "
            ":SCR_USERBAND, "
            ":SCR_FREQUENCY, "
            ":SCR_PIN ) " );

    if (ParentId == 0) // Value 0 is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":PARENTID", QVariant(QVariant::UInt));
#else
        query.bindValue(":PARENTID", QVariant(QMetaType(QMetaType::UInt)));
#endif
    }
    else
    {
        query.bindValue(":PARENTID", ParentId);
    }
    query.bindValue(":ORDINAL", Ordinal);
    query.bindValue(":TYPE", Type);
    query.bindValue(":SUBTYPE", SubType);
    query.bindValue(":DESCRIPTION", Description);
    query.bindValue(":SWITCH_PORTS", SwitchPorts);
    query.bindValue(":ROTOR_HI_SPEED", RotorHiSpeed);
    query.bindValue(":ROTOR_LO_SPEED", RotorLoSpeed);
    query.bindValue(":ROTOR_POSITIONS", RotorPositions);
    query.bindValue(":LNB_LOF_SWITCH", LnbLofSwitch);
    query.bindValue(":LNB_LOF_HI", LnbLofHi);
    query.bindValue(":LNB_LOF_LO", LnbLofLo);
    query.bindValue(":CMD_REPEAT", CmdRepeat);
    query.bindValue(":LNB_POL_INV", LnbPolInv);
    query.bindValue(":ADDRESS", Address);
    query.bindValue(":SCR_USERBAND", ScrUserband);
    query.bindValue(":SCR_FREQUENCY", ScrFrequency);
    query.bindValue(":SCR_PIN", ScrPin);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::AddDiseqcTree()", query);
        throw( QString( "Database Error executing query." ));
    }
    uint DiSEqCId  = query.lastInsertId().toInt();
    return DiSEqCId;
}

bool V2Capture::UpdateDiseqcTree  ( uint           DiSEqCId,
                                    uint           ParentId,
                                    uint           Ordinal,
                                    const QString& Type,
                                    const QString& SubType,
                                    const QString& Description,
                                    uint           SwitchPorts,
                                    float          RotorHiSpeed,
                                    float          RotorLoSpeed,
                                    const QString& RotorPositions,
                                    int            LnbLofSwitch,
                                    int            LnbLofHi,
                                    int            LnbLofLo,
                                    int            CmdRepeat,
                                    bool           LnbPolInv,
                                    int            Address,
                                    uint           ScrUserband,
                                    uint           ScrFrequency,
                                    int            ScrPin)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
            "UPDATE diseqc_tree SET "
            "parentid = :PARENTID, "
            "ordinal = :ORDINAL, "
            "type = :TYPE, "
            "subtype = :SUBTYPE, "
            "description = :DESCRIPTION, "
            "switch_ports = :SWITCH_PORTS, "
            "rotor_hi_speed = :ROTOR_HI_SPEED, "
            "rotor_lo_speed = :ROTOR_LO_SPEED, "
            "rotor_positions = :ROTOR_POSITIONS, "
            "lnb_lof_switch = :LNB_LOF_SWITCH, "
            "lnb_lof_hi = :LNB_LOF_HI, "
            "lnb_lof_lo = :LNB_LOF_LO, "
            "cmd_repeat = :CMD_REPEAT, "
            "lnb_pol_inv = :LNB_POL_INV, "
            "address = :ADDRESS, "
            "scr_userband = :SCR_USERBAND, "
            "scr_frequency = :SCR_FREQUENCY, "
            "scr_pin = :SCR_PIN "
            "WHERE diseqcid = :DISEQCID " );

    query.bindValue(":DISEQCID", DiSEqCId);
    if (ParentId == 0) // Value 0 is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":PARENTID", QVariant(QVariant::UInt));
#else
        query.bindValue(":PARENTID", QVariant(QMetaType(QMetaType::UInt)));
#endif
    }
    else
    {
        query.bindValue(":PARENTID", ParentId);
    }
    query.bindValue(":ORDINAL", Ordinal);
    query.bindValue(":TYPE", Type);
    query.bindValue(":SUBTYPE", SubType);
    query.bindValue(":DESCRIPTION", Description);
    query.bindValue(":SWITCH_PORTS", SwitchPorts);
    query.bindValue(":ROTOR_HI_SPEED", RotorHiSpeed);
    query.bindValue(":ROTOR_LO_SPEED", RotorLoSpeed);
    query.bindValue(":ROTOR_POSITIONS", RotorPositions);
    query.bindValue(":LNB_LOF_SWITCH", LnbLofSwitch);
    query.bindValue(":LNB_LOF_HI", LnbLofHi);
    query.bindValue(":LNB_LOF_LO", LnbLofLo);
    query.bindValue(":CMD_REPEAT", CmdRepeat);
    query.bindValue(":LNB_POL_INV", LnbPolInv);
    query.bindValue(":ADDRESS", Address);
    query.bindValue(":SCR_USERBAND", ScrUserband);
    query.bindValue(":SCR_FREQUENCY", ScrFrequency);
    query.bindValue(":SCR_PIN", ScrPin);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::UpdateDiseqcTree()", query);
        throw( QString( "Database Error executing query." ));
    }
    return true;
}

bool  V2Capture::RemoveDiseqcTree  ( uint DiSEqCId)
{
    // Find and remove children
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT diseqcid FROM diseqc_tree WHERE parentid = :PARENTID ");
    query.bindValue(":PARENTID", DiSEqCId);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::RemoveDiseqcTree()", query);
        throw( QString( "Database Error executing query." ));
    }

    bool childOK = true;
    while (query.next())
    {
        uint child = query.value(  0 ).toUInt();
        childOK = RemoveDiseqcTree(child) && childOK;
    }
    // remove this row
    MSqlQuery query2(MSqlQuery::InitCon());
    query2.prepare("DELETE FROM diseqc_tree WHERE diseqcid = :DISEQCID ");
    query2.bindValue(":DISEQCID", DiSEqCId);
    if (!query2.exec())
    {
        MythDB::DBError("MythAPI::RemoveDiseqcTree()", query2);
        throw( QString( "Database Error executing query." ));
    }
    int numrows = query2.numRowsAffected();
    return numrows > 0 && childOK;
}


V2DiseqcConfigList* V2Capture::GetDiseqcConfigList  ( void )
{

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "DiseqcConfigs."));

    QString str =  "SELECT cardinputid, "
                   "diseqcid, "
                   "value "
                   "FROM diseqc_config ORDER BY cardinputid, diseqcid";

    query.prepare(str);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetDiseqcConfigList()", query);
        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto* pList = new V2DiseqcConfigList();

    while (query.next())
    {
        auto *pRec = pList->AddDiseqcConfig();
        pRec->setCardId             ( query.value(  0 ).toUInt() );
        pRec->setDiSEqCId           ( query.value(  1 ).toUInt() );
        pRec->setValue              ( query.value(  2 ).toString() );
    }
    return pList;
}

bool V2Capture::AddDiseqcConfig  ( uint           CardId,
                                   uint           DiSEqCId,
                                   const QString& Value)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
             "INSERT INTO diseqc_config "
            "(cardinputid, "
            "diseqcid, "
            "value) "
            "VALUES "
            "(:CARDID, "
            ":DISEQCID, "
            ":VALUE) ");

    query.bindValue(":CARDID", CardId);
    query.bindValue(":DISEQCID", DiSEqCId);
    query.bindValue(":VALUE", Value);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::AddDiseqcConfig()", query);
        throw( QString( "Database Error executing query." ));
    }
    return true;
}


bool V2Capture::RemoveDiseqcConfig  ( uint  CardId )
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM diseqc_config WHERE cardinputid = :CARDID ");
    query.bindValue(":CARDID", CardId);
    if (!query.exec())
    {
        MythDB::DBError("MythAPI::RemoveDiseqcConfig()", query);
        throw( QString( "Database Error executing query." ));
    }
    int numrows = query.numRowsAffected();
    return numrows > 0;
}


V2RecProfileGroupList* V2Capture::GetRecProfileGroupList ( uint GroupId, uint ProfileId, bool OnlyInUse ) {

    MSqlQuery query(MSqlQuery::InitCon());

    QString str =
        "SELECT profilegroups.id, profilegroups.name, cardtype, recordingprofiles.id, recordingprofiles.name, "
        "videocodec, audiocodec, "
        "codecparams.name, codecparams.value "
        "FROM profilegroups  "
        "INNER JOIN recordingprofiles on profilegroups.id = recordingprofiles.profilegroup "
        "LEFT OUTER JOIN codecparams on codecparams.profile = recordingprofiles.id ";
    QString where = "WHERE ";
    if (OnlyInUse)
    {
        str.append(where).append("CARDTYPE = 'TRANSCODE' OR (cardtype in (SELECT cardtype FROM capturecard)) ");
        where = "AND ";
    }
    if (GroupId > 0)
    {
        str.append(where).append("profilegroups.id = :GROUPID ");
        where = "AND ";
    }
    if (ProfileId > 0)
    {
        str.append(where).append("recordingprofiles.id = :PROFILEID ");
        where = "AND ";
    }
    str.append(
        // Force TRANSCODE entry to be at the end
        "ORDER BY IF(cardtype = 'TRANSCODE', 9999, profilegroups.id), recordingprofiles.id, codecparams.name ");

    query.prepare(str);

    if (GroupId > 0)
        query.bindValue(":GROUPID", GroupId);
    if (ProfileId > 0)
        query.bindValue(":PROFILEID", ProfileId);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetRecProfileGroupList()", query);
        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto* pList = new V2RecProfileGroupList();

    int prevGroupId = -1;
    int prevProfileId = -1;

    V2RecProfileGroup * pGroup = nullptr;
    V2RecProfile *pProfile = nullptr;

    while (query.next())
    {
        int groupId = query.value(0).toInt();
        if (groupId != prevGroupId)
        {
            pGroup = pList->AddProfileGroup();
            pGroup->setId               ( groupId );
            pGroup->setName             ( query.value(1).toString() );
            pGroup->setCardType         ( query.value(2).toString() );
            prevGroupId = groupId;
        }
        int profileId = query.value(3).toInt();
        // the pGroup != nullptr check is to satisfy clang-tidy.
        // pGroup should never be null unless the groupId on
        // the database is -1, which should not happen.
        if (profileId != prevProfileId && pGroup != nullptr)
        {
            pProfile = pGroup->AddProfile();
            pProfile->setId          ( profileId );
            pProfile->setName        ( query.value(4).toString() );
            pProfile->setVideoCodec  ( query.value(5).toString() );
            pProfile->setAudioCodec  ( query.value(6).toString() );
            prevProfileId = profileId;
        }
        // the pProfile != nullptr check is to satisfy clang-tidy.
        // pProfile should never be null unless the profileId on
        // the database is -1, which should not happen.
        if (!query.isNull(7) && pProfile != nullptr)
        {
            auto *pParam = pProfile->AddParam();
            pParam->setName  ( query.value(7).toString() );
            pParam->setValue ( query.value(8).toString() );
        }
    }

    return pList;
}

int V2Capture::AddRecProfile  ( uint GroupId, const QString& ProfileName,
    const QString& VideoCodec, const QString& AudioCodec )
{
    if (GroupId == 0 || ProfileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString( "AddRecProfile: GroupId and ProfileName are required." ));
        return 0;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    // Check if it already exists
    query.prepare(
        "SELECT id "
            "FROM recordingprofiles "
            "WHERE name = :NAME AND profilegroup = :PROFILEGROUP;");
    query.bindValue(":NAME", ProfileName);
    query.bindValue(":PROFILEGROUP", GroupId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::AddRecProfile", query);
        throw( QString( "Database Error executing SELECT." ));
    }
    if (query.next())
    {
        int id = query.value(0).toInt();
        LOG(VB_GENERAL, LOG_ERR,
            QString( "Profile %1 already exists in group id %2 with id %3").arg(ProfileName).arg(GroupId).arg(id));
        return 0;
    }

    query.prepare(
       "INSERT INTO recordingprofiles "
           "(name, videocodec, audiocodec, profilegroup) "
         "VALUES "
           "(:NAME, :VIDEOCODEC, :AUDIOCODEC, :PROFILEGROUP);");
    query.bindValue(":NAME", ProfileName);
    query.bindValue(":VIDEOCODEC", VideoCodec);
    query.bindValue(":AUDIOCODEC", AudioCodec);
    query.bindValue(":PROFILEGROUP", GroupId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::AddRecProfile", query);
        throw( QString( "Database Error executing INSERT." ));
    }
    int id = query.lastInsertId().toInt();
    RecordingProfile profile(ProfileName);
    profile.loadByID(id);
    profile.setCodecTypes();
    profile.Save();
    return id;
}

bool V2Capture::UpdateRecProfile ( uint ProfileId,
                                   const QString& VideoCodec,
                                   const QString& AudioCodec )
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT id from recordingprofiles "
        "WHERE id = :ID; ");
    query.bindValue(":ID", ProfileId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::UpdateRecProfileParam", query);
        throw( QString( "Database Error executing SELECT." ));
    }
    uint id = -1;
    if (query.next())
    {
        id = query.value(0).toUInt();
    }
    if (id != ProfileId)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UpdateRecProfile: Profile id %1 does not exist").arg(ProfileId));
        return false;
    }
    query.prepare(
        "UPDATE recordingprofiles "
        "SET videocodec = :VIDEOCODEC, "
        "audiocodec = :AUDIOCODEC "
        "WHERE id = :ID; ");
    query.bindValue(":VIDEOCODEC", VideoCodec);
    query.bindValue(":AUDIOCODEC", AudioCodec);
    query.bindValue(":ID", ProfileId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::UpdateRecProfileParam", query);
        throw( QString( "Database Error executing UPDATE." ));
    }
    RecordingProfile profile;
    profile.loadByID(ProfileId);
    profile.setCodecTypes();
    profile.Save();
    return true;
}

bool V2Capture::DeleteRecProfile ( uint ProfileId )
{
    // Delete profile parameters
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE from codecparams "
        "WHERE profile = :ID; ");
    query.bindValue(":ID", ProfileId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::UpdateRecProfileParam", query);
        throw( QString( "Database Error executing DELETE." ));
    }
    int rows = query.numRowsAffected();
    query.prepare(
        "DELETE from recordingprofiles "
        "WHERE id = :ID; ");
    query.bindValue(":ID", ProfileId);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::UpdateRecProfileParam", query);
        throw( QString( "Database Error executing DELETE." ));
    }
    return (rows > 0);
}



bool V2Capture::UpdateRecProfileParam ( uint ProfileId, const QString  &Name, const QString &Value )
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
       "REPLACE INTO codecparams "
           "(profile, name, value) "
         "VALUES "
           "(:PROFILE, :NAME, :VALUE);");
    query.bindValue(":PROFILE", ProfileId);
    query.bindValue(":NAME", Name);
    query.bindValue(":VALUE", Value);
    if (!query.exec())
    {
        MythDB::DBError("V2Capture::UpdateRecProfileParam", query);
        throw( QString( "Database Error executing REPLACE." ));
    }
    return true;
}
