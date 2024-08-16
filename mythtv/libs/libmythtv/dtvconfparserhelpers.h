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

#ifndef DTVCONFPARSERHELPERS_H
#define DTVCONFPARSERHELPERS_H

#include <vector>
#include <QString>
#ifdef USING_DVB
#include <linux/dvb/frontend.h>
#endif

// The following are a set of helper classes to allow easy translation
// between the different string representations of various tuning parameters.

struct DTVParamHelperStruct
{
    QString symbol;
    int     value;
};
using DTVParamHelperVec = std::vector<DTVParamHelperStruct>;
using DTVParamStringVec = std::vector<std::string>;

/** \class DTVParamHelper
 *  \brief Helper abstract template to do some of the mundane portions
 *         of translating and comparing the parameter strings.
 */
class DTVParamHelper
{
  public:
    explicit DTVParamHelper(int _value) : m_value(_value) { }
    DTVParamHelper &operator=(int _value) { m_value = _value; return *this; }

    operator int()                const { return m_value;        }
    bool operator==(const int v)  const { return m_value == v;   }
    bool operator!=(const int v)  const { return m_value != v;   }

  protected:
    static bool ParseParam(const QString &symbol, int &value,
                           const DTVParamHelperVec &table);

    static QString toString(const DTVParamStringVec &strings, int index);

  protected:
    int m_value;
};

class DTVTunerType : public DTVParamHelper
{
    static const DTVParamHelperVec kParseTable;

  public:
    // WARNING: kTunerTypes cannot be defined by a C++03 enum
    // because gcc 4.3.3 will reportedly promote an enum inconsistently
    // to int on IA-32 platforms. I don't know whether this is
    // correct or not, it comes down to interpretation of section
    // 7.2.5 and whether 0x80000000 should be considered too big
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
    static const int kTunerTypeDVBT2; // OFDM
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

    explicit DTVTunerType(int _default = kTunerTypeUnknown)
        : DTVParamHelper(_default) { initStr(); }
    DTVTunerType& operator=(int type) { m_value = type; return *this; }

    bool Parse(const QString &_value)
        { return ParseParam(_value, m_value, kParseTable); }

    bool IsFECVariable(void) const
    {
        return ((kTunerTypeDVBC  == m_value) ||
                (kTunerTypeDVBS1 == m_value) ||
                (kTunerTypeDVBS2 == m_value));
    }

    bool IsModulationVariable(void) const
    {
        return ((kTunerTypeDVBC  == m_value) ||
                (kTunerTypeATSC  == m_value) ||
                (kTunerTypeDVBS2 == m_value));
    }

    bool IsDiSEqCSupported(void) const
    {
        return ((kTunerTypeDVBS1 == m_value) ||
                (kTunerTypeDVBS2 == m_value));
    }

    QString toString() const { return toString(m_value); }

    uint toUInt() const { return static_cast<uint>(m_value); }

    static void initStr(void);
    static QString toString(int _value);
};

class DTVInversion : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kInversionOff,
        kInversionOn,
        kInversionAuto,
    };
#ifdef USING_DVB
    static_assert((kInversionOff  == (Types)INVERSION_OFF ) &&
                  (kInversionOn   == (Types)INVERSION_ON  ) &&
                  (kInversionAuto == (Types)INVERSION_AUTO),
                  "Inversion types don't match DVB includes.");
#endif

    explicit DTVInversion(Types _default = kInversionAuto)
        : DTVParamHelper(_default) { }
    DTVInversion& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVInversion& operator=(const fe_spectral_inversion_t type)
        { m_value = type; return *this; }
#endif

    bool IsCompatible(const DTVInversion other) const
        { return m_value == other.m_value || m_value == kInversionAuto ||
                other.m_value == kInversionAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }
    QChar   toChar() const
        {
            if (toString().length() > 0)
                return toString().at(0);
            return {};
        }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVBandwidth : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kBandwidth8MHz,
        kBandwidth7MHz,
        kBandwidth6MHz,
        kBandwidthAuto,
        kBandwidth5MHz,
        kBandwidth10MHz,
        kBandwidth1712kHz,
    };
#ifdef USING_DVB
    static_assert((kBandwidth8MHz    == (Types)BANDWIDTH_8_MHZ    ) &&
                  (kBandwidth7MHz    == (Types)BANDWIDTH_7_MHZ    ) &&
                  (kBandwidth6MHz    == (Types)BANDWIDTH_6_MHZ    ) &&
                  (kBandwidthAuto    == (Types)BANDWIDTH_AUTO     ) &&
                  (kBandwidth5MHz    == (Types)BANDWIDTH_5_MHZ    ) &&
                  (kBandwidth10MHz   == (Types)BANDWIDTH_10_MHZ   ) &&
                  (kBandwidth1712kHz == (Types)BANDWIDTH_1_712_MHZ),
                  "Bandwidth types don't match DVB includes.");
#endif

    explicit DTVBandwidth(Types _default = kBandwidthAuto)
        : DTVParamHelper(_default) { }
    DTVBandwidth& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVBandwidth& operator=(const fe_bandwidth_t bwidth)
        { m_value = bwidth; return *this; }
#endif

    bool IsCompatible(const DTVBandwidth other) const
        { return m_value == other.m_value || m_value == kBandwidthAuto ||
                other.m_value == kBandwidthAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }
    QChar   toChar() const
        {
            if (toString().length() > 0)
                return toString().at(0);
            return {};
        }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVCodeRate : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
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
#ifdef USING_DVB
    static_assert((kFECNone  == (Types)FEC_NONE) &&
                  (kFEC_1_2  == (Types)FEC_1_2 ) &&
                  (kFEC_2_3  == (Types)FEC_2_3 ) &&
                  (kFEC_3_4  == (Types)FEC_3_4 ) &&
                  (kFEC_4_5  == (Types)FEC_4_5 ) &&
                  (kFEC_5_6  == (Types)FEC_5_6 ) &&
                  (kFEC_6_7  == (Types)FEC_6_7 ) &&
                  (kFEC_7_8  == (Types)FEC_7_8 ) &&
                  (kFEC_8_9  == (Types)FEC_8_9 ) &&
                  (kFECAuto  == (Types)FEC_AUTO) &&
                  (kFEC_3_5  == (Types)FEC_3_5 ) &&
                  (kFEC_9_10 == (Types)FEC_9_10),
                  "FEC types don't match DVB includes.");
#endif

    explicit DTVCodeRate(Types _default = kFECAuto)
        : DTVParamHelper(_default) { }
    DTVCodeRate& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVCodeRate& operator=(const fe_code_rate_t rate)
        { m_value = rate; return *this; }
#endif

    bool IsCompatible(const DTVCodeRate other) const
        { return m_value == other.m_value || m_value == kFECAuto ||
                other.m_value == kFECAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVModulation : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint16_t
    {
        kModulationQPSK    = 0x000,
        kModulationQAM16   = 0x001,
        kModulationQAM32   = 0x002,
        kModulationQAM64   = 0x003,
        kModulationQAM128  = 0x004,
        kModulationQAM256  = 0x005,
        kModulationQAMAuto = 0x006,
        kModulation8VSB    = 0x007,
        kModulation16VSB   = 0x008,
        kModulation8PSK    = 0x009,
        kModulation16APSK  = 0x00A,
        kModulation32APSK  = 0x00B,
        kModulationDQPSK   = 0x00C,
        kModulationInvalid = 0x100, /* for removed modulations */
        kModulationAnalog  = 0x200, /* for analog channel scanner */
    };
#ifdef USING_DVB
    static_assert((kModulationQPSK    == (Types)QPSK    ) &&
                  (kModulationQAM16   == (Types)QAM_16  ) &&
                  (kModulationQAM32   == (Types)QAM_32  ) &&
                  (kModulationQAM64   == (Types)QAM_64  ) &&
                  (kModulationQAM128  == (Types)QAM_128 ) &&
                  (kModulationQAM256  == (Types)QAM_256 ) &&
                  (kModulationQAMAuto == (Types)QAM_AUTO) &&
                  (kModulation8VSB    == (Types)VSB_8   ) &&
                  (kModulation16VSB   == (Types)VSB_16  ) &&
                  (kModulation8PSK    == (Types)PSK_8   ) &&
                  (kModulation16APSK  == (Types)APSK_16 ) &&
                  (kModulation32APSK  == (Types)APSK_32 ) &&
                  (kModulationDQPSK   == (Types)DQPSK   ),
                  "Modulation types don't match DVB includes.");
#endif

    explicit DTVModulation(Types _default = kModulationQAMAuto)
        : DTVParamHelper(_default) { }
    DTVModulation& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVModulation& operator=(const fe_modulation_t modulation)
        { m_value = modulation; return *this; }
#endif

    bool IsCompatible(const DTVModulation other) const
        { return m_value == other.m_value || m_value == kModulationQAMAuto ||
                other.m_value == kModulationQAMAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }

    static QString toString(int _value)
    {
        if (kModulationInvalid == _value)
            return "invalid";
        if (kModulationAnalog == _value)
            return "analog";
        return DTVParamHelper::toString(kParseStrings, _value);
    }
};

class DTVTransmitMode : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kTransmissionMode2K,
        kTransmissionMode8K,
        kTransmissionModeAuto,
        kTransmissionMode4K,
        kTransmissionMode1K,
        kTransmissionMode16K,
        kTransmissionMode32K,
    };
#ifdef USING_DVB
    static_assert((kTransmissionMode2K   == (Types)TRANSMISSION_MODE_2K  ) &&
                  (kTransmissionMode8K   == (Types)TRANSMISSION_MODE_8K  ) &&
                  (kTransmissionModeAuto == (Types)TRANSMISSION_MODE_AUTO) &&
                  (kTransmissionMode4K   == (Types)TRANSMISSION_MODE_4K  ) &&
                  (kTransmissionMode1K   == (Types)TRANSMISSION_MODE_1K  ) &&
                  (kTransmissionMode16K  == (Types)TRANSMISSION_MODE_16K ) &&
                  (kTransmissionMode32K  == (Types)TRANSMISSION_MODE_32K ),
                  "Transmission types don't match DVB includes.");
#endif

    explicit DTVTransmitMode(Types _default = kTransmissionModeAuto)
        : DTVParamHelper(_default) { }
    DTVTransmitMode& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVTransmitMode& operator=(const fe_transmit_mode_t mode)
        { m_value = mode; return *this; }
#endif

    bool IsCompatible(const DTVTransmitMode other) const
        { return m_value == other.m_value || m_value == kTransmissionModeAuto ||
                other.m_value == kTransmissionModeAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }
    QChar   toChar() const
        {
            if (toString().length() > 0)
                return toString().at(0);
            return {};
        }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVGuardInterval : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kGuardInterval_1_32,
        kGuardInterval_1_16,
        kGuardInterval_1_8,
        kGuardInterval_1_4,
        kGuardIntervalAuto,
        kGuardInterval_1_128,
        kGuardInterval_19_128,
        kGuardInterval_19_256,
    };
#ifdef USING_DVB
    static_assert((kGuardInterval_1_32   == (Types)GUARD_INTERVAL_1_32  ) &&
                  (kGuardInterval_1_16   == (Types)GUARD_INTERVAL_1_16  ) &&
                  (kGuardInterval_1_8    == (Types)GUARD_INTERVAL_1_8   ) &&
                  (kGuardInterval_1_4    == (Types)GUARD_INTERVAL_1_4   ) &&
                  (kGuardIntervalAuto    == (Types)GUARD_INTERVAL_AUTO  ) &&
                  (kGuardInterval_1_128  == (Types)GUARD_INTERVAL_1_128 ) &&
                  (kGuardInterval_19_128 == (Types)GUARD_INTERVAL_19_128) &&
                  (kGuardInterval_19_256 == (Types)GUARD_INTERVAL_19_256),
                  "Guard Interval types don't match DVB includes.");
#endif

    explicit DTVGuardInterval(Types _default = kGuardIntervalAuto)
        : DTVParamHelper(_default) { }
    DTVGuardInterval& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVGuardInterval& operator=(const fe_guard_interval_t interval)
        { m_value = interval; return *this; }
#endif

    bool IsCompatible(const DTVGuardInterval other) const
        { return m_value == other.m_value || m_value == kGuardIntervalAuto ||
                other.m_value == kGuardIntervalAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVHierarchy : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kHierarchyNone,
        kHierarchy1,
        kHierarchy2,
        kHierarchy4,
        kHierarchyAuto,
    };
#ifdef USING_DVB
    static_assert((kHierarchyNone == (Types)HIERARCHY_NONE) &&
                  (kHierarchy1    == (Types)HIERARCHY_1   ) &&
                  (kHierarchy2    == (Types)HIERARCHY_2   ) &&
                  (kHierarchy4    == (Types)HIERARCHY_4   ) &&
                  (kHierarchyAuto == (Types)HIERARCHY_AUTO),
                  "Hierarchy types don't match DVB includes.");
#endif

    explicit DTVHierarchy(Types _default = kHierarchyAuto)
        : DTVParamHelper(_default) { }
    DTVHierarchy& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVHierarchy& operator=(const fe_hierarchy_t hierarchy)
        { m_value = hierarchy; return *this; }
#endif

    bool IsCompatible(const DTVHierarchy other) const
        { return m_value == other.m_value || m_value == kHierarchyAuto ||
                other.m_value == kHierarchyAuto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }
    QChar   toChar() const
        {
            if (toString().length() > 0)
                return toString().at(0);
            return {};
        }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVPolarity : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum PolarityValues : std::uint8_t
    {
        kPolarityVertical,
        kPolarityHorizontal,
        kPolarityRight,
        kPolarityLeft
    };

    explicit DTVPolarity(PolarityValues _default = kPolarityVertical)
        : DTVParamHelper(_default) { }
    DTVPolarity& operator=(const PolarityValues _value)
        { m_value = _value; return *this; }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }
    QChar   toChar() const
        {
            if (toString().length() > 0)
                return toString().at(0);
            return {};
        }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVModulationSystem : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        // see fe_delivery_system in frontend.h
        kModulationSystem_UNDEFINED,
        kModulationSystem_DVBC_ANNEX_A,
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
        kModulationSystem_DTMB,
        kModulationSystem_CMMB,
        kModulationSystem_DAB,
        kModulationSystem_DVBT2,
        kModulationSystem_TURBO,
        kModulationSystem_DVBC_ANNEX_C
    };
#ifdef USING_DVB
    static_assert((kModulationSystem_UNDEFINED    == (Types)SYS_UNDEFINED   ) &&
                  (kModulationSystem_DVBC_ANNEX_A == (Types)SYS_DVBC_ANNEX_A) &&
                  (kModulationSystem_DVBC_ANNEX_B == (Types)SYS_DVBC_ANNEX_B) &&
                  (kModulationSystem_DVBT         == (Types)SYS_DVBT        ) &&
                  (kModulationSystem_DSS          == (Types)SYS_DSS         ) &&
                  (kModulationSystem_DVBS         == (Types)SYS_DVBS        ) &&
                  (kModulationSystem_DVBS2        == (Types)SYS_DVBS2       ) &&
                  (kModulationSystem_DVBH         == (Types)SYS_DVBH        ) &&
                  (kModulationSystem_ISDBT        == (Types)SYS_ISDBT       ) &&
                  (kModulationSystem_ISDBS        == (Types)SYS_ISDBS       ) &&
                  (kModulationSystem_ISDBC        == (Types)SYS_ISDBC       ) &&
                  (kModulationSystem_ATSC         == (Types)SYS_ATSC        ) &&
                  (kModulationSystem_ATSCMH       == (Types)SYS_ATSCMH      ) &&
                  (kModulationSystem_DTMB         == (Types)SYS_DTMB        ) &&
                  (kModulationSystem_CMMB         == (Types)SYS_CMMB        ) &&
                  (kModulationSystem_DAB          == (Types)SYS_DAB         ) &&
                  (kModulationSystem_DVBT2        == (Types)SYS_DVBT2       ) &&
                  (kModulationSystem_TURBO        == (Types)SYS_TURBO       ) &&
                  (kModulationSystem_DVBC_ANNEX_C == (Types)SYS_DVBC_ANNEX_C),
                  "Modulation System types don't match DVB includes.");
#endif

    explicit DTVModulationSystem(uint8_t _value = kModulationSystem_UNDEFINED)
        : DTVParamHelper(_value) { }

    DTVModulationSystem& operator=(uint8_t _value)
        { m_value = _value; return *this; }

    bool IsCompatible(const DTVModulationSystem other) const
        { return
            (m_value == other.m_value) ||
            (m_value == kModulationSystem_DVBT  && other.m_value == kModulationSystem_DVBT2) ||
            (m_value == kModulationSystem_DVBT2 && other.m_value == kModulationSystem_DVBT);
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

class DTVRollOff : public DTVParamHelper
{
  protected:
    static const DTVParamHelperVec kConfTable;
    static const DTVParamHelperVec kVdrTable;
    static const DTVParamHelperVec kParseTable;
    static const DTVParamStringVec kParseStrings;

  public:
    enum Types : std::uint8_t
    {
        kRollOff_35,
        kRollOff_20,
        kRollOff_25,
        kRollOff_Auto,
    };
#ifdef USING_DVB
    static_assert((kRollOff_35 == (Types)ROLLOFF_35)
                  && (kRollOff_Auto == (Types)ROLLOFF_AUTO),
                  "Rolloff types don't match DVB includes.");
#endif

    explicit DTVRollOff(Types _default = kRollOff_35)
        : DTVParamHelper(_default) { }
    DTVRollOff& operator=(const Types _value)
        { m_value = _value; return *this; }
#ifdef USING_DVB
    DTVRollOff& operator=(fe_rolloff_t type)
        { m_value = type; return *this; }
#endif

    bool IsCompatible(const DTVRollOff other) const
        { return m_value == other.m_value || m_value == kRollOff_Auto ||
                other.m_value == kRollOff_Auto;
        }

    bool ParseConf(const QString &_value)
       { return ParseParam(_value, m_value, kConfTable); }
    bool ParseVDR(const QString &_value)
       { return ParseParam(_value, m_value, kVdrTable); }
    bool Parse(const QString &_value)
       { return ParseParam(_value, m_value, kParseTable); }

    QString toString() const { return toString(m_value); }

    static QString toString(int _value)
        { return DTVParamHelper::toString(kParseStrings, _value); }
};

#endif // DTVCONFPARSERHELPERS_H
