#include "math.h"

#include "compat.h"
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "videocolourspace.h"


#define LOC QString("ColourSpace: ")

Matrix::Matrix(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34)
{
    m[0][0] = m11; m[0][1] = m12; m[0][2] = m13; m[0][3] = m14;
    m[1][0] = m21; m[1][1] = m22; m[1][2] = m23; m[1][3] = m24;
    m[2][0] = m31; m[2][1] = m32; m[2][2] = m33; m[2][3] = m34;
    m[3][0] = m[3][1] = m[3][2] = m[3][3] = 1.0f;
}

Matrix::Matrix()
{
    setToIdentity();
}

void Matrix::setToIdentity(void)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

void Matrix::scale(float val1, float val2, float val3)
{
    Matrix scale;
    scale.m[0][0] = val1;
    scale.m[1][1] = val2;
    scale.m[2][2] = val3;
    this->operator *=(scale);
}

void Matrix::translate(float val1, float val2, float val3)
{
    Matrix translate;
    translate.m[0][3] = val1;
    translate.m[1][3] = val2;
    translate.m[2][3] = val3;
    this->operator *=(translate);
}

Matrix & Matrix::operator*=(const Matrix &r)
{
    for (int i = 0; i < 3; i++)
        product(i, r);
    return *this;
}

void Matrix::product(int row, const Matrix &r)
{
    float t0, t1, t2, t3;
    t0 = m[row][0] * r.m[0][0] + m[row][1] * r.m[1][0] + m[row][2] * r.m[2][0];
    t1 = m[row][0] * r.m[0][1] + m[row][1] * r.m[1][1] + m[row][2] * r.m[2][1];
    t2 = m[row][0] * r.m[0][2] + m[row][1] * r.m[1][2] + m[row][2] * r.m[2][2];
    t3 = m[row][0] * r.m[0][3] + m[row][1] * r.m[1][3] + m[row][2] * r.m[2][3] + m[row][3];
    m[row][0] = t0; m[row][1] = t1; m[row][2] = t2; m[row][3] = t3;
}

void Matrix::debug(void)
{
    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("%1 %2 %3 %4")
        .arg(m[0][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][3], 4, 'f', 4, QLatin1Char('0')));
    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("%1 %2 %3 %4")
        .arg(m[1][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][3], 4, 'f', 4, QLatin1Char('0')));
    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("%1 %2 %3 %4")
        .arg(m[2][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][3], 4, 'f', 4, QLatin1Char('0')));
}

VideoColourSpace::VideoColourSpace(VideoCStd colour_std)
  : m_supported_attributes(kPictureAttributeSupported_None),
    m_changed(false), m_studioLevels(false), m_brightness(0.0f),
    m_contrast(1.0f), m_saturation(1.0f),    m_hue(0.0f),
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

void VideoColourSpace::SetSupportedAttributes(PictureAttributeSupported supported)
{
    m_supported_attributes = supported;
    VERBOSE(VB_PLAYBACK, LOC + QString("PictureAttributes: %1")
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

    float uvcos         = m_saturation * cos(m_hue);
    float uvsin         = m_saturation * sin(m_hue);
    float brightness    = m_brightness * 255.0f / luma_range;
    float luma_scale    = 255.0f / luma_range;
    float chroma_scale  = 255.0f / chroma_range;

    Matrix csc;
    switch (m_colourSpace)
    {
        case kCSTD_SMPTE_240M:
            csc = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.5756f * uvsin),
                                 ( 1.5756f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                         1.000f, (-0.2253f * uvcos) + ( 0.5000f * uvsin),
                                 ( 0.5000f * uvcos) - (-0.2253f * uvsin), 0.0f,
                         1.000f, ( 1.8270f * uvcos) + ( 0.0000f * uvsin),
                                 ( 0.0000f * uvcos) - ( 1.8270f * uvsin), 0.0f);
            break;

        case kCSTD_ITUR_BT_709:
            csc = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.5701f * uvsin),
                                 ( 1.5701f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                         1.000f, (-0.1870f * uvcos) + (-0.4664f * uvsin),
                                 (-0.4664f * uvcos) - (-0.1870f * uvsin), 0.0f,
                         1.000f, ( 1.8556f * uvcos) + ( 0.0000f * uvsin),
                                 ( 0.0000f * uvcos) - ( 1.8556f * uvsin), 0.0f);
            break;

        case kCSTD_ITUR_BT_601:
        default:
            csc = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.4030f * uvsin),
                                 ( 1.4030f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                         1.000f, (-0.3440f * uvcos) + (-0.7140f * uvsin),
                                 (-0.7140f * uvcos) - (-0.3440f * uvsin), 0.0f,
                         1.000f, ( 1.7730f * uvcos) + ( 0.0000f * uvsin),
                                 ( 0.0000f * uvcos) - ( 1.7730f * uvsin), 0.0f);
    }

    m_matrix.setToIdentity();
    m_matrix.translate(brightness, brightness, brightness);
    m_matrix.scale(m_contrast, m_contrast, m_contrast);
    m_matrix *= csc;
    m_matrix.scale(luma_scale, chroma_scale, chroma_scale);
    m_matrix.translate(luma_offset, chroma_offset, chroma_offset);
    m_changed = true;
    Debug();
}

void VideoColourSpace::Debug(void)
{
    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC +
        QString("Brightness: %1 Contrast: %2 Saturation: %3 Hue: %4 StudioLevels: %5")
        .arg(m_brightness, 2, 'f', 4, QLatin1Char('0'))
        .arg(m_contrast  , 2, 'f', 4, QLatin1Char('0'))
        .arg(m_saturation, 2, 'f', 4, QLatin1Char('0'))
        .arg(m_hue       , 2, 'f', 4, QLatin1Char('0'))
        .arg(m_studioLevels));
    m_matrix.debug();
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
    m_hue = value * (-3.6f * M_PI / 180.0f);
    Update();
}

void VideoColourSpace::SetSaturation(int value)
{
    m_saturation = value * 0.02f;
    Update();
}

void VideoColourSpace::SaveValue(PictureAttribute attributeType, int value)
{
    QString dbName = QString::null;
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
