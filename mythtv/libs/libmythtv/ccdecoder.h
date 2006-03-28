#ifndef CCDECODER_H_
#define CCDECODER_H_

#include <stdint.h>

#include <vector>
using namespace std;

#include <qstringlist.h>

#include "format.h"

class CCReader
{
  public:
    virtual ~CCReader() { }
    virtual void AddTextData(unsigned char *buf, int len,
                             long long timecode, char type) = 0;
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

class CCDecoder
{
  public:
    CCDecoder(CCReader *ccr);
    ~CCDecoder();

    void FormatCC(int tc, int code1, int code2);
    void FormatCCField(int tc, int field, int data);
    int FalseDup(int tc, int field, int data);

    void DecodeVPS(const unsigned char *buf);
    void DecodeWSS(const unsigned char *buf);

    void SetIgnoreTimecode(bool val) { ignore_time_code = val; }

    uint GetRatingSystems(void) const { return xds_rating_systems; }
    uint GetRating(uint i) const { return xds_rating[i&3]&7; }
    QString GetRatingString(uint i) const;

    QString GetXDS(const QString &key) const;

  private:
    QChar CharCC(int code) const { return stdchar[code]; }
    void ResetCC(int mode);
    void BufferCC(int mode, int len, int clr);
    int NewRowCC(int mode, int len);

    QString DecodeXDSString(const vector<unsigned char>&,
                            uint string, uint end) const;
    void DecodeXDSStartTime(int b1, int b2);
    void DecodeXDSProgramLength(int b1, int b2);
    void DecodeXDSProgramName(int b1, int b2);
    void DecodeXDSProgramType(int b1, int b2);
    void DecodeXDSVChip(int b1, int b2);
    void DecodeXDSPacket(int b1, int b2);
    void DecodeXDS(int field, int b1, int b2);

    CCReader *reader;

    bool ignore_time_code;

    // per-field
    int badvbi[2];
    int lasttc[2];
    int lastcode[2];
    int lastcodetc[2];
    int ccmode[2];	// 0=cc1/txt1, 1=cc2/txt2
    int xds[2];
    int txtmode[4];

    // per-mode state
    int lastrow[8];
    int newrow[8];
    int newcol[8];
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

    // VPS data
    char            vps_pr_label[20];
    char            vps_label[20];
    int             vps_l;

    // WSS data
    uint            wss_flags;
    bool            wss_valid;

    // XDS data
    enum
    {
        kXDSStartTime = 0x0101,
        kXDSProgLen   = 0x0102,
        kXDSProgName  = 0x0103,
        kXDSProgType  = 0x0104,
        kXDSVChip     = 0x0105,
        kXDSNetName   = 0x8501,
        kXDSNetCall   = 0x8502,
        kXDSNetCallX  = 0x0501, // reverse engineered
        kXDSNetNameX  = 0x0502, // reverse engineered
        kXDSIgnore    = 0xFFFF,
    };
    uint            xds_current_packet;
    vector<unsigned char> xds_buf;

    uint            xds_rating_systems;
    uint            xds_rating[4];
    QString         xds_program_name;
    QString         xds_net_call;
    QString         xds_net_call_x;
    QString         xds_net_name;
    QString         xds_net_name_x;
};

#endif
