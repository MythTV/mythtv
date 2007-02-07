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

class CardInput;
typedef QMap<int,QString> InputNames;

QString get_on_source(const QString&, uint, uint);
QString get_on_input(const QString&, uint, const QString&);

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
        HDTV,
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
        if ("HDTV" == name)
            return HDTV;
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
            (rawtype != "DVB")       && (rawtype != "HDTV")    &&
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
            (rawtype == "DVB")       || (rawtype == "HDTV")  ||
            (rawtype == "HDHOMERUN");
    }

    static int          GetCardID(const QString &videodevice,
                                  QString hostname = QString::null);
    static uint         GetChildCardID(uint cardid);
    static uint         GetParentCardID(uint cardid);

    static bool         IsCardTypePresent(const QString &strType);

    static QString      GetRawCardType(uint cardid, const QString &input)
        { return get_on_input("cardtype", cardid, input).upper(); }
    static QString      GetVideoDevice(uint cardid, const QString &input)
        { return get_on_input("videodevice", cardid, input); }
    static QString      GetVBIDevice(uint cardid, const QString &input)
        { return get_on_input("vbidevice", cardid, input); }
    static uint         GetHDHRTuner(uint cardid, const QString &input)
        { return get_on_input("dbox2_port", cardid, input).toUInt(); }

    static int          GetValueInt(const QString &col, uint cid, uint sid)
        { return get_on_source(col, cid, sid).toInt(); }
    static bool         SetValue(const QString &col, uint cid,
                                 uint sid, int val)
        { return set_on_source(col, cid, sid, QString::number(val)); }
    static bool         SetValue(const QString &col, uint cid,
                                 uint sid, const QString &val)
        { return set_on_source(col, cid, sid, val); }


    static QString      GetDefaultInput(uint cardid);
    static QStringList  GetInputNames(uint cardid, uint sourceid);

    static QString      GetDeviceLabel(uint    cardid,
                                       QString cardtype,
                                       QString videodevice);

    static QString      ProbeSubTypeName(uint cardid, const QString &input);

    static QStringList  probeInputs(QString device,
                                    QString cardtype = QString::null);
    static void         GetCardInputs(int                 cardid,
                                      QString             device,
                                      QString             cardtype,
                                      QStringList        &inputLabels,
                                      vector<CardInput*> &cardInputs,
                                      int                 parentid = 0);

    static bool         DeleteCard(uint cardid);

    // DTV info
    static bool         GetTimeouts(uint cardid,
                                    uint &signal_timeout,
                                    uint &channel_timeout);
    static bool         IgnoreEncrypted(uint cardid, const QString &inputname);
    static bool         TVOnly(uint cardid, const QString &inputname);
    static bool         IsInNeedOfExternalInputConf(uint cardid);
    static uint         GetQuickTuning(uint cardid, const QString &inputname);

    // DVB info
    static bool         IsDVB(uint cardid, const QString &_inputname);
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
    static QStringList  probeChildInputs(QString device);
};

#endif //_CARDUTIL_H_
