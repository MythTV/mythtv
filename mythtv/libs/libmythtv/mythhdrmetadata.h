#ifndef MYTHHDRMETADATA_H
#define MYTHHDRMETADATA_H

// Std
#include <array>
#include <memory>

using MythHDRPtr       = std::shared_ptr<class MythHDRMetadata>;
using MythHDRPrimary   = std::array<uint16_t,2>;
using MythHDRPrimaries = std::array<MythHDRPrimary,3>;

class MythHDRMetadata
{
  public:
    static void Populate(class MythVideoFrame* Frame, struct AVFrame* AvFrame);
    MythHDRMetadata() = default;
   ~MythHDRMetadata() = default;
    bool Equals(MythHDRMetadata* Other);

    MythHDRPrimaries m_displayPrimaries {{{ 0 }}};
    MythHDRPrimary m_whitePoint          {{ 0 }};
    uint16_t m_maxMasteringLuminance      { 0 };
    uint16_t m_minMasteringLuminance      { 0 };
    uint16_t m_maxContentLightLevel       { 0 };
    uint16_t m_maxFrameAverageLightLevel  { 0 };

  protected:
    void Update(const struct AVMasteringDisplayMetadata* Display,
                const struct AVContentLightMetadata* Light);
};

#endif
