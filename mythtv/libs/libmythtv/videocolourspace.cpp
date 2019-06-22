// MythTV
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythavutil.h"
#include "videocolourspace.h"

// libavutil
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
}

// Std
#include <cmath>

#define LOC QString("ColourSpace: ")

/*! \class VideoColourSpace
 *  \brief VideoColourSpace contains a QMatrix4x4 that can convert YCbCr data to RGB.
 *
 * A 4x4 matrix is created that is customised for the source colourspace and user
 * defined adjustments for brightness, contrast, hue, saturation (colour) and 'levels'.
 *
 * An alpha value is also added for rendering purposes. This assumes the raw data
 * is in the form YCbCrA.
 *
 * Levels are expanded to the full RGB colourspace range by default but enabling studio
 * levels will ensure there is no adjustment. In both cases it is assumed the display
 * device is setup appropriately.
 *
 * Each instance may have a parent VideoColourSpace. This configuration is used
 * for Picture In Picture support. The master/parent object will receive requests
 * to update the various attributes and will signal changes to the children. Each
 * instance manages its own underlying video colourspace for the stream it is playing.
 * In this way, picture adjustments affect each player in the same way whilst each
 * underlying stream dictates the video colourspace to use. 'Child' instances do
 * not interrogate or update the database settings.
 *
 * \note This class is not complete and will have limitations for certain source material
 * and displays. It currently assumes an 8bit display (and GPU framebuffer) with
 * a 'traditional' gamma of 2.2 (most colourspace standards have a reference gamma
 * of 2.2). Hence colour rendition may not be optimal for material that specificies
 * a colourspace that has a different reference gamma (e.g. Rec 2020) or when
 * using an HDR display. Futher work is required.
*/
VideoColourSpace::VideoColourSpace(VideoColourSpace *Parent)
  : QObject(),
    QMatrix4x4(),
    ReferenceCounter("Colour"),
    m_supportedAttributes(kPictureAttributeSupported_None),
    m_studioLevels(false),
    m_brightness(0.0F),
    m_contrast(1.0F),
    m_saturation(1.0F),
    m_hue(0.0F),
    m_alpha(1.0F),
    m_colourSpace(AVCOL_SPC_UNSPECIFIED),
    m_colourSpaceDepth(8),
    m_range(AVCOL_RANGE_MPEG),
    m_updatesDisabled(true),
    m_colorShifted(0),
    m_parent(Parent)
{
    if (m_parent)
    {
        m_parent->IncrRef();
        connect(m_parent, &VideoColourSpace::PictureAttributeChanged, this, &VideoColourSpace::SetPictureAttribute);
        m_supportedAttributes = m_parent->SupportedAttributes();
        m_dbSettings[kPictureAttribute_Brightness]   = m_parent->GetPictureAttribute(kPictureAttribute_Brightness);
        m_dbSettings[kPictureAttribute_Contrast]     = m_parent->GetPictureAttribute(kPictureAttribute_Contrast);
        m_dbSettings[kPictureAttribute_Colour]       = m_parent->GetPictureAttribute(kPictureAttribute_Colour);
        m_dbSettings[kPictureAttribute_Hue]          = m_parent->GetPictureAttribute(kPictureAttribute_Hue);
        m_dbSettings[kPictureAttribute_StudioLevels] = m_parent->GetPictureAttribute(kPictureAttribute_StudioLevels);
    }
    else
    {
        m_dbSettings[kPictureAttribute_Brightness]   = gCoreContext->GetNumSetting("PlaybackBrightness",   50);
        m_dbSettings[kPictureAttribute_Contrast]     = gCoreContext->GetNumSetting("PlaybackContrast",     50);
        m_dbSettings[kPictureAttribute_Colour]       = gCoreContext->GetNumSetting("PlaybackColour",       50);
        m_dbSettings[kPictureAttribute_Hue]          = gCoreContext->GetNumSetting("PlaybackHue",          0);
        m_dbSettings[kPictureAttribute_StudioLevels] = gCoreContext->GetNumSetting("PlaybackStudioLevels", 0);
    }

    SetBrightness(m_dbSettings[kPictureAttribute_Brightness]);
    SetContrast(m_dbSettings[kPictureAttribute_Contrast]);
    SetSaturation(m_dbSettings[kPictureAttribute_Colour]);
    SetHue(m_dbSettings[kPictureAttribute_Hue]);
    SetStudioLevels(m_dbSettings[kPictureAttribute_StudioLevels]);
    m_updatesDisabled = false;
    Update();
}

VideoColourSpace::~VideoColourSpace()
{
    if (m_parent)
        m_parent->DecrRef();
}

PictureAttributeSupported VideoColourSpace::SupportedAttributes(void) const
{
    return m_supportedAttributes;
}

/*! \brief Enable the given set of picture attributes.
 *
 * This is determined by the video rendering classes and is usually dependant upon
 * the rendering method in use and type of video frame (e.g. hardware decoded or not).
*/
void VideoColourSpace::SetSupportedAttributes(PictureAttributeSupported Supported)
{
    m_supportedAttributes = Supported;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PictureAttributes: %1").arg(toString(m_supportedAttributes)));
}

int VideoColourSpace::GetPictureAttribute(PictureAttribute Attribute)
{
    if (m_dbSettings.contains(Attribute))
        return m_dbSettings.value(Attribute);
    return -1;
}

/// \brief Set the Value for the given PictureAttribute
int VideoColourSpace::SetPictureAttribute(PictureAttribute Attribute, int Value)
{
    if (!(m_supportedAttributes & toMask(Attribute)))
        return -1;

    Value = std::min(std::max(Value, 0), 100);

    switch (Attribute)
    {
        case kPictureAttribute_Brightness:
            SetBrightness(Value);
            break;
        case kPictureAttribute_Contrast:
            SetContrast(Value);
            break;
        case kPictureAttribute_Colour:
            SetSaturation(Value);
            break;
        case kPictureAttribute_Hue:
            SetHue(Value);
            break;
        case kPictureAttribute_StudioLevels:
            Value = std::min(std::max(Value, 0), 1);
            SetStudioLevels(Value > 0);
            break;
        default:
            Value = -1;
    }

    emit PictureAttributeChanged(Attribute, Value);

    if (Value >= 0)
        SaveValue(Attribute, Value);

    return Value;
}

/*! \brief Update the matrix for the current settings and colourspace.
 *
 * The matrix is built from first principles to help with maintainability.
 * This an expensive task but it is only recalculated when a change is detected
 * or notified.
 *
 * Suppport for higher depth colorspaces is available but currently untested.
*/
void VideoColourSpace::Update(void)
{
    if (m_updatesDisabled)
        return;

    // build an RGB to YCbCr conversion matrix from first principles
    // and then invert it for the YCbCr to RGB conversion
    std::vector<float> rgb;
    switch (static_cast<AVColorSpace>(m_colourSpace))
    {
        case AVCOL_SPC_RGB:        rgb = { 1.0000F, 1.0000F, 1.0000F }; break;
        case AVCOL_SPC_BT709:      rgb = { 0.2126F, 0.7152F, 0.0722F }; break;
        case AVCOL_SPC_FCC:        rgb = { 0.30F,   0.59F,   0.11F   }; break;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:  rgb = { 0.299F,  0.587F,  0.114F  }; break;
        case AVCOL_SPC_SMPTE240M:  rgb = { 0.212F,  0.701F,  0.087F  }; break;
        case AVCOL_SPC_YCOCG:      rgb = { 0.25F,   0.5F,    0.25F   }; break;
        case AVCOL_SPC_BT2020_CL:
        case AVCOL_SPC_BT2020_NCL: rgb = { 0.2627F, 0.6780F, 0.0593F }; break;
        case AVCOL_SPC_UNSPECIFIED:
        case AVCOL_SPC_RESERVED:
        case AVCOL_SPC_SMPTE2085:
        case AVCOL_SPC_CHROMA_DERIVED_CL:
        case AVCOL_SPC_CHROMA_DERIVED_NCL:
        case AVCOL_SPC_ICTCP:
        default:                  rgb = { 0.2126F, 0.7152F, 0.0722F }; //Rec.709
    }

    float bs = rgb[2] == 1.0F ? 0.0F : 0.5F / (rgb[2] - 1.0F);
    float rs = rgb[0] == 1.0F ? 0.0F : 0.5F / (rgb[0] - 1.0F);
    QMatrix4x4 rgb2yuv(      rgb[0],      rgb[1],      rgb[2], 0.0F,
                        bs * rgb[0], bs * rgb[1],        0.5F, 0.0F,
                               0.5F, rs * rgb[1], rs * rgb[2], 0.0F,
                               0.0F,        0.0F,        0.0F, m_alpha);

    // TODO check AVCOL_SPC_RGB
    if (m_colourSpace == AVCOL_SPC_YCOCG)
    {
        rgb2yuv = QMatrix4x4(0.25F, 0.50F,  0.25F, 0.0F,
                            -0.25F, 0.50F, -0.25F, 0.0F,
                             0.50F, 0.00F, -0.50F, 0.0F,
                             0.00F, 0.00F,  0.00F, m_alpha);
    }
    QMatrix4x4 yuv2rgb = rgb2yuv.inverted();

    // scale the chroma values for saturation
    yuv2rgb.scale(1.0F, m_saturation, m_saturation);
    // rotate the chroma for hue - this is a rotation around the 'Y' (luminance) axis
    yuv2rgb.rotate(m_hue, 1.0F, 0.0F, 0.0F);
    // denormalise chroma
    yuv2rgb.translate(0.0F, -0.5F, -0.5F);
    // Levels adjustment
    // If using limited/studio levels - this is a no-op.
    // For level expansion, this expands to the full, unadulterated RGB range
    // e.g. 0-255, 0-1023 etc.
    // N.B all of the quantization parameters scale perfectly between the different
    // standards and bitdepths (i.e. 709 8 and 10 bit, 2020 10 and 12bit).
    // In the event that we are displaying a downsampled format, the following
    // also effectively limits the precision in line with that loss in precision.
    // For example, YUV420P10 is downsampled by removing the 2 lower bits of
    // precision. We identify the resultant YUV420P frame as 8bit and calculate
    // the quantization/range accordingly.
    bool fullrange = (m_range == AVCOL_RANGE_JPEG) || m_studioLevels;
    float depth        =  (1 <<  m_colourSpaceDepth) - 1;
    float blacklevel   =  16 << (m_colourSpaceDepth - 8);
    float lumapeak     = 235 << (m_colourSpaceDepth - 8);
    float chromapeak   = 240 << (m_colourSpaceDepth - 8);
    float luma_scale   = fullrange ? 1.0F : depth / (lumapeak - blacklevel);
    float chroma_scale = fullrange ? 1.0F : depth / (chromapeak - blacklevel);
    float offset       = fullrange ? 0.0F : -blacklevel / depth;

    setToIdentity();
    translate(m_brightness, m_brightness, m_brightness);
    scale(m_contrast);
    this->operator *= (yuv2rgb);
    scale(luma_scale, chroma_scale, chroma_scale);
    translate(offset, offset, offset);

    // Scale when needed for 10/12/16bit fixed point data
    // Raw 10bit video is represented as XXXXXXXX:XX000000
    // Raw 12bit video is                XXXXXXXX:XXXX0000
    // Raw 16bit video is                XXXXXXXX:XXXXXXXX
    // and these formats are returned by FFmpeg when software decoding
    // so we need to shift by the appropriate number of 'bits' (actually a float in the shader)
    // Hardware decoders seem to return 'corrected' values
    // i.e. 10bit as XXXXXXXXXX for both direct rendering and copy back.
    // Works for NVDEC and VAAPI. VideoToolBox untested.
    if ((m_colourSpaceDepth > 8) && !m_colorShifted)
    {
        float scaler = 65535.0f / ((1 << m_colourSpaceDepth) -1);
        scale(scaler);
    }
    static_cast<QMatrix4x4*>(this)->operator = (this->transposed());
    Debug();
    emit Updated();
}

void VideoColourSpace::Debug(void)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Brightness: %1 Contrast: %2 Saturation: %3 Hue: %4 Alpha: %5 Range: %6")
        .arg(static_cast<qreal>(m_brightness), 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_contrast)  , 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_saturation), 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_hue)       , 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_alpha)     , 2, 'f', 4, QLatin1Char('0'))
        .arg((AVCOL_RANGE_JPEG == m_range) || m_studioLevels ? "Full" : "Limited"));

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        QString stream;
        QDebug debug(&stream);
        debug << *this;
        LOG(VB_PLAYBACK, LOG_DEBUG, stream);
    }
}

/*! \brief Set the current colourspace to use.
 *
 * We rely on FFmpeg to detect and report the correct colourspace. In the event
 * that no colourspace is found we use sensible defaults for standard and high
 * definition content (BT470BG/BT601 and BT709 respectively).
*/
bool VideoColourSpace::UpdateColourSpace(const VideoFrame *Frame)
{
    if (!Frame)
        return false;

    int csp = Frame->colorspace;
    int raw = csp;
    VideoFrameType frametype = Frame->codec;
    VideoFrameType softwaretype = PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));

    // workaround for NVDEC. NVDEC defaults to a colorspace of 0 - which happens
    // to equate to RGB. In testing, NVDEC reports the same colourspace as FFmpeg
    // software decode for MPEG2, MPEG4, H.264, HEVC and VP8. VP9 seems to go wrong (with limited samples)
    bool forced = false;
    if (csp == AVCOL_SPC_RGB && (format_is_yuv(frametype) || frametype == FMT_NVDEC))
    {
        forced = true;
        csp = AVCOL_SPC_UNSPECIFIED;
    }
    int range = Frame->colorrange;
    int depth = ColorDepth(format_is_hw(frametype) ? softwaretype : frametype);
    if (csp == AVCOL_SPC_UNSPECIFIED)
        csp = (Frame->width < 1280) ? AVCOL_SPC_BT470BG : AVCOL_SPC_BT709;
    if ((csp == m_colourSpace) && (m_colourSpaceDepth == depth) &&
        (m_range == range) && (m_colorShifted == Frame->colorshifted))
    {
        return false;
    }

    m_colourSpace = csp;
    m_colourSpaceDepth = depth;
    m_range = range;
    m_colorShifted = Frame->colorshifted;

    if (forced)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Forcing inconsistent colourspace - frame format %1")
            .arg(format_description(Frame->codec)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Video Colourspace:%1(%2) Depth:%3 %4Range:%5")
        .arg(av_color_space_name(static_cast<AVColorSpace>(m_colourSpace)))
        .arg(m_colourSpace == raw ? "Reported" : "Guessed")
        .arg(m_colourSpaceDepth)
        .arg((m_colourSpaceDepth > 8) ? (m_colorShifted ? "(Pre-scaled) " : "(Fixed point) ") : "")
        .arg((AVCOL_RANGE_JPEG == m_range) ? "Full" : "Limited"));

    Update();
    return true;
}

void VideoColourSpace::SetStudioLevels(bool Studio)
{
    m_studioLevels = Studio;
    Update();
}

void VideoColourSpace::SetBrightness(int Value)
{
    m_brightness = (Value * 0.02F) - 1.0F;
    Update();
}

void VideoColourSpace::SetContrast(int Value)
{
    m_contrast = Value * 0.02F;
    Update();
}

void VideoColourSpace::SetHue(int Value)
{
    m_hue = Value * -3.6f;
    Update();
}

void VideoColourSpace::SetSaturation(int Value)
{
    m_saturation = Value * 0.02F;
    Update();
}

void VideoColourSpace::SetAlpha(int Value)
{
    m_alpha = 100.0f / Value;
    Update();
}

/// \brief Save the PictureAttribute value to the database.
void VideoColourSpace::SaveValue(PictureAttribute AttributeType, int Value)
{
    // parent owns the database settings
    if (m_parent)
        return;

    QString dbName;
    if (kPictureAttribute_Brightness == AttributeType)
        dbName = "PlaybackBrightness";
    else if (kPictureAttribute_Contrast == AttributeType)
        dbName = "PlaybackContrast";
    else if (kPictureAttribute_Colour == AttributeType)
        dbName = "PlaybackColour";
    else if (kPictureAttribute_Hue == AttributeType)
        dbName = "PlaybackHue";
    else if (kPictureAttribute_StudioLevels == AttributeType)
        dbName = "PlaybackStudioLevels";

    if (!dbName.isEmpty())
        gCoreContext->SaveSetting(dbName, Value);

    m_dbSettings[AttributeType] = Value;
}
