/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _DTVCONFPARSERHELPERS_H_
#define _DTVCONFPARSERHELPERS_H_

#include <QString>

// The following are a set of helper classes to allow easy translation
// between the different string representations of various tuning params.

struct DTVParamHelperStruct
{
    QString symbol;
    int     value;
};

/** \class DTVParamHelper
 *  \brief Helper abstract template to do some of the mundane portions
 *         of translating and comparing the paramater strings.
 */
class DTVParamHelper
{
  public:
    DTVParamHelper(int _value) : value(_value) { }
    DTVParamHelper &operator=(int _value) { value = _value; return *this; }

    operator int()                const { return value;          }
    bool operator==(const int& v) const { return value == v;     }
    bool operator!=(const int& v) const { return value != v;     }

  protected:
    static bool ParseParam(const QString &symbol, int &value,
                           const DTVParamHelperStruct *table);

    static QString toString(const char *strings[], int index,
                            uint strings_size);

  protected:
    int value;
};

class DTVTunerType : public DTVParamHelper
{
    static const DTVParamHelperStruct parseTable[];

  public:
    // WARNING: kTunerTypes cannot be defined by a C++03 enum
    // because gcc 4.3.3 will reportedly promote an enum inconsistently
    // to int on IA-32 platforms. I don't know whether this is
    // correct or not, it comes down to interpretation of section
    // 7.2.5 and whether 0x80000000 should be considered to big
    // for a 32 bit integer or not. Using an enum to represent int
    // bitmasks is valid C code, but the C++03 standard was still a
    // bit loosey gosey on this point. It looks like the breakage
    // was caused by work in gcc to support C++0x which will allow
    // one to specify things as exactly as C does. -- dtk 2009-10-05

    //                                // Modulations which may be supported
    static const int kTunerTypeDVBS1; // QPSK
    static const int kTunerTypeDVBS2; // QPSK, 8PSK, 16APSK, 32APSK
    static const int kTunerTypeDVBC;  // QAM-64, QAM-256
    static const int kTunerTypeDVBT;  // OFDM
    static const int kTunerTypeATSC;  // 8-VSB, 16-VSB,
                                      // QAM-16, QAM-64, QAM-256, QPSK
    static const int kTunerTypeASI;   // baseband
    static const int kTunerTypeOCUR;  // Virtual Channel tuning of QAM-64/256
    static const int kTunerTypeIPTV;  // IPTV
    static const int kTunerTypeUnknown;

    // Note: Just because some cards sold in different regions support the same
    // modulation scheme does not mean that they decode the same signals, there
    // are also region specific FEC algorithms and the tuner which precedes the
    // demodulator may be limited to frequencies used in that specific market.
    // The tuner may also be bandwidth limited to 6 or 7 Mhz, so it could not
    // support the 8 Mhz channels used in some contries, and/or the ADC which
    // sits between the tuner and the demodulator may be bandwidth limited.
    // While often the same hardware could physically support more than it
    // is designed for the card/device maker does not write the firmware
    // but licenses blocks of it and so only selects the pieces absolutely
    // necessary for their market segment. Some ATSC cards only supported
    // 8-VSB, newer cards don't support the unpopular 16-VSB, no consumer
    // card supports the QAM-16 or QPSK used for USA Cable PSIP, etc.
    // DVB-S cards also generally support DiSEqC signaling, and future
    // ATSC cards may support similar but incompatible signalling for
    // pointable antennas.
    //
    // Note 2: These values are keyed to the Linux DVB driver values, in
    // reality some hardware does support multiple formats and this should
    // be a mask. Also the transmission schemes used in Asia and South
    // America are not represented here.

    DTVTunerType(int _default = kTunerTypeUnknown)
        : DTVParamHelper(_default) { initStr(); }

    bool Parse(const QString &_value)
        { return ParseParam(_value, value, parseTable); }

    bool IsFECVariable(void) const
    {
        return ((kTunerTypeDVBC  == value) ||
                (kTunerTypeDVBS1 == value) ||
                (kTunerTypeDVBS2 == value));
    }

    bool IsModulationVariable(void) const
    {
        return ((kTunerTypeDVBC  == value) ||
                (kTunerTypeATSC  == value) ||
                (kTunerTypeDVBS2 == value));
    }

    bool IsDiSEqCSupported(void) const
    {
        return ((kTunerTypeDVBS1 == value) ||
                (kTunerTypeDVBS2 == value));
    }

    QString toString() const { return toString(value); }

    static void initStr(void);
    static QString toString(int _value);
};

class DTVInversion : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 3;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kInversionOff,
        kInversionOn,
        kInversionAuto,
    };

    DTVInversion(int _default = kInversionAuto)
        : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVInversion &other) const
        { return value == other.value || value == kInversionAuto ||
                other.value == kInversionAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }
    QChar   toChar() const
        { if (toString().length() > 0)
              return toString()[0]; else return QChar(0); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVBandwidth : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 4;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kBandwidth8MHz,
        kBandwidth7MHz,
        kBandwidth6MHz,
        kBandwidthAuto,
    };

    DTVBandwidth(int _default = kBandwidthAuto) : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVBandwidth &other) const
        { return value == other.value || value == kBandwidthAuto ||
                other.value == kBandwidthAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }
    QChar   toChar() const
        { if (toString().length() > 0)
              return toString()[0]; else return QChar(0); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVCodeRate : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 12;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kFECNone,
        kFEC_1_2,
        kFEC_2_3,
        kFEC_3_4,
        kFEC_4_5,
        kFEC_5_6,
        kFEC_6_7,
        kFEC_7_8,
        kFEC_8_9,
        kFECAuto,
        kFEC_3_5,
        kFEC_9_10,
    };

    DTVCodeRate(int _default = kFECAuto) : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVCodeRate &other) const
        { return value == other.value || value == kFECAuto ||
                other.value == kFECAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVModulation : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 13;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kModulationQPSK,
        kModulationQAM16,
        kModulationQAM32,
        kModulationQAM64,
        kModulationQAM128,
        kModulationQAM256,
        kModulationQAMAuto,
        kModulation8VSB,
        kModulation16VSB,
        kModulation8PSK,
        kModulation16APSK,
        kModulation32APSK,
        kModulationDQPSK,
        kModulationInvalid = 0x100, /* for removed modulations */
        kModulationAnalog  = 0x200, /* for analog channel scanner */
    };

    DTVModulation(int _default = kModulationQAMAuto)
        : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVModulation &other) const
        { return value == other.value || value == kModulationQAMAuto ||
                other.value == kModulationQAMAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }

    static QString toString(int _value)
    {
        if (kModulationInvalid == _value)
            return "invalid";
        else if (kModulationAnalog == _value)
            return "analog";
        return DTVParamHelper::toString(dbStr, _value, kDBStrCnt);
    }
};

class DTVTransmitMode : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 3;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kTransmissionMode2K,
        kTransmissionMode8K,
        kTransmissionModeAuto,
    };

    DTVTransmitMode(int _default = kTransmissionModeAuto)
        : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVTransmitMode &other) const
        { return value == other.value || value == kTransmissionModeAuto ||
                other.value == kTransmissionModeAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }
    QChar   toChar() const
        { if (toString().length() > 0)
              return toString()[0]; else return QChar(0); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVGuardInterval : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 5;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kGuardInterval_1_32,
        kGuardInterval_1_16,
        kGuardInterval_1_8,
        kGuardInterval_1_4,
        kGuardIntervalAuto,
    };

    DTVGuardInterval(int _default = kGuardIntervalAuto)
        : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVGuardInterval &other) const
        { return value == other.value || value == kGuardIntervalAuto ||
                other.value == kGuardIntervalAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVHierarchy : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 5;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kHierarchyNone,
        kHierarchy1,
        kHierarchy2,
        kHierarchy4,
        kHierarchyAuto,
    };

    DTVHierarchy(int _default = kHierarchyAuto) : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVHierarchy &other) const
        { return value == other.value || value == kHierarchyAuto ||
                other.value == kHierarchyAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }
    QChar   toChar() const
        { if (toString().length() > 0)
              return toString()[0]; else return QChar(0); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVPolarity : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 4;
    static const char *dbStr[kDBStrCnt];

  public:
    enum PolarityValues
    {
        kPolarityVertical,
        kPolarityHorizontal,
        kPolarityRight,
        kPolarityLeft
    };

    DTVPolarity(int _default = kPolarityVertical)
        : DTVParamHelper(_default) { }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, parseTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, parseTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }
    QChar   toChar() const
        { if (toString().length() > 0)
              return toString()[0]; else return QChar(0); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVModulationSystem : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 16;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kModulationSystem_UNDEFINED,
        kModulationSystem_DVBC_ANNEX_AC,
        kModulationSystem_DVBC_ANNEX_B,
        kModulationSystem_DVBT,
        kModulationSystem_DSS,
        kModulationSystem_DVBS,
        kModulationSystem_DVBS2,
        kModulationSystem_DVBH,
        kModulationSystem_ISDBT,
        kModulationSystem_ISDBS,
        kModulationSystem_ISDBC,
        kModulationSystem_ATSC,
        kModulationSystem_ATSCMH,
        kModulationSystem_DMBTH,
        kModulationSystem_CMMB,
        kModulationSystem_DAB,
    };

    DTVModulationSystem(int _default = kModulationSystem_UNDEFINED)
        : DTVParamHelper(_default) { }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

class DTVRollOff : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 4;
    static const char *dbStr[kDBStrCnt];

  public:
    enum
    {
        kRollOff_35,
        kRollOff_20,
        kRollOff_25,
        kRollOff_Auto,
    };

    DTVRollOff(int _default = kRollOff_35)
        : DTVParamHelper(_default) { }

    bool IsCompatible(const DTVRollOff &other) const
        { return value == other.value || value == kRollOff_Auto ||
                other.value == kRollOff_Auto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, value, confTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, value, vdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, value, parseTable); }

    QString toString() const { return toString(value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

#endif // _DTVCONFPARSERHELPERS_H_
