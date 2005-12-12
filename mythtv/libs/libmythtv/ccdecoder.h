#ifndef CCDECODER_H_
#define CCDECODER_H_

#include <stdint.h>

#include <qstringlist.h>

#include "format.h"

class CCReader
{
  public:
    virtual ~CCReader() { }
    virtual void AddTextData(unsigned char *buf, int len,
                             long long timecode, char type) = 0;
};

class CCDecoder
{
  public:
    CCDecoder(CCReader *ccr);
    ~CCDecoder();

    void FormatCC(int tc, int code1, int code2);
    void FormatCCField(int tc, int field, int data);

    void DecodeVPS(const unsigned char *buf);
    void DecodeWSS(const unsigned char *buf);

  private:
    QChar CharCC(int code) const { return stdchar[code]; }
    void ResetCC(int mode);
    void BufferCC(int mode, int len, int clr);
    int NewRowCC(int mode, int len);

    CCReader *reader;

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
};

#endif
