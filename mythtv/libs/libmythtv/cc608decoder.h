// -*- Mode: c++ -*-

#ifndef CCDECODER_H_
#define CCDECODER_H_

#include <time.h>
#include <stdint.h>

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QChar>

#include "format.h"

class CC608Input
{
  public:
    virtual ~CC608Input() { }
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
    CC608Decoder(CC608Input *ccr);
    CC608Decoder(const CC608Decoder& rhs);
    ~CC608Decoder();

    void FormatCC(int tc, int code1, int code2);
    void FormatCCField(int tc, int field, int data);
    int FalseDup(int tc, int field, int data);

    void DecodeVPS(const unsigned char *buf);
    void DecodeWSS(const unsigned char *buf);

    void SetIgnoreTimecode(bool val) { ignore_time_code = val; }

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
    QChar CharCC(int code) const { return stdchar[code]; }
    void ResetCC(int mode);
    void BufferCC(int mode, int len, int clr);
    int NewRowCC(int mode, int len);

    QString XDSDecodeString(const vector<unsigned char>&,
                            uint string, uint end) const;
    bool XDSDecode(int field, int b1, int b2);

    bool XDSPacketParseProgram(const vector<unsigned char> &xds_buf,
                               bool future);
    bool XDSPacketParseChannel(const vector<unsigned char> &xds_buf);
    void XDSPacketParse(const vector<unsigned char> &xds_buf);
    bool XDSPacketCRC(const vector<unsigned char> &xds_buf);

    CC608Input *reader;

    bool ignore_time_code;

    time_t last_seen[4];

    // per-field
    int badvbi[2];
    int lasttc[2];
    int lastcode[2];
    int lastcodetc[2];
    int ccmode[2];      // 0=cc1/txt1, 1=cc2/txt2
    int xds[2];
    int txtmode[4];

    // per-mode state
    int lastrow[8];
    int newrow[8];
    int newcol[8];
    int newattr[8]; // color+italic+underline
    int timecode[8];
    int row[8];
    int col[8];
    int rowcount[8];
    int style[8];
    int linecont[8];
    int resumetext[8];
    int lastclr[8];
    QString ccbuf[8];

    // translation table
    QChar stdchar[128];

    // temporary buffer
    unsigned char *rbuf;
    int last_format_tc[2];
    int last_format_data[2];

    // VPS data
    char            vps_pr_label[20];
    char            vps_label[20];
    int             vps_l;

    // WSS data
    uint            wss_flags;
    bool            wss_valid;

    int             xds_cur_service;
    vector<unsigned char> xds_buf[7];
    uint            xds_crc_passed;
    uint            xds_crc_failed;

    mutable QMutex  xds_lock;
    uint            xds_rating_systems[2];
    uint            xds_rating[2][4];
    QString         xds_program_name[2];
    vector<uint>    xds_program_type[2];

    QString         xds_net_call;
    QString         xds_net_name;
    uint            xds_tsid;

    QString         xds_program_type_string[96];
};

#endif
