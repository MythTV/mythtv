#ifndef MYTHEDID_H
#define MYTHEDID_H

// C++
#include <array>

// Qt
#include <QSize>
#include <QStringList>

// MythTV
#include "mythuiexp.h"

class MUI_PUBLIC MythEDID
{
  public:
    // This structure matches VideoColourSpace::ColourPrimaries
    // TODO move ColourPrimaries into MythDisplay
    struct Primaries
    {
        float primaries[3][2];
        float whitepoint[2];
    };

    MythEDID(void) = default;
    explicit MythEDID(QByteArray &Data);
    MythEDID(const char* Data, int Length);

    bool        Valid             (void) const;
    QStringList SerialNumbers     (void) const;
    QSize       DisplaySize       (void) const;
    double      DisplayAspect     (void) const;
    uint16_t    PhysicalAddress   (void) const;
    float       Gamma             (void) const;
    bool        IsHDMI            (void) const;
    bool        IsSRGB            (void) const;
    bool        IsLikeSRGB        (void) const;
    Primaries   ColourPrimaries   (void) const;
    int         AudioLatency      (bool Interlaced) const;
    int         VideoLatency      (bool Interlaced) const;
    void        Debug             (void) const;

  private:
    void        Parse             (void);
    bool        ParseBaseBlock    (const quint8 *Data);
    bool        ParseCTA861       (const quint8 *Data, uint Offset);
    bool        ParseCTABlock     (const quint8 *Data, uint Offset);
    bool        ParseVSDB         (const quint8 *Data, uint Offset, uint Length);

    bool        m_valid           { false };
    QByteArray  m_data            { };
    uint        m_size            { 0 };
    quint8      m_minorVersion    { 0 };
    QSize       m_displaySize     { };    // N.B. Either size or aspect are valid
    double      m_displayAspect   { 0.0 };
    QStringList m_serialNumbers   { };
    float       m_gamma           { 0.0F }; // Invalid
    bool        m_sRGB            { false };
    bool        m_likeSRGB        { false }; // Temporary until Colourspace handling in libmythui
    Primaries   m_primaries       { {{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}}, {0.0F, 0.0F} };
    bool        m_isHDMI          { false };
    uint16_t    m_physicalAddress { 0 };
    bool        m_latencies       { false };
    bool        m_interLatencies  { false };
    int         m_audioLatency[2] { 0 };
    int         m_videoLatency[2] { 0 };
};

#endif // MYTHEDID_H
