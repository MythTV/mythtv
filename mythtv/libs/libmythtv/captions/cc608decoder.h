// -*- Mode: c++ -*-

#ifndef CCDECODER_H_
#define CCDECODER_H_

#include <cstdint>

#include <array>
#include <vector>

#include <QString>
#include <QRecursiveMutex>
#include <QChar>

#include "libmythbase/mythchrono.h"

struct teletextsubtitle
{
    unsigned char row;
    unsigned char col;
    unsigned char dbl;
    unsigned char fg;
    unsigned char bg;
    unsigned char len;
};

struct ccsubtitle
{
    unsigned char row;
    unsigned char rowcount;
    unsigned char resumedirect;
    unsigned char resumetext;
    unsigned char clr; // clear the display
    unsigned char len; //length of string to follow
};

// resumedirect codes
enum CC_STYLE : std::uint8_t {
    CC_STYLE_POPUP  = 0x00,
    CC_STYLE_PAINT  = 0x01,
    CC_STYLE_ROLLUP = 0x02,
};

// resumetext special codes
static constexpr uint8_t CC_LINE_CONT  { 0x02 };
static constexpr uint8_t CC_MODE_MASK  { 0xf0 };
static constexpr uint8_t CC_TXT_MASK   { 0x20 };
enum CC_MODE : std::uint8_t {
    CC_CC1  = 0x00,
    CC_CC2  = 0x10,
    CC_TXT1 = 0x20,
    CC_TXT2 = 0x30,
    CC_CC3  = 0x40,
    CC_CC4  = 0x50,
    CC_TXT3 = 0x60,
    CC_TXT4 = 0x70,
};

using CC608Seen        = std::array<bool,4>;
using CC608ProgramType = std::array<QString,96>;
using CC608PerField    = std::array<int,2>;
using CC608PerFieldTc  = std::array<std::chrono::milliseconds,2>;
using CC608PerMode     = std::array<int,8>;
using CC608PerModeTc   = std::array<std::chrono::milliseconds,8>;

class CC608Input
{
  public:
    virtual ~CC608Input() = default;
    virtual void AddTextData(unsigned char *buf, int len,
                             std::chrono::milliseconds timecode, char type) = 0;
};

enum : std::uint8_t
{
    kHasMPAA       = 0x1,
    kHasTPG        = 0x2,
    kHasCanEnglish = 0x4,
    kHasCanFrench  = 0x8,
};
enum : std::uint8_t
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

    void FormatCC(std::chrono::milliseconds tc, int code1, int code2);
    void FormatCCField(std::chrono::milliseconds tc, size_t field, int data);
    bool FalseDup(std::chrono::milliseconds tc, int field, int data);

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
    void GetServices(std::chrono::seconds seconds, CC608Seen& seen) const;

    static QString ToASCII(const QString &cc608, bool suppress_unknown);

  private:
    QChar CharCC(int code) const { return m_stdChar[code]; }
    void ResetCC(size_t mode);
    void BufferCC(size_t mode, int len, int clr);
    int NewRowCC(size_t mode, int len);

    void FormatTextCode(std::chrono::milliseconds tc, size_t field, size_t mode, size_t len, int b1, int b2);
    void FormatControlCode(std::chrono::milliseconds tc, size_t field, int b1, int b2);
    QString XDSDecodeString(const std::vector<unsigned char>&buf,
                            uint start, uint end) const;
    bool XDSDecode(int field, int b1, int b2);

    bool XDSPacketParseProgram(const std::vector<unsigned char> &xds_buf,
                               bool future);
    bool XDSPacketParseChannel(const std::vector<unsigned char> &xds_buf);
    void XDSPacketParse(const std::vector<unsigned char> &xds_buf);
    bool XDSPacketCRC(const std::vector<unsigned char> &xds_buf);

    CC608Input     *m_reader                {nullptr};

    bool            m_ignoreTimeCode        {false};

    std::array<SystemTime,4> m_lastSeen     {};

    // per-field
    CC608PerField   m_badVbi                { 0,  0};
    CC608PerFieldTc m_lastTc                { 0ms,  0ms};
    CC608PerField   m_lastCode              {-1, -1};
    CC608PerFieldTc m_lastCodeTc            { 0ms,  0ms};
    CC608PerField   m_ccMode                {-1, -1}; // 0=cc1/txt1, 1=cc2/txt2
    CC608PerField   m_xds                   { 0,  0};
    std::array<int,4> m_txtMode             { 0,  0,  0,  0};

    // per-mode state
    CC608PerMode    m_lastRow               {0};
    CC608PerMode    m_newRow                {0};
    CC608PerMode    m_newCol                {0};
    CC608PerMode    m_newAttr               {0}; // color+italic+underline
    CC608PerModeTc  m_timeCode              {0ms};
    CC608PerMode    m_row                   {0};
    CC608PerMode    m_col                   {0};
    CC608PerMode    m_rowCount              {0};
    CC608PerMode    m_style                 {0};
    CC608PerMode    m_lineCont              {0};
    CC608PerMode    m_resumeText            {0};
    CC608PerModeTc  m_lastClr               {0ms};
    std::array<QString,8> m_ccBuf;

    // translation table
    std::array<QChar,128> m_stdChar;

    // temporary buffer
    unsigned char  *m_rbuf                  {nullptr};
    CC608PerFieldTc    m_lastFormatTc       {0ms, 0ms};
    CC608PerField      m_lastFormatData     {0, 0};

    // VPS data
    std::array<char,20> m_vpsPrLabel        {0};
    std::array<char,20> m_vpsLabel          {0};
    int             m_vpsL                  {0};

    // WSS data
    uint            m_wssFlags              {0};
    bool            m_wssValid              {false};

    int             m_xdsCurService         {-1};
    std::array<std::vector<unsigned char>,7> m_xdsBuf;
    uint            m_xdsCrcPassed          {0};
    uint            m_xdsCrcFailed          {0};

    mutable QRecursiveMutex  m_xdsLock;
    std::array<uint,2> m_xdsRatingSystems   {0};
    std::array<std::array<uint,4>,2> m_xdsRating       {{}};
    std::array<QString,2>            m_xdsProgramName;
    std::array<std::vector<uint>,2>  m_xdsProgramType;

    QString         m_xdsNetCall;
    QString         m_xdsNetName;
    uint            m_xdsTsid               {0};

    CC608ProgramType  m_xdsProgramTypeString;
};

#endif
