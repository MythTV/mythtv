// MythTV
#include "mythhdr.h"
#include "mythdisplay.h"

#if defined (USING_DRM) && defined (USING_QTPRIVATEHEADERS)
#include "platforms/drm/mythdrmhdr.h"
#endif

/*! \class MythHDRMetadata
 * \brief Encapsulates HDR metadata conformant with Static Metadata Type 1 per CTA-861-G Final.
 */
bool MythHDRMetadata::Equals(MythHDRMetadata* Other)
{
    return Other &&
           m_maxMasteringLuminance     == Other->m_maxMasteringLuminance     &&
           m_minMasteringLuminance     == Other->m_minMasteringLuminance     &&
           m_maxContentLightLevel      == Other->m_maxContentLightLevel      &&
           m_maxFrameAverageLightLevel == Other->m_maxFrameAverageLightLevel &&
           m_whitePoint[0]             == Other->m_whitePoint[0]             &&
           m_whitePoint[1]             == Other->m_whitePoint[1]             &&
           m_displayPrimaries[0][0]    == Other->m_displayPrimaries[0][0]    &&
           m_displayPrimaries[0][1]    == Other->m_displayPrimaries[0][1]    &&
           m_displayPrimaries[1][0]    == Other->m_displayPrimaries[1][0]    &&
           m_displayPrimaries[1][1]    == Other->m_displayPrimaries[1][1]    &&
           m_displayPrimaries[2][0]    == Other->m_displayPrimaries[2][0]    &&
           m_displayPrimaries[2][1]    == Other->m_displayPrimaries[2][1];
}

MythHDRPtr MythHDR::Create([[maybe_unused]] MythDisplay* MDisplay,
                           const MythHDRDesc& Desc)
{
    MythHDRPtr result = nullptr;

    // Only try and create a controllable device if the display supports HDR
    if (std::get<0>(Desc) > SDR)
    {
#if defined (USING_DRM) && defined (USING_QTPRIVATEHEADERS)
        result = MythDRMHDR::Create(MDisplay, Desc);
#endif
    }

    if (!result)
        result = std::shared_ptr<MythHDR>(new MythHDR(Desc));
    return result;
}

MythHDR::MythHDR(const MythHDRDesc& Desc)
  : m_supportedTypes(std::get<0>(Desc)),
    m_minLuminance(std::get<1>(Desc)),
    m_maxAvgLuminance(std::get<2>(Desc)),
    m_maxLuminance(std::get<3>(Desc))
{
}

bool MythHDR::IsControllable() const
{
    return m_controllable;
}

double MythHDR::GetMaxLuminance() const
{
    return m_maxLuminance;
}

QString MythHDR::TypeToString(HDRType Type)
{
    if (Type & HDR10) return QObject::tr("HDR10");
    if (Type & HLG)   return QObject::tr("Hybrid Log-Gamma");
    return QObject::tr("Unknown");
}

QStringList MythHDR::TypesToString(HDRTypes Types)
{
    QStringList res;
    if (Types & HDR10) res << TypeToString(HDR10);
    if (Types & HLG)   res << TypeToString(HLG);
    if (res.empty())   res << QObject::tr("None");
    return res;
}

QStringList MythHDR::TypesToString() const
{
    return MythHDR::TypesToString(m_supportedTypes);
}
