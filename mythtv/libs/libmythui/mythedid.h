#ifndef MYTHEDID_H
#define MYTHEDID_H

// Qt
#include <QSize>
#include <QStringList>

// MythTV
#include "mythuiexp.h"
#include "mythhdr.h"
#include "mythcolourspace.h"

// Std
#include <utility>
#include <array>
#include <tuple>

using MythHDRDesc  = std::tuple<MythHDR::HDRTypes,double,double,double>;
using MythVRRRange = std::tuple<int,int,bool>;

class MUI_PUBLIC MythEDID
{
  public:
    MythEDID() = default;
    explicit MythEDID(QByteArray  Data);
    MythEDID(const char* Data, int Length);

    bool        Valid             () const;
    QStringList SerialNumbers     () const;
    QSize       DisplaySize       () const;
    double      DisplayAspect     () const;
    uint16_t    PhysicalAddress   () const;
    float       Gamma             () const;
    bool        IsHDMI            () const;
    bool        IsSRGB            () const;
    bool        IsLikeSRGB        () const;
    MythColourSpace ColourPrimaries() const;
    int         AudioLatency      (bool Interlaced) const;
    int         VideoLatency      (bool Interlaced) const;
    void        Debug             () const;
    MythHDRDesc GetHDRSupport     () const;
    MythVRRRange GetVRRRange      () const;

  private:
    enum HDREOTF : std::uint8_t
    {
        SDR     = 1 << 0,
        HDRTrad = 1 << 1,
        HDR10   = 1 << 2,
        HLG     = 1 << 3
    };

    void        Parse             ();
    bool        ParseBaseBlock    (const quint8* Data);
    void        ParseDisplayDescriptor(const quint8* Data, uint Offset);
    void        ParseDetailedTimingDescriptor(const quint8* Data, size_t Offset);
    bool        ParseCTA861       (const quint8* Data, size_t Offset);
    bool        ParseCTABlock     (const quint8* Data, size_t Offset);
    bool        ParseVSDB         (const quint8* Data, size_t Offset, size_t Length);
    bool        ParseExtended     (const quint8* Data, size_t Offset, size_t Length);

    bool        m_valid           { false };
    QByteArray  m_data;
    size_t      m_size            { 0 };
    quint8      m_minorVersion    { 0 };
    QSize       m_displaySize;    // N.B. Either size or aspect are valid
    double      m_displayAspect   { 0.0 };
    QStringList m_serialNumbers;
    QString     m_name;
    int         m_vrangeMin       { 0 };
    int         m_vrangeMax       { 0 };
    float       m_gamma           { 0.0F }; // Invalid
    bool        m_sRGB            { false };
    bool        m_likeSRGB        { false }; // Temporary until Colourspace handling in libmythui
    MythColourSpace m_primaries   { {{{0.0F}}}, {0.0F} };
    bool        m_isHDMI          { false };
    uint16_t    m_physicalAddress { 0 };
    uint8_t     m_deepColor       { 0 };
    bool        m_latencies       { false };
    bool        m_interLatencies  { false };
    std::array<int,2> m_audioLatency { 0 };
    std::array<int,2> m_videoLatency { 0 };
    uint8_t     m_deepYUV         { 0 };
    int         m_vrrMin          { 0 };
    int         m_vrrMax          { 0 };
    int         m_hdrMetaTypes    { 0 };
    MythHDR::HDRTypes m_hdrSupport { MythHDR::SDR };
    double      m_maxLuminance    { 0.0 };
    double      m_maxAvgLuminance { 0.0 };
    double      m_minLuminance    { 0.0 };
};

#endif
