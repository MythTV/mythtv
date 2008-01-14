// -*- Mode: c++ -*-
#ifndef _CARDUTIL_H_
#define _CARDUTIL_H_

#include "libmyth/settings.h"

// C++ headers
#include <stdint.h>
#include <vector>
using namespace std;

// Qt headers
#include <qstringlist.h>
#include <qmap.h>

class InputInfo;
class CardInput;
typedef QMap<int,QString> InputNames;
typedef vector<QString>   QStringVec;

QString get_on_cardid(const QString&, uint);

bool set_on_source(const QString&, uint, uint, const QString);

typedef enum
{
    DVB_DEV_FRONTEND = 1,
    DVB_DEV_DVR,
    DVB_DEV_DEMUX,
    DVB_DEV_CA,
    DVB_DEV_AUDIO,
    DVB_DEV_VIDEO,
} dvb_dev_type_t;

/** \class CardUtil
 *  \brief Collection of helper utilities for capture card DB use
 */
class MPUBLIC CardUtil
{
  public:
    /// \brief all the different capture cards
    enum CARD_TYPES
    {
        ERROR_OPEN = 0,
        ERROR_UNKNOWN,
        ERROR_PROBE,
        QPSK,
        QAM,
        OFDM,
        ATSC,
        V4L,
        MPEG,
        FIREWIRE,
        HDHOMERUN,
        FREEBOX,
    };

    static enum CARD_TYPES toCardType(const QString &name)
    {
        if ("ERROR_OPEN" == name)
            return ERROR_OPEN;
        if ("ERROR_UNKNOWN" == name)
            return ERROR_UNKNOWN;
        if ("ERROR_PROBE" == name)
            return ERROR_PROBE;
        if ("QPSK" == name)
            return QPSK;
        if ("QAM" == name)
            return QAM;
        if ("OFDM" == name)
            return OFDM;
        if ("ATSC" == name)
            return ATSC;
        if ("V4L" == name)
            return V4L;
        if ("MPEG" == name)
            return MPEG;
        if ("FIREWIRE" == name)
            return FIREWIRE;
        if ("HDHOMERUN" == name)
            return HDHOMERUN;
        if ("FREEBOX" == name)
            return FREEBOX;
        return ERROR_UNKNOWN;
    }

    static bool         IsEncoder(const QString &rawtype)
    {
        return
            (rawtype != "DVB")       &&
            (rawtype != "FIREWIRE")  && (rawtype != "DBOX2")   &&
            (rawtype != "HDHOMERUN") && (rawtype != "FREEBOX");
    }

    static bool         IsUnscanable(const QString &rawtype)
    {
        return
            (rawtype == "FIREWIRE")  || (rawtype == "DBOX2");
    }

    static bool         IsEITCapable(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN");
    }

    static bool         IsTunerSharingCapable(const QString &rawtype)
    {
        return (rawtype == "DVB");
    }

    static bool         IsTunerShared(uint cardidA, uint cardidB);

    static bool         IsTuningDigital(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN");
    }

    static bool         IsTuningAnalog(const QString &rawtype)
    {
        return (rawtype == "V4L");
    }

    /// Convenience function for GetCardIDs(const QString&, QString, QString)
    static uint         GetFirstCardID(const QString &videodevice)
    {
        vector<uint> list = GetCardIDs(videodevice);
        if (list.empty())
            return 0;
        return list[0];
    }

    static vector<uint> GetCardIDs(const QString &videodevice,
                                   QString rawtype  = QString::null,
                                   QString hostname = QString::null);

    static bool         IsCardTypePresent(const QString &rawtype,
                                          QString hostname = QString::null);
    static QStringVec   GetVideoDevices(const QString &rawtype,
                                        QString hostname = QString::null);

    static QString      GetRawCardType(uint cardid)
        { return get_on_cardid("cardtype", cardid).upper(); }
    static QString      GetVideoDevice(uint cardid)
        { return get_on_cardid("videodevice", cardid); }
    static QString      GetVBIDevice(uint cardid)
        { return get_on_cardid("vbidevice", cardid); }
    static uint         GetHDHRTuner(uint cardid)
        { return get_on_cardid("dbox2_port", cardid).toUInt(); }

    static int          GetValueInt(const QString &col, uint cid)
        { return get_on_cardid(col, cid).toInt(); }
    static bool         SetValue(const QString &col, uint cid,
                                 uint sid, int val)
        { return set_on_source(col, cid, sid, QString::number(val)); }
    static bool         SetValue(const QString &col, uint cid,
                                 uint sid, const QString &val)
        { return set_on_source(col, cid, sid, val); }

    // Inputs
    static vector<uint> GetCardIDs(uint sourceid);
    static QString      GetDefaultInput(uint cardid);
    static QStringList  GetInputNames(uint cardid, uint sourceid);
    static bool         GetInputInfo(InputInfo &info,
                                     vector<uint> *groupids = NULL);
    static uint         GetCardID(uint inputid);
    static QString      GetInputName(uint inputid);
    static QString      GetDisplayName(uint inputid);
    static QString      GetDisplayName(uint cardid, const QString &inputname);
    static vector<uint> GetInputIDs(uint cardid);
    static bool         DeleteInput(uint inputid);
    static bool         DeleteOrphanInputs(void);

    // Input Groups
    static uint         CreateInputGroup(const QString &name);
    static bool         CreateInputGroupIfNeeded(uint cardid);
    static bool         LinkInputGroup(uint inputid, uint inputgroupid);
    static bool         UnlinkInputGroup(uint inputid, uint inputgroupid);
    static vector<uint> GetInputGroups(uint inputid);
    static vector<uint> GetSharedInputGroups(uint cardid);
    static vector<uint> GetGroupCardIDs(uint inputgroupid);
    static vector<uint> GetConflictingCards(uint inputid, uint exclude_cardid);

    static QString      GetDeviceLabel(uint    cardid,
                                       QString cardtype,
                                       QString videodevice);

    static QString      ProbeSubTypeName(uint cardid);

    static QStringList  probeInputs(QString device,
                                    QString cardtype = QString::null);
    static void         GetCardInputs(uint                cardid,
                                      const QString      &device,
                                      const QString      &cardtype,
                                      QStringList        &inputLabels,
                                      vector<CardInput*> &cardInputs);

    static bool         DeleteCard(uint cardid);
    static bool         DeleteAllCards(void);
    static vector<uint> GetCardList(void);

    // General info from OS
    static QStringVec   ProbeVideoDevices(const QString &rawtype);

    // Other
    static bool         CloneCard(uint src_cardid, uint dst_cardid);
    static vector<uint> GetCloneCardIDs(uint cardid);

    // DTV info
    static bool         GetTimeouts(uint cardid,
                                    uint &signal_timeout,
                                    uint &channel_timeout);
    static bool         IgnoreEncrypted(uint cardid, const QString &inputname);
    static bool         TVOnly(uint cardid, const QString &inputname);
    static bool         IsInNeedOfExternalInputConf(uint cardid);
    static uint         GetQuickTuning(uint cardid, const QString &inputname);

    // DVB info
    /// \brief Returns true if the card is a DVB card
    static bool         IsDVB(uint cardid)
        { return "DVB" == GetRawCardType(cardid); }
    static bool         IsDVBCardType(const QString card_type);
    static QString      ProbeDVBFrontendName(uint device);
    static QString      ProbeDVBType(uint device);
    static bool         HasDVBCRCBug(uint device);
    static uint         GetMinSignalMonitoringDelay(uint device);
    static QString      GetDeviceName(dvb_dev_type_t, uint cardnum);
    static InputNames   GetConfiguredDVBInputs(uint cardid);

    // V4L info
    static bool         hasV4L2(int videofd);
    static bool         GetV4LInfo(int videofd, QString &card, QString &driver,
                                   uint32_t &version);
    static bool         GetV4LInfo(int videofd, QString &card, QString &driver)
        { uint32_t dummy; return GetV4LInfo(videofd, card, driver, dummy); }
    static InputNames   probeV4LInputs(int videofd, bool &ok);

  private:
    static QStringList  probeV4LInputs(QString device);
    static QStringList  probeDVBInputs(QString device);
};

#endif //_CARDUTIL_H_
