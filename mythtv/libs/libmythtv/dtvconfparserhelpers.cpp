
#include <QMutex>
#include <QMap>

#include "libmythbase/mythlogging.h"
#include "dtvconfparserhelpers.h"

bool DTVParamHelper::ParseParam(const QString &symbol, int &value,
                                const DTVParamHelperVec &table)
{
    auto it = std::find_if(table.cbegin(), table.cend(),
                           [symbol](const auto& item) -> bool
                               {return item.symbol == symbol;});
    if (it == table.cend())
        return false;

    value = it->value;
    return true;
}

QString DTVParamHelper::toString(const DTVParamStringVec &strings, int index)
{
    if ((index < 0) || ((uint)index >= strings.size()))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DTVParamHelper::toString() index out of bounds");

        return {};
    }

    return QString::fromStdString(strings[index]);
}

//
// === Tuner Type ===
//

const int DTVTunerType::kTunerTypeDVBS1   = 0x0000;
const int DTVTunerType::kTunerTypeDVBS2   = 0x0020;
const int DTVTunerType::kTunerTypeDVBC    = 0x0001;
const int DTVTunerType::kTunerTypeDVBT    = 0x0002;
const int DTVTunerType::kTunerTypeDVBT2   = 0x0022;
const int DTVTunerType::kTunerTypeATSC    = 0x0003;
const int DTVTunerType::kTunerTypeASI     = 0x1000;
const int DTVTunerType::kTunerTypeOCUR    = 0x2000;
const int DTVTunerType::kTunerTypeIPTV    = 0x4000;
const int DTVTunerType::kTunerTypeUnknown = 0x80000000;

static QMutex dtv_tt_canonical_str_lock;
static QMap<int,QString> dtv_tt_canonical_str;
void DTVTunerType::initStr(void)
{
    QMutexLocker locker(&dtv_tt_canonical_str_lock);
    dtv_tt_canonical_str[kTunerTypeATSC]    = "ATSC";
    dtv_tt_canonical_str[kTunerTypeDVBT]    = "OFDM";
    dtv_tt_canonical_str[kTunerTypeDVBT2]   = "DVB_T2";
    dtv_tt_canonical_str[kTunerTypeDVBC]    = "QAM";
    dtv_tt_canonical_str[kTunerTypeDVBS1]   = "QPSK";
    dtv_tt_canonical_str[kTunerTypeDVBS2]   = "DVB_S2";
    dtv_tt_canonical_str[kTunerTypeASI]     = "ASI";
    dtv_tt_canonical_str[kTunerTypeOCUR]    = "OCUR";
    dtv_tt_canonical_str[kTunerTypeUnknown] = "UNKNOWN";
}

QString DTVTunerType::toString(int _value)
{
    QMutexLocker locker(&dtv_tt_canonical_str_lock);
    QMap<int,QString>::const_iterator it = dtv_tt_canonical_str.constFind(_value);
    if (it != dtv_tt_canonical_str.constEnd())
        return *it;
    return dtv_tt_canonical_str[kTunerTypeUnknown];
}

const DTVParamHelperVec DTVTunerType::kParseTable
{
    { "QPSK",    kTunerTypeDVBS1   },
    { "QAM",     kTunerTypeDVBC    },
    { "OFDM",    kTunerTypeDVBT    },
    { "DVB_T2",  kTunerTypeDVBT2   },
    { "ATSC",    kTunerTypeATSC    },
    { "DVB_S2",  kTunerTypeDVBS2   },
    { "ASI",     kTunerTypeASI     },
    { "OCUR",    kTunerTypeOCUR    },
    { "UNKNOWN", kTunerTypeUnknown },
    { nullptr,   kTunerTypeUnknown },
};

//
// === Inversion ===
//

const DTVParamHelperVec DTVInversion::kConfTable
{
   { "INVERSION_AUTO", kInversionAuto },
   { "INVERSION_OFF",  kInversionOff  },
   { "INVERSION_ON",   kInversionOn   },
   { nullptr,          kInversionAuto },
};

const DTVParamHelperVec DTVInversion::kVdrTable
{
   { "999",   kInversionAuto },
   { "0",     kInversionOff  },
   { "1",     kInversionOn   },
   { nullptr, kInversionAuto },
};

const DTVParamHelperVec DTVInversion::kParseTable
{
   { "a",     kInversionAuto },
   { "0",     kInversionOff  },
   { "1",     kInversionOn   },
   { nullptr, kInversionAuto },
};

const DTVParamStringVec DTVInversion::kParseStrings
{
    "0", ///< kInversionOff
    "1", ///< kInversionOn
    "a"  ///< kInversionAuto
};

//
// === Bandwidth ===
//
// Database update needed for the 10MHz and 1.712MHz because
// dtvmultiplex:bandwidth is now only one character.
//

const DTVParamHelperVec DTVBandwidth::kConfTable
{
   { "BANDWIDTH_AUTO",      kBandwidthAuto    },
   { "BANDWIDTH_8_MHZ",     kBandwidth8MHz    },
   { "BANDWIDTH_7_MHZ",     kBandwidth7MHz    },
   { "BANDWIDTH_6_MHZ",     kBandwidth6MHz    },
   { "BANDWIDTH_5_MHZ",     kBandwidth5MHz    },
   { "BANDWIDTH_10_MHZ",    kBandwidth10MHz   },
   { "BANDWIDTH_1_712_MHZ", kBandwidth1712kHz },
   { nullptr,               kBandwidthAuto    },
};

const DTVParamHelperVec DTVBandwidth::kVdrTable
{
   { "999",   kBandwidthAuto    },
   { "8",     kBandwidth8MHz    },
   { "7",     kBandwidth7MHz    },
   { "6",     kBandwidth6MHz    },
   { "5",     kBandwidth5MHz    },
   { "10",    kBandwidth10MHz   },
   { "1712",  kBandwidth1712kHz },
   { nullptr, kBandwidthAuto    },
};

const DTVParamHelperVec DTVBandwidth::kParseTable
{
   { "a",     kBandwidthAuto    },
   { "8",     kBandwidth8MHz    },
   { "7",     kBandwidth7MHz    },
   { "6",     kBandwidth6MHz    },
   { "5",     kBandwidth5MHz    },
   { "10",    kBandwidth10MHz   },
   { "1712",  kBandwidth1712kHz },
   { nullptr, kBandwidthAuto    },
};

const DTVParamStringVec DTVBandwidth::kParseStrings
{
    "8",     ///< kBandwidth8MHz
    "7",     ///< kBandwidth7MHz
    "6",     ///< kBandwidth6MHz
    "a",     ///< kBandwidthAUTO
    "5",     ///< kBandwidth5MHz
    "10",    ///< kBandwidth10MHz
    "1712",  ///< kBandwidth1712kHz
};

//
// === Forward Error Correction / Code Rate ===
//

const DTVParamHelperVec DTVCodeRate::kConfTable
{
    { "FEC_AUTO", kFECAuto  },
    { "FEC_1_2",  kFEC_1_2  },
    { "FEC_2_3",  kFEC_2_3  },
    { "FEC_3_4",  kFEC_3_4  },
    { "FEC_4_5",  kFEC_4_5  },
    { "FEC_5_6",  kFEC_5_6  },
    { "FEC_6_7",  kFEC_6_7  },
    { "FEC_7_8",  kFEC_7_8  },
    { "FEC_8_9",  kFEC_8_9  },
    { "FEC_NONE", kFECNone  },
    { "FEC_3_5",  kFEC_3_5  },
    { "FEC_9_10", kFEC_9_10 },
    { nullptr,    kFECAuto  },
};

const DTVParamHelperVec DTVCodeRate::kVdrTable
{
    { "999",   kFECAuto  },
    { "12",    kFEC_1_2  },
    { "23",    kFEC_2_3  },
    { "34",    kFEC_3_4  },
    { "45",    kFEC_4_5  },
    { "56",    kFEC_5_6  },
    { "67",    kFEC_6_7  },
    { "78",    kFEC_7_8  },
    { "89",    kFEC_8_9  },
    { "0",     kFECNone  },
    { "35",    kFEC_3_5  },
    { "910",   kFEC_9_10 },
    { nullptr, kFECAuto  }
};

const DTVParamHelperVec DTVCodeRate::kParseTable
{
    { "auto",  kFECAuto  },
    { "1/2",   kFEC_1_2  },
    { "2/3",   kFEC_2_3  },
    { "3/4",   kFEC_3_4  },
    { "4/5",   kFEC_4_5  },
    { "5/6",   kFEC_5_6  },
    { "6/7",   kFEC_6_7  },
    { "7/8",   kFEC_7_8  },
    { "8/9",   kFEC_8_9  },
    { "none",  kFECNone  },
    { "3/5",   kFEC_3_5  },
    { "9/10",  kFEC_9_10 },
    { nullptr, kFECAuto  }
};

const DTVParamStringVec DTVCodeRate::kParseStrings
{
     "none",  ///< kFECNone
     "1/2",   ///< kFEC_1_2
     "2/3",   ///< kFEC_2_3
     "3/4",   ///< kFEC_3_4
     "4/5",   ///< kFEC_4_5
     "5/6",   ///< kFEC_5_6
     "6/7",   ///< kFEC_6_7
     "7/8",   ///< kFEC_7_8
     "8/9",   ///< kFEC_8_9
     "auto",  ///< kFECAuto
     "3/5",   ///< kFEC_3_5
     "9/10",  ///< kFEC_9_10
};

//
// === Modulation ===
//

const DTVParamHelperVec DTVModulation::kConfTable
{
   { "QAM_AUTO", kModulationQAMAuto },
   { "QAM_16",   kModulationQAM16   },
   { "QAM_32",   kModulationQAM32   },
   { "QAM_64",   kModulationQAM64   },
   { "QAM_128",  kModulationQAM128  },
   { "QAM_256",  kModulationQAM256  },
   { "QPSK",     kModulationQPSK    },
   { "8VSB",     kModulation8VSB    },
   { "16VSB",    kModulation16VSB   },
   { "8PSK",     kModulation8PSK    },
   { "16APSK",   kModulation16APSK  },
   { "32APSK",   kModulation32APSK  },
   { "DQPSK",    kModulationDQPSK   },
   { "16PSK",    kModulationInvalid },
   { "2VSB",     kModulationInvalid },
   { "4VSB",     kModulationInvalid },
   { "BPSK",     kModulationInvalid },
   { "analog",   kModulationAnalog  },
   { nullptr,    kModulationQAMAuto },
};

const DTVParamHelperVec DTVModulation::kVdrTable
{
   { "999",   kModulationQAMAuto },
   { "16",    kModulationQAM16   },
   { "32",    kModulationQAM32   },
   { "64",    kModulationQAM64   },
   { "128",   kModulationQAM128  },
   { "256",   kModulationQAM256  },
   { "2",     kModulationQPSK    },
   { "5",     kModulation8PSK    },
   { "6",     kModulation16APSK  },
   { "7",     kModulation32APSK  },
   { "10",    kModulation8VSB    },
   { "11",    kModulation16VSB   },
   { nullptr, kModulationQAMAuto },
};

const DTVParamHelperVec DTVModulation::kParseTable
{
   { "auto",     kModulationQAMAuto },
   { "qam_16",   kModulationQAM16   },
   { "qam_32",   kModulationQAM32   },
   { "qam_64",   kModulationQAM64   },
   { "qam_128",  kModulationQAM128  },
   { "qam_256",  kModulationQAM256  },
   { "qpsk",     kModulationQPSK    },
   { "8vsb",     kModulation8VSB    },
   { "16vsb",    kModulation16VSB   },
   { "8psk",     kModulation8PSK    },
   { "16apsk",   kModulation16APSK  },
   { "32apsk",   kModulation32APSK  },
   { "dqpsk",    kModulationDQPSK   },
   // alternates
   { "a",        kModulationQAMAuto },
   { "qam_auto", kModulationQAMAuto },
   { "qam-16",   kModulationQAM16   },
   { "qam-32",   kModulationQAM32   },
   { "qam-64",   kModulationQAM64   },
   { "qam-128",  kModulationQAM128  },
   { "qam-256",  kModulationQAM256  },
   // qpsk, no alternative
   { "8-vsb",    kModulation8VSB    },
   { "16-vsb",   kModulation16VSB   },
   // bpsk, no alternative
   { "16-apsk",  kModulation16APSK  },
   { "32-apsk",  kModulation32APSK  },
   { "8-psk",    kModulation8PSK    },
   // removed modulations and alternatives
   { "bpsk",     kModulationInvalid },
   { "2vsb",     kModulationInvalid },
   { "2-vsb",    kModulationInvalid },
   { "4vsb",     kModulationInvalid },
   { "4-vsb",    kModulationInvalid },
   { "16psk",    kModulationInvalid },
   { "16-psk",   kModulationInvalid },
   { nullptr,    kModulationQAMAuto },
};

const DTVParamStringVec DTVModulation::kParseStrings
{
    "qpsk",    ///< kModulationQPSK,
    "qam_16",  ///< kModulationQAM16
    "qam_32",  ///< kModulationQAM32
    "qam_64",  ///< kModulationQAM64
    "qam_128", ///< kModulationQAM128
    "qam_256", ///< kModulationQAM256
    "auto",    ///< kModulationQAMAuto
    "8vsb",    ///< kModulation8VSB
    "16vsb",   ///< kModulation16VSB
    "8psk",    ///< kModulation8PSK
    "16apsk",  ///< kModulation16APSK
    "32apsk",  ///< kModulation32APSK
    "dqpsk"    ///< kModulationDQPSK
};

//
// === Transmission Mode ===
//
// Database update needed for the 16k and 32k modes because
// dtvmultiplex:transmission_mode is now only one character.
//
const DTVParamHelperVec DTVTransmitMode::kConfTable
{
   { "TRANSMISSION_MODE_1K",   kTransmissionMode1K   },
   { "TRANSMISSION_MODE_2K",   kTransmissionMode2K   },
   { "TRANSMISSION_MODE_4K",   kTransmissionMode4K   },
   { "TRANSMISSION_MODE_8K",   kTransmissionMode8K   },
   { "TRANSMISSION_MODE_16K",  kTransmissionMode16K  },
   { "TRANSMISSION_MODE_32K",  kTransmissionMode32K  },
   { "TRANSMISSION_MODE_AUTO", kTransmissionModeAuto },
   { nullptr,                  kTransmissionModeAuto },
};

const DTVParamHelperVec DTVTransmitMode::kVdrTable
{
   { "999",   kTransmissionModeAuto },
   { "1",     kTransmissionMode1K   },
   { "2",     kTransmissionMode2K   },
   { "4",     kTransmissionMode4K   },
   { "8",     kTransmissionMode8K   },
   { "16",    kTransmissionMode16K  },
   { "32",    kTransmissionMode32K  },
   { nullptr, kTransmissionModeAuto },
};

const DTVParamHelperVec DTVTransmitMode::kParseTable
{
   { "a",     kTransmissionModeAuto },
   { "1",     kTransmissionMode1K   },
   { "2",     kTransmissionMode2K   },
   { "4",     kTransmissionMode4K   },
   { "8",     kTransmissionMode8K   },
   { "16",    kTransmissionMode16K  },
   { "32",    kTransmissionMode32K  },
   { nullptr, kTransmissionModeAuto },
};

const DTVParamStringVec DTVTransmitMode::kParseStrings
{
    "2",   ///< kTransmissionMode2K
    "8",   ///< kTransmissionMode8K
    "a",   ///< kTransmissionModeAuto
    "4",   ///< kTransmissionMode4K
    "1",   ///< kTransmissionMode1K
    "16",  ///< kTransmissionMode16K
    "32"   ///< kTransmissionMode32K
};

//
// === Guard Interval ===
//

const DTVParamHelperVec DTVGuardInterval::kConfTable
{
   { "GUARD_INTERVAL_1_32",    kGuardInterval_1_32   },
   { "GUARD_INTERVAL_1_16",    kGuardInterval_1_16   },
   { "GUARD_INTERVAL_1_8",     kGuardInterval_1_8    },
   { "GUARD_INTERVAL_1_4",     kGuardInterval_1_4    },
   { "GUARD_INTERVAL_AUTO",    kGuardIntervalAuto    },
   { "GUARD_INTERVAL_1_128",   kGuardInterval_1_128  },
   { "GUARD_INTERVAL_19_128",  kGuardInterval_19_128 },
   { "GUARD_INTERVAL_19_256",  kGuardInterval_19_256 },
   { nullptr,                  kGuardIntervalAuto    },
};

const DTVParamHelperVec DTVGuardInterval::kVdrTable
{
   { "999",    kGuardIntervalAuto    },
   { "32",     kGuardInterval_1_32   },
   { "16",     kGuardInterval_1_16   },
   { "8",      kGuardInterval_1_8    },
   { "4",      kGuardInterval_1_4    },
   { "128",    kGuardInterval_1_128  },
   { "19128",  kGuardInterval_19_128 },
   { "19256",  kGuardInterval_19_256 },
   { nullptr,  kGuardIntervalAuto    },
};

const DTVParamHelperVec DTVGuardInterval::kParseTable
{
   { "auto",   kGuardIntervalAuto    },
   { "1/32",   kGuardInterval_1_32   },
   { "1/16",   kGuardInterval_1_16   },
   { "1/8",    kGuardInterval_1_8    },
   { "1/4",    kGuardInterval_1_4    },
   { "1/128",  kGuardInterval_1_128  },
   { "19/128", kGuardInterval_19_128 },
   { "19/256", kGuardInterval_19_256 },
   { nullptr,  kGuardIntervalAuto    },
};

const DTVParamStringVec DTVGuardInterval::kParseStrings
{
    "1/32",   ///< kGuardInterval_1_32
    "1/16",   ///< kGuardInterval_1_16
    "1/8",    ///< kGuardInterval_1_8
    "1/4",    ///< kGuardInterval_1_4
    "auto",   ///< kGuardIntervalAuto
    "1/128",  ///< kGuardInterval_1_128
    "19/128", ///< kGuardInterval_19_128
    "19/256", ///< kGuardInterval_19_256
};

//
// === Hierarchy ===
//

const DTVParamHelperVec DTVHierarchy::kConfTable
{
   { "HIERARCHY_NONE", kHierarchyNone },
   { "HIERARCHY_1",    kHierarchy1    },
   { "HIERARCHY_2",    kHierarchy2    },
   { "HIERARCHY_4",    kHierarchy4    },
   { "HIERARCHY_AUTO", kHierarchyAuto },
   { nullptr,          kHierarchyAuto },
};

const DTVParamHelperVec DTVHierarchy::kVdrTable
{
   { "999",   kHierarchyAuto },
   { "0",     kHierarchyNone },
   { "1",     kHierarchy1    },
   { "2",     kHierarchy2    },
   { "4",     kHierarchy4    },
   { nullptr, kHierarchyAuto },
};

const DTVParamHelperVec DTVHierarchy::kParseTable
{
   { "n",     kHierarchyNone },
   { "1",     kHierarchy1    },
   { "2",     kHierarchy2    },
   { "4",     kHierarchy4    },
   { "a",     kHierarchyAuto },
   { nullptr, kHierarchyAuto },
};

const DTVParamStringVec DTVHierarchy::kParseStrings
{
    "n", ///< kHierarchyNone
    "1", ///< kHierarchy1
    "2", ///< kHierarchy2
    "4", ///< kHierarchy4
    "a"  ///< kHierarchyAuto
};

//
// === Polarity ===
//

const DTVParamHelperVec DTVPolarity::kParseTable
{
    { "v",     kPolarityVertical   },
    { "h",     kPolarityHorizontal },
    { "r",     kPolarityRight      },
    { "l",     kPolarityLeft       },
    { nullptr, kPolarityVertical   },
};

const DTVParamStringVec DTVPolarity::kParseStrings
{
   "v", ///< kPolarityVertical
   "h", ///< kPolarityHorizontal
   "r", ///< kPolarityRight
   "l"  ///< kPolarityLeft
};

//
// === Modulation System ===
//

const DTVParamHelperVec DTVModulationSystem::kConfTable
{
    { "SYS_UNDEFINED",     kModulationSystem_UNDEFINED     },
    { "SYS_DVBC_ANNEX_A",  kModulationSystem_DVBC_ANNEX_A  },
    { "SYS_DVBC_ANNEX_B",  kModulationSystem_DVBC_ANNEX_B  },
    { "SYS_DVBT",          kModulationSystem_DVBT          },
    { "SYS_DVBT2",         kModulationSystem_DVBT2         },
    { "SYS_DSS",           kModulationSystem_DSS           },
    { "SYS_DVBS",          kModulationSystem_DVBS          },
    { "SYS_DVBS2",         kModulationSystem_DVBS2         },
    { "SYS_DVBH",          kModulationSystem_DVBH          },
    { "SYS_ISDBT",         kModulationSystem_ISDBT         },
    { "SYS_ISDBS",         kModulationSystem_ISDBS         },
    { "SYS_ISDBC",         kModulationSystem_ISDBC         },
    { "SYS_ATSC",          kModulationSystem_ATSC          },
    { "SYS_ATSCMH",        kModulationSystem_ATSCMH        },
    { "SYS_DTMB",          kModulationSystem_DTMB          },
    { "SYS_CMMB",          kModulationSystem_CMMB          },
    { "SYS_DAB",           kModulationSystem_DAB           },
    { "SYS_TURBO",         kModulationSystem_TURBO         },
    { "SYS_DVBC_ANNEX_C",  kModulationSystem_DVBC_ANNEX_C  },
    { nullptr,             kModulationSystem_UNDEFINED     },
};

const DTVParamHelperVec DTVModulationSystem::kVdrTable
{
    { "0",     kModulationSystem_DVBS      },
    { "1",     kModulationSystem_DVBS2     },
    { nullptr, kModulationSystem_UNDEFINED },
};

const DTVParamHelperVec DTVModulationSystem::kParseTable
{
    { "UNDEFINED",    kModulationSystem_UNDEFINED     },
    { "DVB-C/A",      kModulationSystem_DVBC_ANNEX_A  },
    { "DVB-C/B",      kModulationSystem_DVBC_ANNEX_B  },
    { "DVB-T",        kModulationSystem_DVBT          },
    { "DSS",          kModulationSystem_DSS           },
    { "DVB-S",        kModulationSystem_DVBS          },
    { "DVB-S2",       kModulationSystem_DVBS2         },
    { "DVBH",         kModulationSystem_DVBH          },
    { "ISDBT",        kModulationSystem_ISDBT         },
    { "ISDBS",        kModulationSystem_ISDBS         },
    { "ISDBC",        kModulationSystem_ISDBC         },
    { "ATSC",         kModulationSystem_ATSC          },
    { "ATSCMH",       kModulationSystem_ATSCMH        },
    { "DTMB",         kModulationSystem_DTMB          },
    { "CMMB",         kModulationSystem_CMMB          },
    { "DAB",          kModulationSystem_DAB           },
    { "DVB-T2",       kModulationSystem_DVBT2         },
    { "TURBO",        kModulationSystem_TURBO         },
    { "DVB-C/C",      kModulationSystem_DVBC_ANNEX_C  },
    { nullptr,        kModulationSystem_UNDEFINED     },
};

const DTVParamStringVec DTVModulationSystem::kParseStrings
{
    "UNDEFINED",     ///< kModulationSystem_UNDEFINED
    "DVB-C/A",       ///< kModulationSystem_DVBC_ANNEX_A
    "DVB-C/B",       ///< kModulationSystem_DVBC_ANNEX_B
    "DVB-T",         ///< kModulationSystem_DVBT
    "DSS",           ///< kModulationSystem_DSS
    "DVB-S",         ///< kModulationSystem_DVBS
    "DVB-S2",        ///< kModulationSystem_DVBS2
    "DVBH",          ///< kModulationSystem_DVBH
    "ISDBT",         ///< kModulationSystem_ISDBT
    "ISDBS",         ///< kModulationSystem_ISDBS
    "ISDBC",         ///< kModulationSystem_ISDBC
    "ATSC",          ///< kModulationSystem_ATSC
    "ATSCMH",        ///< kModulationSystem_ATSCMH
    "DTMB",          ///< kModulationSystem_DTMB
    "CMMB",          ///< kModulationSystem_CMMB
    "DAB",           ///< kModulationSystem_DAB
    "DVB-T2",        ///< kModulationSystem_DVBT2
    "TURBO",         ///< kModulationSystem_TURBO
    "DVB-C/C",       ///< kModulationSystem_DVBC_ANNEX_C
};

//
// === Rolloff ===
//

const DTVParamHelperVec DTVRollOff::kConfTable
{
   { "ROLLOFF_35",   kRollOff_35   },
   { "ROLLOFF_20",   kRollOff_20   },
   { "ROLLOFF_25",   kRollOff_25   },
   { "ROLLOFF_AUTO", kRollOff_Auto },
   { nullptr,        kRollOff_35   },
};

const DTVParamHelperVec DTVRollOff::kVdrTable
{
   { "35",    kRollOff_35   },
   { "20",    kRollOff_20   },
   { "25",    kRollOff_25   },
   { "0",     kRollOff_Auto },
   { nullptr, kRollOff_35   },
};

const DTVParamHelperVec DTVRollOff::kParseTable
{
   { "0.35",  kRollOff_35   },
   { "0.20",  kRollOff_20   },
   { "0.25",  kRollOff_25   },
   { "auto",  kRollOff_Auto },
   { nullptr, kRollOff_35   },
};

const DTVParamStringVec DTVRollOff::kParseStrings
{
   "0.35",   ///< kRollOff_35
   "0.20",   ///< kRollOff_20
   "0.25",   ///< kRollOff_25
   "auto",   ///< kRollOff_Auto
};
