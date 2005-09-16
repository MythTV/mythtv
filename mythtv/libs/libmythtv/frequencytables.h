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
    TransportScanItem();
    TransportScanItem(int sourceid, const QString &std,
                      const QString &name, int mplexid,
                      uint tuneTimeout);
    TransportScanItem(int sourceid,           /* source id in DB */
                      const QString &std,     /* atsc/dvb */
                      const QString &strFmt,  /* fmt for info shown to user  */
                      uint freqNum,
                      uint frequency,         /* center frequency to use     */
                      const FrequencyTable&,  /* freq table to get info from */
                      uint tuneTimeout);

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
    QString toString() const;
};

class transport_scan_items_it_t
{
  public:
    transport_scan_items_it_t() : _offset(0) {}
    transport_scan_items_it_t(const QValueList<TransportScanItem>::iterator it)
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
        { transport_scan_items_it_t tmp = *this; return ++tmp; }
    transport_scan_items_it_t operator--(int)
        { transport_scan_items_it_t tmp = *this; return --tmp; }

    transport_scan_items_it_t& operator+=(int incr)
        { for (int i = 0; i < incr; i++) ++(*this); return *this; }
    transport_scan_items_it_t& operator-=(int incr)
        { for (int i = 0; i < incr; i++) --(*this); return *this; }


    const TransportScanItem& operator*() const { return *_it; }
    TransportScanItem&       operator*()       { return *_it; }

    uint offset() const { return (uint) _offset; }
    transport_scan_items_it_t nextTransport() const
    {
        QValueList<TransportScanItem>::iterator tmp = _it;
        return transport_scan_items_it_t(++tmp);
    }
  private:
    QValueList<TransportScanItem>::iterator _it;
    int _offset;

    friend bool operator==(const transport_scan_items_it_t&,
                           const transport_scan_items_it_t&);
    friend bool operator!=(const transport_scan_items_it_t&,
                           const transport_scan_items_it_t&);

    friend bool operator==(const transport_scan_items_it_t&,
                           const QValueList<TransportScanItem>::iterator&);
};

inline bool operator==(const transport_scan_items_it_t& A,
                       const transport_scan_items_it_t &B)
{
    return (A._it == B._it) && (A._offset == B._offset);
}

inline bool operator!=(const transport_scan_items_it_t &A,
                       const transport_scan_items_it_t &B)
{
    return (A._it != B._it) || (A._offset != B._offset);
}

inline bool operator==(const transport_scan_items_it_t& A,
                       const QValueList<TransportScanItem>::iterator& B)
{
    return A._it == B && (0 == A.offset());
}

typedef QValueList<TransportScanItem> transport_scan_items_t;

#endif // FREQUENCY_TABLE_H
