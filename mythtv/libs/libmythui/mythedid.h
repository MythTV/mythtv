#ifndef MYTHEDID_H
#define MYTHEDID_H

// Qt
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

    MythEDID(void) { }
    MythEDID(QByteArray &Data);
    MythEDID(const char* Data, int Length);

    bool        Valid             (void);
    QStringList SerialNumbers     (void);
    int         PhysicalAddress   (void);
    float       Gamma             (void);
    bool        IsSRGB            (void);
    Primaries   ColourPrimaries   (void);
    int         AudioLatency      (bool Interlaced);
    int         VideoLatency      (bool Interlaced);
    void        Debug             (void);

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
    QStringList m_serialNumbers   { };
    float       m_gamma           { 0.0F }; // Invalid
    bool        m_sRGB            { false };
    Primaries   m_primaries       { {{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}}, {0.0F, 0.0F} };
    int         m_physicalAddress { 0 };
    bool        m_latencies       { false };
    bool        m_interLatencies  { false };
    int         m_audioLatency[2] { 0 };
    int         m_videoLatency[2] { 0 };
};

#endif // MYTHEDID_H
