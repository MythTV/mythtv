#ifndef MYTHHDR_H
#define MYTHHDR_H

// Qt
#include <QObject>

// Std
#include <memory>

// MythTV
#include "mythuiexp.h"
#include "mythcolourspace.h"

using MythHDRMetaPtr = std::shared_ptr<class MythHDRMetadata>;
class MUI_PUBLIC MythHDRMetadata
{
  public:
    MythHDRMetadata() = default;
    explicit MythHDRMetadata(const MythHDRMetadata& Other) = default;
    bool Equals(MythHDRMetadata* Other);

    MythPrimariesUInt16 m_displayPrimaries {{{ 0 }}};
    MythPrimaryUInt16 m_whitePoint          {{ 0 }};
    uint16_t m_maxMasteringLuminance         { 0 };
    uint16_t m_minMasteringLuminance         { 0 };
    uint16_t m_maxContentLightLevel          { 0 };
    uint16_t m_maxFrameAverageLightLevel     { 0 };
};

using MythHDRPtr = std::shared_ptr<class MythHDR>;
class MUI_PUBLIC MythHDR
{
    Q_GADGET

  public:
    enum HDRType
    {
        SDR   = 0x00,
        HDR10 = 0x01,
        HLG   = 0x02
    };
    Q_DECLARE_FLAGS(HDRTypes, HDRType)
    Q_FLAG(HDRTypes)

    enum HDRMeta
    {
        Unknown     = 0,
        StaticType1 = 1
    };

    static MythHDRPtr  Create();

    static QString     TypeToString (HDRType Type);
    static QStringList TypesToString(HDRTypes Types);
    QStringList        TypesToString() const;

    bool     m_controllable    { false };
    HDRType  m_currentType     { SDR };
    HDRTypes m_supportedTypes  { SDR };
    double   m_maxLuminance    { 0.0 };
    double   m_maxAvgLuminance { 0.0 };
    double   m_minLuminance    { 0.0 };
    HDRMeta  m_metadataType    { Unknown };
    MythHDRMetaPtr m_metadata  { nullptr };

  protected:
    MythHDR() = default;

  private:
    Q_DISABLE_COPY(MythHDR)
};

#endif
