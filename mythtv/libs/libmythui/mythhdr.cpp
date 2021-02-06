// MythTV
#include "mythhdr.h"

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

MythHDRPtr MythHDR::Create()
{
    return std::shared_ptr<MythHDR>(new MythHDR());
}

QString MythHDR::TypeToString(HDRType Type)
{
    if (Type & HDR10)   return QObject::tr("HDR10");
    if (Type & HLG)     return QObject::tr("Hybrid Log-Gamma");
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
