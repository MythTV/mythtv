// -*- Mode: c++ -*-

#ifndef FREQUENCY_TABLE_H
#define FREQUENCY_TABLE_H

// C++ includes
#include <vector>
using namespace std;

// Qt includes
#include <qmap.h>
#include <qstring.h>
#include <qmutex.h>

// MythTV includes
#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbtypes.h"
#if (DVB_API_VERSION_MINOR <= 3 && DVB_API_VERSION_MINOR == 0)
#    define VSB_8         (QAM_AUTO+1)
#    define VSB_16        (QAM_AUTO+2)
#endif
#else // if ! USING_DVB
#define QAM_64   3
#define QAM_128  4
#define QAM_256  5
#define QAM_AUTO 6
#define VSB_8    7
#define VSB_16   8
#endif // USING_DVB

class FrequencyTable;
class TransportScanItem;

typedef QMap<QString, const FrequencyTable*> freq_table_map_t;
typedef vector<const FrequencyTable*>        freq_table_list_t;
typedef QValueList<TransportScanItem>        transport_scan_items_t;
typedef transport_scan_items_t::iterator     transport_scan_items_it_t;

void init_freq_tables();

freq_table_list_t get_matching_freq_tables(
    QString format, QString modulation, QString country);

class FrequencyTable
{
  public:
    FrequencyTable(QString                 _name_format,
                   int                     _name_offset,
                   uint                    _frequencyStart,
                   uint                    _frequencyEnd,
                   uint                    _frequencyStep,
                   uint                    _modulation)
        : name_format(_name_format),       name_offset(_name_offset),
          frequencyStart(_frequencyStart), frequencyEnd(_frequencyEnd),
          frequencyStep(_frequencyStep),   
          modulation(_modulation) { ; }
    virtual ~FrequencyTable() { ; }

    QString name_format;          ///< pretty name format
    int     name_offset;          ///< Offset to add to the pretty name
    uint    frequencyStart;       ///< The staring centre frequency
    uint    frequencyEnd;         ///< The ending centre frequency
    uint    frequencyStep;        ///< The step in frequency
    uint    modulation;
};

#ifdef USING_DVB
class DVBFrequencyTable : public FrequencyTable
{
  public:
    DVBFrequencyTable(uint                    _frequencyStart,
                      uint                    _frequencyEnd,
                      uint                    _frequencyStep,
                      QString                 _name_format,
                      int                     _name_offset,
                      fe_spectral_inversion_t _inversion,
                      fe_bandwidth_t          _bandwidth,
                      fe_code_rate_t          _coderate_hp,
                      fe_code_rate_t          _coderate_lp,
                      fe_modulation_t         _constellation,  
                      fe_transmit_mode_t      _trans_mode, 
                      fe_guard_interval_t     _guard_interval,
                      fe_hierarchy_t          _hierarchy,
                      fe_modulation_t         _modulation,
                      int                     _offset1,
                      int                     _offset2)
        : FrequencyTable(_name_format,     _name_offset,
                         _frequencyStart,  _frequencyEnd,
                         _frequencyStep,   _modulation),
          inversion(_inversion),           bandwidth(_bandwidth),
          coderate_hp(_coderate_hp),       coderate_lp(_coderate_lp),
          constellation(_constellation),   trans_mode(_trans_mode),
          guard_interval(_guard_interval), hierarchy(_hierarchy),
          offset1(_offset1),               offset2(_offset2) { ; }

    fe_spectral_inversion_t inversion;
    fe_bandwidth_t          bandwidth;
    fe_code_rate_t          coderate_hp;
    fe_code_rate_t          coderate_lp;
    fe_modulation_t         constellation;  
    fe_transmit_mode_t      trans_mode; 
    fe_guard_interval_t     guard_interval;
    fe_hierarchy_t          hierarchy;
    int     offset1;              ///< The first offset from the centre freq
    int     offset2;              ///< The second offset from the centre freq
};
#endif // USING_DVB

/**
 *  \brief Class used for doing a list of frequencies / transports.
 *
 *   This is used for ATSC/NA Digital Cable and also scan all transports.
 */
class TransportScanItem
{
  public:
    enum
    {
        DVBT_TUNINGTIMEOUT = 1000,
        ATSC_TUNINGTIMEOUT = 2000,
    };

  public:
    TransportScanItem();
    TransportScanItem(int sourceid, int mplexid, const QString &fn);
    TransportScanItem(int sourceid,           /* source id in DB */
                      const QString &std,     /* atsc/dvb */
                      const QString &strFmt,  /* fmt for info shown to user  */
                      uint freqNum,
                      uint frequency,         /* center frequency to use     */
                      const FrequencyTable&); /* freq table to get info from */


    uint offset_cnt() const
        { return (freq_offsets[2]) ? 3 : ((freq_offsets[1]) ? 2 : 1); }

    uint freq_offset(uint i) const;

  private:
    int GetMultiplexIdFromDB() const;

  public:
    int       mplexid;          ///< DB Mplexid
    QString   standard;         ///< DVB/ATSC

    QString   FriendlyName;     ///< Name to display in scanner dialog
    uint      friendlyNum;      ///< Frequency number (freqid w/freq table)
    int       SourceID;         ///< Associated SourceID
    bool      UseTimer;         /**< Set if timer is used after 
                                     lock for getting PAT */

    bool      scanning;         ///< Probbably Unnecessary
    int       freq_offsets[3];  ///< Frequency offsets
    unsigned  timeoutTune;      ///< Timeout to tune to a frequency

#ifdef USING_DVB
    DVBTuning tuning;           ///< DVB Tuning struct if mplexid == -1
#else
    uint      frequency;        ///< Tuning frequency if mplexid == -1
    uint      modulation;       ///< Tuning frequency if mplexid == -1
#endif

    bool      complete;         ///< Used by old siscan
    int       offset1;          ///< Used by old siscan
    int       offset2;          ///< Used by old siscan

    QString toString() const;
};

#endif // FREQUENCY_TABLE_H
