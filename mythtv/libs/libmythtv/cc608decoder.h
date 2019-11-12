// -*- Mode: c++ -*-

#ifndef CCDECODER_H_
#define CCDECODER_H_

#include <cstdint>
#include <ctime>

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QChar>

#include "format.h"

class CC608Input
{
  public:
    virtual ~CC608Input() = default;
    virtual void AddTextData(unsigned char *buf, int len,
                             int64_t timecode, char type) = 0;
};

enum
{
    kHasMPAA       = 0x1,
    kHasTPG        = 0x2,
    kHasCanEnglish = 0x4,
    kHasCanFrench  = 0x8,
};
enum
{
    kRatingMPAA = 0,
    kRatingTPG,
    kRatingCanEnglish,
    kRatingCanFrench,
};

class CC608Decoder
{
  public:
    explicit CC608Decoder(CC608Input *ccr);
    CC608Decoder(const CC608Decoder& rhs);
    ~CC608Decoder();

    void FormatCC(int tc, int code1, int code2);
    void FormatCCField(int tc, int field, int data);
    int FalseDup(int tc, int field, int data);

    void DecodeVPS(const unsigned char *buf);
    void DecodeWSS(const unsigned char *buf);

    void SetIgnoreTimecode(bool val) { m_ignore_time_code = val; }

    uint    GetRatingSystems(bool future) const;
    uint    GetRating(uint i, bool future) const;
    QString GetRatingString(uint i, bool future) const;
    QString GetProgramName(bool future) const;
    QString GetProgramType(bool future) const;
    QString GetXDS(const QString &key) const;

    /// \return Services seen in last few seconds as specified.
    void GetServices(uint seconds, bool[4]) const;

    static QString ToASCII(const QString &cc608, bool suppress_unknown);

  private:
    QChar CharCC(int code) const { return m_stdchar[code]; }
    void ResetCC(int mode);
    void BufferCC(int mode, int len, int clr);
    int NewRowCC(int mode, int len);

    QString XDSDecodeString(const vector<unsigned char>&,
                            uint start, uint end) const;
    bool XDSDecode(int field, int b1, int b2);

    bool XDSPacketParseProgram(const vector<unsigned char> &xds_buf,
                               bool future);
    bool XDSPacketParseChannel(const vector<unsigned char> &xds_buf);
    void XDSPacketParse(const vector<unsigned char> &xds_buf);
    bool XDSPacketCRC(const vector<unsigned char> &xds_buf);

    CC608Input     *m_reader                {nullptr};

    bool            m_ignore_time_code      {false};

    time_t          m_last_seen[4]          {0};

    // per-field
    int             m_badvbi[2]             { 0,  0};
    int             m_lasttc[2]             { 0,  0};
    int             m_lastcode[2]           {-1, -1};
    int             m_lastcodetc[2]         { 0,  0};
    int             m_ccmode[2]             {-1, -1}; // 0=cc1/txt1, 1=cc2/txt2
    int             m_xds[2]                { 0,  0};
    int             m_txtmode[4]            { 0,  0,  0,  0};

    // per-mode state
    int             m_lastrow[8]            {0};
    int             m_newrow[8]             {0};
    int             m_newcol[8]             {0};
    int             m_newattr[8]            {0}; // color+italic+underline
    int             m_timecode[8]           {0};
    int             m_row[8]                {0};
    int             m_col[8]                {0};
    int             m_rowcount[8]           {0};
    int             m_style[8]              {0};
    int             m_linecont[8]           {0};
    int             m_resumetext[8]         {0};
    int             m_lastclr[8]            {0};
    QString         m_ccbuf[8];

    // translation table
    QChar           m_stdchar[128];

    // temporary buffer
    unsigned char  *m_rbuf                  {nullptr};
    int             m_last_format_tc[2]     {0, 0};
    int             m_last_format_data[2]   {0, 0};

    // VPS data
    char            m_vps_pr_label[20]      {0};
    char            m_vps_label[20]         {0};
    int             m_vps_l                 {0};

    // WSS data
    uint            m_wss_flags             {0};
    bool            m_wss_valid             {false};

    int             m_xds_cur_service       {-1};
    vector<unsigned char> m_xds_buf[7];
    uint            m_xds_crc_passed        {0};
    uint            m_xds_crc_failed        {0};

    mutable QMutex  m_xds_lock              {QMutex::Recursive};
    uint            m_xds_rating_systems[2] {0};
    uint            m_xds_rating[2][4]      {{0}};
    QString         m_xds_program_name[2];
    vector<uint>    m_xds_program_type[2];

    QString         m_xds_net_call;
    QString         m_xds_net_name;
    uint            m_xds_tsid              {0};

    QString         m_xds_program_type_string[96];
};

#endif
