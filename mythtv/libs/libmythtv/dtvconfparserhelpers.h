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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _DTVCONFPARSERHELPERS_H_
#define _DTVCONFPARSERHELPERS_H_

#include <qstring.h>

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

    operator int()                const { return value;          }
    int operator=(int _value)           { return value = _value; }
    bool operator==(const int& v) const { return value == v;     }

  protected:
    static bool ParseParam(const QString &symbol, int &value,
                           const DTVParamHelperStruct *table);

    static QString toString(const char *strings[], int index,
                            uint strings_size);

  protected:
    int value;
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
        kBandwidth8Mhz,
        kBandwidth7Mhz,
        kBandwidth6Mhz,
        kBandwidthAuto,
    };

    DTVBandwidth(int _default = kBandwidthAuto) : DTVParamHelper(_default) { }

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

class DTVCodeRate : public DTVParamHelper
{
  protected:
    static const DTVParamHelperStruct confTable[];
    static const DTVParamHelperStruct vdrTable[];
    static const DTVParamHelperStruct parseTable[];
    static const uint kDBStrCnt = 10;
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
    };

    DTVCodeRate(int _default = kFECAuto) : DTVParamHelper(_default) { }

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
    static const uint kDBStrCnt = 10;
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
    };

    DTVModulation(int _default = kModulationQAMAuto)
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

    static QString toString(int _value)
        { return DTVParamHelper::toString(dbStr, _value, kDBStrCnt); }
};

#endif // _DTVCONFPARSERHELPERS_H_
