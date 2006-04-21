// -*- Mode: c++ -*-
#ifndef _CARDUTIL_H_
#define _CARDUTIL_H_

#include "libmyth/settings.h"

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qstringlist.h>
#include <qmap.h>

class CardInput;
typedef QMap<int,QString> InputNames;

class DVBDiSEqCInput
{
  public:
    DVBDiSEqCInput() { clearValues(); }
    DVBDiSEqCInput(const QString &in, const QString &prt, const QString &pos)
        : input(in), port(prt), position(pos) {}

    void clearValues(void) { input = port = position = ""; }

    QString input;
    QString port;
    QString position;
};
typedef QValueList<DVBDiSEqCInput> DiSEqCList;

/// \brief all the different dvb DiSEqC devices
enum DISEQC_TYPES
{
    DISEQC_SINGLE                  = 0,
    DISEQC_MINI_2                  = 1,
    DISEQC_SWITCH_2_1_0            = 2,
    DISEQC_SWITCH_2_1_1            = 3,
    DISEQC_SWITCH_4_1_0            = 4,
    DISEQC_SWITCH_4_1_1            = 5,
    DISEQC_POSITIONER_1_2          = 6,
    DISEQC_POSITIONER_X            = 7,
    DISEQC_POSITIONER_1_2_SWITCH_2 = 8,
    DISEQC_POSITIONER_X_SWITCH_2   = 9,
    DISEQC_SW21                    = 10,
    DISEQC_SW64                    = 11,
};

QString get_on_source(const QString&, uint, uint);
QString get_on_input(const QString&, uint, const QString&);

/** \class CardUtil
 *  \brief Collection of helper utilities for capture card DB use
 */
class CardUtil
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
        return ERROR_UNKNOWN;
    }

    static int          GetCardID(const QString &videodevice,
                                  QString hostname = QString::null);
    static uint         GetChildCardID(uint cardid);
    static uint         GetParentCardID(uint cardid);

    static bool         IsCardTypePresent(const QString &strType);

    static QString      GetRawCardType(uint cardid, uint sourceid)
        { return get_on_source("cardtype", cardid, sourceid); }
    static QString      GetVideoDevice(uint cardid, uint sourceid)
        { return get_on_source("videodevice", cardid, sourceid); }
    static bool         GetVBIDevice(uint cardid, uint sourceid)
        { return get_on_source("vbidevice", cardid, sourceid); }
    static uint         GetDBOX2Port(uint cardid, uint sourceid)
        { return get_on_source("dbox2_port", cardid, sourceid).toUInt(); }
    static uint         GetHDHRTuner(uint cardid, uint sourceid)
        { return get_on_source("dbox2_port", cardid, sourceid).toUInt(); }

    static QString      GetRawCardType(uint cardid, const QString &input)
        { return get_on_input("cardtype", cardid, input); }
    static QString      GetVideoDevice(uint cardid, const QString &input)
        { return get_on_input("videodevice", cardid, input); }
    static QString      GetVBIDevice(uint cardid, const QString &input)
        { return get_on_input("vbidevice", cardid, input); }

    static QString      GetDefaultInput(uint cardid);
    static QString      GetInputName(uint cardid, uint sourceid);

    static QString      GetDeviceLabel(uint    cardid,
                                       QString cardtype,
                                       QString videodevice);

    static QString      ProbeSubTypeName(uint cardid, const QString &input);

    static QStringList  probeInputs(QString device,
                                    QString cardtype = QString::null,
                                    int diseqctype = -1);
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

    // DVB info
    static bool         IsDVB(uint cardid, const QString &_inputname);
    static bool         IsDVBCardType(const QString card_type);
    static QString      ProbeDVBFrontendName(uint device);
    static QString      ProbeDVBType(uint device);
    static bool         HasDVBCRCBug(uint device);
    static uint         GetMinSignalMonitoringDelay(uint device);
    static DISEQC_TYPES GetDISEqCType(uint cardid);

    // V4L info
    static bool         hasV4L2(int videofd);
    static InputNames   probeV4LInputs(int videofd, bool &ok);

  private:
    static QStringList  probeV4LInputs(QString device);
    static QStringList  probeDVBInputs(QString device, int diseqctype = -1);
    static QStringList  probeChildInputs(QString device);

    static QStringList  fillDVBInputs(int dvb_diseqc_type);
    static DiSEqCList   fillDVBInputsDiSEqC(int dvb_diseqc_type);
};

#endif //_CARDUTIL_H_
