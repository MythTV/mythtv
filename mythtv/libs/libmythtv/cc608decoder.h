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

    void SetIgnoreTimecode(bool val) { m_ignoreTimeCode = val; }

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
    QChar CharCC(int code) const { return m_stdChar[code]; }
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

    bool            m_ignoreTimeCode        {false};

    time_t          m_lastSeen[4]           {0};

    // per-field
    int             m_badVbi[2]             { 0,  0};
    int             m_lastTc[2]             { 0,  0};
    int             m_lastCode[2]           {-1, -1};
    int             m_lastCodeTc[2]         { 0,  0};
    int             m_ccMode[2]             {-1, -1}; // 0=cc1/txt1, 1=cc2/txt2
    int             m_xds[2]                { 0,  0};
    int             m_txtMode[4]            { 0,  0,  0,  0};

    // per-mode state
    int             m_lastRow[8]            {0};
    int             m_newRow[8]             {0};
    int             m_newCol[8]             {0};
    int             m_newAttr[8]            {0}; // color+italic+underline
    int             m_timeCode[8]           {0};
    int             m_row[8]                {0};
    int             m_col[8]                {0};
    int             m_rowCount[8]           {0};
    int             m_style[8]              {0};
    int             m_lineCont[8]           {0};
    int             m_resumeText[8]         {0};
    int             m_lastClr[8]            {0};
    QString         m_ccBuf[8];

    // translation table
    QChar           m_stdChar[128];

    // temporary buffer
    unsigned char  *m_rbuf                  {nullptr};
    int             m_lastFormatTc[2]       {0, 0};
    int             m_lastFormatData[2]     {0, 0};

    // VPS data
    char            m_vpsPrLabel[20]        {0};
    char            m_vpsLabel[20]          {0};
    int             m_vpsL                  {0};

    // WSS data
    uint            m_wssFlags              {0};
    bool            m_wssValid              {false};

    int             m_xdsCurService         {-1};
    vector<unsigned char> m_xdsBuf[7];
    uint            m_xdsCrcPassed          {0};
    uint            m_xdsCrcFailed          {0};

    mutable QMutex  m_xdsLock               {QMutex::Recursive};
    uint            m_xdsRatingSystems[2]   {0};
    uint            m_xdsRating[2][4]       {{0}};
    QString         m_xdsProgramName[2];
    vector<uint>    m_xdsProgramType[2];

    QString         m_xdsNetCall;
    QString         m_xdsNetName;
    uint            m_xdsTsid               {0};

    QString         m_xdsProgramTypeString[96];
};

#endif
