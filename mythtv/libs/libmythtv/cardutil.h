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
        ERROR_PROBE,
        QPSK,
        QAM,
        OFDM,
        ATSC,
        V4L,
        MPEG,
        HDTV,
        FIREWIRE,
    };

    static int          GetCardID(const QString &videodevice,
                                  QString hostname = QString::null);
    static uint         GetChildCardID(uint cardid);
    static uint         GetParentCardID(uint cardid);

    static bool         IsCardTypePresent(const QString &strType);

    static CARD_TYPES   GetCardType(uint cardid, QString &name,
                                    QString &card_type);
    static CARD_TYPES   GetCardType(uint cardid, QString &name);
    static CARD_TYPES   GetCardType(uint cardid);

    static bool         GetVideoDevice(uint cardid, QString& device,
                                       QString& vbi);
    static QString      GetVideoDevice(uint cardid);
    static QString      GetVideoDevice(uint cardid, uint sourceid);

    static QString      GetDefaultInput(uint cardid);
    static QString      GetInputName(uint cardid, uint sourceid);

    static QString      GetDeviceLabel(uint    cardid,
                                       QString cardtype,
                                       QString videodevice);

    static QStringList  probeInputs(QString device,
                                    QString cardtype = QString::null,
                                    int diseqctype = -1);
    static void         GetCardInputs(int                 cardid,
                                      QString             device,
                                      QString             cardtype,
                                      QStringList        &inputLabels,
                                      vector<CardInput*> &cardInputs,
                                      int                 parentid = 0);
    // DTV info
    static bool         GetTimeouts(uint cardid,
                                    uint &signal_timeout,
                                    uint &channel_timeout);
    static bool         IgnoreEncrypted(uint cardid, const QString &inputname);
    static bool         TVOnly(uint cardid, const QString &inputname);

    // DVB info
    static bool         IsDVB(uint cardid);
    static bool         IsDVBCardType(const QString card_type);
    static CARD_TYPES   GetDVBType(uint device, QString &name,
                                   QString &card_type);
    static bool         HasDVBCRCBug(uint device);
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
