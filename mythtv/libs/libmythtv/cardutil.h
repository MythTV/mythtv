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

class DVBDiSEqCInputList
{
  public:
    DVBDiSEqCInputList() { clearValues(); }
    DVBDiSEqCInputList(const QString& in, const QString& prt, 
                       const QString& pos)
        : input(in), port(prt), position(pos)
    {}

    void clearValues() 
    {
        input = "";
        port = "";
        position = "";
    }

    QString input;
    QString port;
    QString position;
};

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
        ERROR_OPEN,
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
    /// \brief dvb card type
    static const QString DVB;

    static int          GetCardID(const QString &videodevice,
                                  QString hostname = QString::null);

    static bool         IsCardTypePresent(const QString &strType);

    static CARD_TYPES   GetCardType(uint cardid, QString &name, QString &card_type);
    static CARD_TYPES   GetCardType(uint cardid, QString &name);
    static CARD_TYPES   GetCardType(uint cardid);
    static bool         IsDVBCardType(const QString card_type);

    static bool         GetVideoDevice(uint cardid, QString& device, QString& vbi);
    static bool         GetVideoDevice(uint cardid, QString& device);

    static bool         IsDVB(uint cardid);
    static DISEQC_TYPES GetDISEqCType(uint cardid);

    static CARD_TYPES   GetDVBType(uint device, QString &name, QString &card_type);
    static bool         HasDVBCRCBug(uint device);
    static QString      GetDefaultInput(uint cardid);

    static bool         IgnoreEncrypted(uint cardid, const QString &inputname);
    static bool         TVOnly(uint cardid, const QString &inputname);

    static bool         hasV4L2(int videofd);
    static InputNames   probeV4LInputs(int videofd, bool &ok);
};

#endif //_CARDUTIL_H_
