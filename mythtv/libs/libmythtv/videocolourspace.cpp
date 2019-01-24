#include <cmath>
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "videocolourspace.h"

#define LOC QString("ColourSpace: ")

VideoColourSpace::VideoColourSpace(VideoCStd colour_std)
  : QMatrix4x4(),
    m_supported_attributes(kPictureAttributeSupported_None),
    m_changed(false),
    m_studioLevels(false),
    m_brightness(0.0f),
    m_contrast(1.0f),
    m_saturation(1.0f),
    m_hue(0.0f),
    m_colourSpace(colour_std)
{
    m_db_settings[kPictureAttribute_Brightness] =
        gCoreContext->GetNumSetting("PlaybackBrightness",   50);
    m_db_settings[kPictureAttribute_Contrast] =
        gCoreContext->GetNumSetting("PlaybackContrast",     50);
    m_db_settings[kPictureAttribute_Colour] =
        gCoreContext->GetNumSetting("PlaybackColour",       50);
    m_db_settings[kPictureAttribute_Hue] =
        gCoreContext->GetNumSetting("PlaybackHue",          0);
    m_db_settings[kPictureAttribute_StudioLevels] =
        gCoreContext->GetNumSetting("PlaybackStudioLevels", 0);

    SetBrightness(m_db_settings[kPictureAttribute_Brightness]);
    SetContrast(m_db_settings[kPictureAttribute_Contrast]);
    SetSaturation(m_db_settings[kPictureAttribute_Colour]);
    SetHue(m_db_settings[kPictureAttribute_Hue]);
    SetStudioLevels(m_db_settings[kPictureAttribute_StudioLevels]);
}

bool VideoColourSpace::HasChanged(void)
{
    bool result = m_changed;
    m_changed = false;
    return result;
}

PictureAttributeSupported VideoColourSpace::SupportedAttributes(void) const
{
    return m_supported_attributes;
}

void VideoColourSpace::SetSupportedAttributes(PictureAttributeSupported supported)
{
    m_supported_attributes = supported;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PictureAttributes: %1")
        .arg(toString(m_supported_attributes)));
}

int VideoColourSpace::GetPictureAttribute(PictureAttribute attribute)
{
    if (m_db_settings.contains(attribute))
        return m_db_settings.value(attribute);
    return -1;
}

int VideoColourSpace::SetPictureAttribute(PictureAttribute attribute, int value)
{
    if (!(m_supported_attributes & toMask(attribute)))
        return -1;

    value = std::min(std::max(value, 0), 100);

    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            SetBrightness(value);
            break;
        case kPictureAttribute_Contrast:
            SetContrast(value);
            break;
        case kPictureAttribute_Colour:
            SetSaturation(value);
            break;
        case kPictureAttribute_Hue:
            SetHue(value);
            break;
        case kPictureAttribute_StudioLevels:
            value = std::min(std::max(value, 0), 1);
            SetStudioLevels(value > 0);
            break;
        default:
            value = -1;
    }

    if (value >= 0)
        SaveValue(attribute, value);

    return value;
}

void VideoColourSpace::Update(void)
{
    float luma_range    = m_studioLevels ? 255.0f : 219.0f;
    float chroma_range  = m_studioLevels ? 255.0f : 224.0f;
    float luma_offset   = m_studioLevels ? 0.0f   : -16.0f / 255.0f;
    float chroma_offset = -128.0f / 255.0f;

    float uvcos         = m_saturation * cosf(m_hue);
    float uvsin         = m_saturation * sinf(m_hue);
    float brightness    = m_brightness * 255.0f / luma_range;
    float luma_scale    = 255.0f / luma_range;
    float chroma_scale  = 255.0f / chroma_range;

    QMatrix4x4 csc;
    switch (m_colourSpace)
    {
        case kCSTD_SMPTE_240M:
            csc = QMatrix4x4(
            1.000f,                      ( 1.5756f * uvsin), ( 1.5756f * uvcos)                     , 0.000f,
            1.000f, (-0.2253f * uvcos) + ( 0.5000f * uvsin), ( 0.5000f * uvcos) - (-0.2253f * uvsin), 0.000f,
            1.000f, ( 1.8270f * uvcos)                     ,                    - ( 1.8270f * uvsin), 0.000f,
            0.000f,                                  0.000f,                                  0.000f, 1.000f);
            break;

        case kCSTD_ITUR_BT_709:
            csc = QMatrix4x4(
            1.000f,                      ( 1.5701f * uvsin), ( 1.5701f * uvcos)                     , 0.000f,
            1.000f, (-0.1870f * uvcos) + (-0.4664f * uvsin), (-0.4664f * uvcos) - (-0.1870f * uvsin), 0.000f,
            1.000f, ( 1.8556f * uvcos)                     ,                    - ( 1.8556f * uvsin), 0.000f,
            0.000f,                                  0.000f,                                  0.000f, 1.000f);
            break;

        case kCSTD_ITUR_BT_601:
        default:
            csc = QMatrix4x4(
            1.000f,                      ( 1.4030f * uvsin), ( 1.4030f * uvcos)                     , 0.000f,
            1.000f, (-0.3440f * uvcos) + (-0.7140f * uvsin), (-0.7140f * uvcos) - (-0.3440f * uvsin), 0.000f,
            1.000f, ( 1.7730f * uvcos)                     ,                    - ( 1.7730f * uvsin), 0.000f,
            0.000f,                                  0.000f,                                  0.000f, 1.000f);
    }

    setToIdentity();
    translate(brightness, brightness, brightness);
    scale(m_contrast);
    this->operator *= (csc);
    scale(luma_scale, chroma_scale, chroma_scale);
    translate(luma_offset, chroma_offset, chroma_offset);
    static_cast<QMatrix4x4*>(this)->operator = (this->transposed());
    m_changed = true;
    Debug();
}

void VideoColourSpace::Debug(void)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Brightness: %1 Contrast: %2 Saturation: %3 Hue: %4 "
                "StudioLevels: %5")
        .arg(m_brightness, 2, 'f', 4, QLatin1Char('0'))
        .arg(m_contrast  , 2, 'f', 4, QLatin1Char('0'))
        .arg(m_saturation, 2, 'f', 4, QLatin1Char('0'))
        .arg(m_hue       , 2, 'f', 4, QLatin1Char('0'))
        .arg(m_studioLevels));

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        QString stream;
        QDebug debug(&stream);
        debug << *this;
        LOG(VB_PLAYBACK, LOG_DEBUG, stream);
    }
}

void VideoColourSpace::SetColourSpace(VideoCStd csp)
{
    m_colourSpace = csp;
    Update();
}

void VideoColourSpace::SetStudioLevels(bool studio)
{
    m_studioLevels = studio;
    Update();
}

void VideoColourSpace::SetBrightness(int value)
{
    m_brightness = (value * 0.02f) - 1.0f;
    Update();
}

void VideoColourSpace::SetContrast(int value)
{
    m_contrast = value * 0.02f;
    Update();
}

void VideoColourSpace::SetHue(int value)
{
    m_hue = value * (-3.6 * M_PI / 180.0);
    Update();
}

void VideoColourSpace::SetSaturation(int value)
{
    m_saturation = value * 0.02f;
    Update();
}

void VideoColourSpace::SaveValue(PictureAttribute attributeType, int value)
{
    QString dbName;
    if (kPictureAttribute_Brightness == attributeType)
        dbName = "PlaybackBrightness";
    else if (kPictureAttribute_Contrast == attributeType)
        dbName = "PlaybackContrast";
    else if (kPictureAttribute_Colour == attributeType)
        dbName = "PlaybackColour";
    else if (kPictureAttribute_Hue == attributeType)
        dbName = "PlaybackHue";
    else if (kPictureAttribute_StudioLevels == attributeType)
        dbName = "PlaybackStudioLevels";

    if (!dbName.isEmpty())
        gCoreContext->SaveSetting(dbName, value);

    m_db_settings[attributeType] = value;
}
