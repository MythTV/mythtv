// -*- Mode: c++ -*-

#ifndef FREQUENCY_TABLE_H
#define FREQUENCY_TABLE_H

// C++ includes
#include <list>
#include <vector>
using namespace std;

// Qt includes
#include <QString>
#include <QMap>

// MythTV includes
#include "mythtvexp.h"
#include "dtvconfparser.h"
#include "dtvconfparserhelpers.h"
#include "iptvtuningdata.h"

class FrequencyTable;
class TransportScanItem;

typedef QMap<QString, const FrequencyTable*> freq_table_map_t;
typedef vector<const FrequencyTable*>        freq_table_list_t;

bool teardown_frequency_tables(void);

freq_table_list_t get_matching_freq_tables(
    const QString &format, const QString &modulation, const QString &country);

MTV_PUBLIC long long get_center_frequency(
    QString format, QString modulation, QString country, int freqid);

int get_closest_freqid(
    QString format, QString modulation, QString country, long long centerfreq);

class FrequencyTable
{
  public:
    FrequencyTable(QString                 _name_format,
                   int                     _name_offset,
                   uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   DTVModulation           _modulation)
        : name_format(_name_format),       name_offset(_name_offset),
          frequencyStart(_frequencyStart), frequencyEnd(_frequencyEnd),
          frequencyStep(_frequencyStep),   modulation(_modulation),
          offset1(0),                      offset2(0),
          symbol_rate(0) { }

    FrequencyTable(uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   QString                 _name_format,
                   int                     _name_offset,
                   DTVInversion            _inversion,
                   DTVBandwidth            _bandwidth,
                   DTVCodeRate             _coderate_hp,
                   DTVCodeRate             _coderate_lp,
                   DTVModulation           _constellation,
                   DTVTransmitMode         _trans_mode,
                   DTVGuardInterval        _guard_interval,
                   DTVHierarchy            _hierarchy,
                   DTVModulation           _modulation,
                   int                     _offset1,
                   int                     _offset2)
        : name_format(_name_format),       name_offset(_name_offset),
          frequencyStart(_frequencyStart), frequencyEnd(_frequencyEnd),
          frequencyStep(_frequencyStep),   modulation(_modulation),
          offset1(_offset1),               offset2(_offset2),
          inversion(_inversion),           bandwidth(_bandwidth),
          coderate_hp(_coderate_hp),       coderate_lp(_coderate_lp),
          constellation(_constellation),   trans_mode(_trans_mode),
          guard_interval(_guard_interval), hierarchy(_hierarchy),
          symbol_rate(0) { }

    FrequencyTable(uint64_t                _frequencyStart,
                   uint64_t                _frequencyEnd,
                   uint                    _frequencyStep,
                   QString                 _name_format,
                   int                     _name_offset,
                   DTVCodeRate             _fec_inner,
                   DTVModulation           _modulation,
                   uint                    _symbol_rate,
                   int                     _offset1,
                   int                     _offset2)
        : name_format(_name_format),       name_offset(_name_offset),
          frequencyStart(_frequencyStart), frequencyEnd(_frequencyEnd),
          frequencyStep(_frequencyStep),   modulation(_modulation),
          offset1(_offset1),               offset2(_offset2),
          symbol_rate(_symbol_rate),       fec_inner(_fec_inner) { ; }

    virtual ~FrequencyTable() { ; }

    // Common Stuff
    QString           name_format;    ///< pretty name format
    int               name_offset;    ///< Offset to add to the pretty name
    uint64_t          frequencyStart; ///< The staring centre frequency
    uint64_t          frequencyEnd;   ///< The ending centre frequency
    uint              frequencyStep;  ///< The step in frequency
    DTVModulation     modulation;
    int               offset1; ///< The first  offset from the centre freq
    int               offset2; ///< The second offset from the centre freq

    // DVB OFDM stuff
    DTVInversion      inversion;
    DTVBandwidth      bandwidth;
    DTVCodeRate       coderate_hp;
    DTVCodeRate       coderate_lp;
    DTVModulation     constellation;
    DTVTransmitMode   trans_mode;
    DTVGuardInterval  guard_interval;
    DTVHierarchy      hierarchy;

    // DVB-C/DVB-S stuff
    uint              symbol_rate;
    DTVCodeRate       fec_inner;
};

/**
 *  \brief Class used for doing a list of frequencies / transports.
 *
 *   This is used for ATSC/NA Digital Cable and also scan all transports.
 */
class TransportScanItem
{
  public:
    TransportScanItem();
    TransportScanItem(uint           _sourceid,
                      const QString &_si_std,
                      const QString &_name,
                      uint           _mplexid,
                      uint           _timeoutTune);

    TransportScanItem(uint           _sourceid,
                      const QString &_name,
                      DTVMultiplex  &_tuning,
                      uint           _timeoutTune);

    TransportScanItem(uint                _sourceid,
                      const QString      &_name,
                      DTVTunerType        _tuner_type,
                      const DTVTransport &_tuning,
                      uint                _timeoutTune);

    TransportScanItem(uint                _sourceid,
                      const QString      &_si_std,
                      const QString &strFmt,  /* fmt for info shown to user  */
                      uint freqNum,
                      uint frequency,         /* center frequency to use     */
                      const FrequencyTable&,  /* freq table to get info from */
                      uint                _timeoutTune);

    TransportScanItem(uint                  _sourceid,
                      const QString        &_name,
                      const IPTVTuningData &_tuning,
                      const QString        &_channel,
                      uint                  _timeoutTune);

    uint offset_cnt() const
        { return (freq_offsets[2]) ? 3 : ((freq_offsets[1]) ? 2 : 1); }

    uint64_t freq_offset(uint i) const;

    QString toString() const;

  private:
    uint GetMultiplexIdFromDB(void) const;

  public:
    uint      mplexid;          ///< DB Mplexid

    QString   FriendlyName;     ///< Name to display in scanner dialog
    uint      friendlyNum;      ///< Frequency number (freqid w/freq table)
    int       SourceID;         ///< Associated SourceID
    bool      UseTimer;         /**< Set if timer is used after
                                     lock for getting PAT */

    bool      scanning;         ///< Probbably Unnecessary
    int       freq_offsets[3];  ///< Frequency offsets
    unsigned  timeoutTune;      ///< Timeout to tune to a frequency

    DTVMultiplex tuning;        ///< Tuning info
    IPTVTuningData iptv_tuning; ///< IPTV Tuning info
    QString        iptv_channel;///< IPTV base channel

    DTVChannelInfoList expectedChannels;
};

class transport_scan_items_it_t
{
  public:
    transport_scan_items_it_t() : _offset(0) {}
    transport_scan_items_it_t(const list<TransportScanItem>::iterator it)
    {
        _it = it;
        _offset = 0;
    }

    transport_scan_items_it_t& operator++()
    {
        _offset++;
        if ((uint)_offset >= (*_it).offset_cnt())
        {
            ++_it;
            _offset = 0;
        }
        return *this;
    }
    transport_scan_items_it_t& operator--()
    {
        _offset--;
        if (_offset < 0)
        {
            --_it;
            _offset = (*_it).offset_cnt() - 1;
        }
        return *this;
    }

    transport_scan_items_it_t operator++(int)
    {
        transport_scan_items_it_t tmp = *this;
        operator++();
        return tmp;
    }

    transport_scan_items_it_t operator--(int)
    {
        transport_scan_items_it_t tmp = *this;
        operator--();
        return tmp;
    }

    transport_scan_items_it_t& operator+=(int incr)
        { for (int i = 0; i < incr; i++) ++(*this); return *this; }
    transport_scan_items_it_t& operator-=(int incr)
        { for (int i = 0; i < incr; i++) --(*this); return *this; }


    const TransportScanItem& operator*() const { return *_it; }
    TransportScanItem&       operator*()       { return *_it; }

    list<TransportScanItem>::iterator iter() { return _it; }
    list<TransportScanItem>::const_iterator iter() const { return _it; }
    uint offset() const { return (uint) _offset; }
    transport_scan_items_it_t nextTransport() const
    {
        list<TransportScanItem>::iterator tmp = _it;
        return transport_scan_items_it_t(++tmp);
    }

  private:
    list<TransportScanItem>::iterator _it;
    int _offset;

    friend bool operator==(const transport_scan_items_it_t&,
                           const transport_scan_items_it_t&);
    friend bool operator!=(const transport_scan_items_it_t&,
                           const transport_scan_items_it_t&);

    friend bool operator==(const transport_scan_items_it_t&,
                           const list<TransportScanItem>::iterator&);
};

inline bool operator==(const transport_scan_items_it_t& A,
                       const transport_scan_items_it_t& B)
{
    list<TransportScanItem>::const_iterator A_it = A._it;
    list<TransportScanItem>::const_iterator B_it = B._it;

    return (A_it == B_it) && (A._offset == B._offset);
}

inline bool operator!=(const transport_scan_items_it_t &A,
                       const transport_scan_items_it_t &B)
{
    return (A._it != B._it) || (A._offset != B._offset);
}

inline bool operator==(const transport_scan_items_it_t& A,
                       const list<TransportScanItem>::iterator& B)
{
    list<TransportScanItem>::const_iterator A_it = A._it;
    list<TransportScanItem>::const_iterator B_it = B;

    return (A_it == B_it) && (0 == A.offset());
}

typedef list<TransportScanItem> transport_scan_items_t;

#endif // FREQUENCY_TABLE_H
