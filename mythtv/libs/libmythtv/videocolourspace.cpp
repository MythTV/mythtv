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

const VideoColourSpace::ColourPrimaries VideoColourSpace::kBT709 =
    {{{0.640F, 0.330F}, {0.300F, 0.600F}, {0.150F, 0.060F}}, {0.3127F, 0.3290F}};
const VideoColourSpace::ColourPrimaries VideoColourSpace::kBT610_525 =
    {{{0.640F, 0.340F}, {0.310F, 0.595F}, {0.155F, 0.070F}}, {0.3127F, 0.3290F}};
const VideoColourSpace::ColourPrimaries VideoColourSpace::kBT610_625 =
    {{{0.640F, 0.330F}, {0.290F, 0.600F}, {0.150F, 0.060F}}, {0.3127F, 0.3290F}};
const VideoColourSpace::ColourPrimaries VideoColourSpace::kBT2020 =
    {{{0.708F, 0.292F}, {0.170F, 0.797F}, {0.131F, 0.046F}}, {0.3127F, 0.3290F}};

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
  : ReferenceCounter("Colour"),
    m_parent(Parent)
{
    if (m_parent)
    {
        m_parent->IncrRef();
        connect(m_parent, &VideoColourSpace::PictureAttributeChanged, this, &VideoColourSpace::SetPictureAttribute);
        m_supportedAttributes = m_parent->SupportedAttributes();
        m_dbSettings[kPictureAttribute_Brightness] = m_parent->GetPictureAttribute(kPictureAttribute_Brightness);
        m_dbSettings[kPictureAttribute_Contrast]   = m_parent->GetPictureAttribute(kPictureAttribute_Contrast);
        m_dbSettings[kPictureAttribute_Colour]     = m_parent->GetPictureAttribute(kPictureAttribute_Colour);
        m_dbSettings[kPictureAttribute_Hue]        = m_parent->GetPictureAttribute(kPictureAttribute_Hue);
        m_dbSettings[kPictureAttribute_Range]      = m_parent->GetPictureAttribute(kPictureAttribute_Range);
        m_primariesMode = m_parent->GetPrimariesMode();
    }
    else
    {
        m_dbSettings[kPictureAttribute_Brightness] = gCoreContext->GetNumSetting("PlaybackBrightness", 50);
        m_dbSettings[kPictureAttribute_Contrast]   = gCoreContext->GetNumSetting("PlaybackContrast",   50);
        m_dbSettings[kPictureAttribute_Colour]     = gCoreContext->GetNumSetting("PlaybackColour",     50);
        m_dbSettings[kPictureAttribute_Hue]        = gCoreContext->GetNumSetting("PlaybackHue",        0);
        m_dbSettings[kPictureAttribute_Range]      = gCoreContext->GetBoolSetting("GUIRGBLevels",      true);
    }

    SetBrightness(m_dbSettings[kPictureAttribute_Brightness]);
    SetContrast(m_dbSettings[kPictureAttribute_Contrast]);
    SetSaturation(m_dbSettings[kPictureAttribute_Colour]);
    SetHue(m_dbSettings[kPictureAttribute_Hue]);
    SetFullRange(m_dbSettings[kPictureAttribute_Range]);
    m_updatesDisabled = false;
    Update();
}

VideoColourSpace::~VideoColourSpace()
{
    delete m_customDisplayPrimaries;
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
    // This is a no-op when using full range MJPEG sources and full range output
    // or 'standard' limited range MPEG sources with limited range output.
    // N.B all of the quantization parameters scale perfectly between the different
    // standards and bitdepths (i.e. 709 8 and 10 bit, 2020 10 and 12bit).
    // In the event that we are displaying a downsampled format, the following
    // also effectively limits the precision in line with that loss in precision.
    // For example, YUV420P10 is downsampled by removing the 2 lower bits of
    // precision. We identify the resultant YUV420P frame as 8bit and calculate
    // the quantization/range accordingly.
    bool expand   = (m_range == AVCOL_RANGE_MPEG) && m_fullRange;
    bool contract = (m_range == AVCOL_RANGE_JPEG) && !m_fullRange;
    bool noop     = !expand && !contract;
    float depth        =  (1 <<  m_colourSpaceDepth) - 1;
    float blacklevel   =  16 << (m_colourSpaceDepth - 8);
    float lumapeak     = 235 << (m_colourSpaceDepth - 8);
    float chromapeak   = 240 << (m_colourSpaceDepth - 8);
    float luma_scale   = noop ? 1.0F : (expand ? depth / (lumapeak - blacklevel)   : (lumapeak - blacklevel) / depth);
    float chroma_scale = noop ? 1.0F : (expand ? depth / (chromapeak - blacklevel) : (chromapeak - blacklevel) / depth);
    float offset       = noop ? 0.0F : (expand ? -blacklevel / depth               : blacklevel / depth);

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
    if ((m_colourSpaceDepth > 8) && !m_colourShifted)
    {
        float scaler = 65535.0F / ((1 << m_colourSpaceDepth) -1);
        scale(scaler);
    }
    static_cast<QMatrix4x4*>(this)->operator = (this->transposed());

    // check for a change in primaries conversion. This will need a recompile
    // of the shaders - not just a parameter update.
    float tmpsrcgamma = m_colourGamma;
    float tmpdspgamma = m_displayGamma;
    QMatrix4x4 tmpmatrix = m_primaryMatrix;
    m_primaryMatrix = GetPrimaryConversion(m_colourPrimaries, m_displayPrimaries);
    bool primchanged = !qFuzzyCompare(tmpsrcgamma, m_colourGamma) ||
                       !qFuzzyCompare(tmpdspgamma, m_displayGamma) ||
                       !qFuzzyCompare(tmpmatrix, m_primaryMatrix);
    Debug();
    emit Updated(primchanged);
}

void VideoColourSpace::Debug(void)
{
    bool primary = !m_primaryMatrix.isIdentity();

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Brightness: %1 Contrast: %2 Saturation: %3 Hue: %4 Alpha: %5 Range: %6 Primary: %7")
        .arg(static_cast<qreal>(m_brightness), 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_contrast)  , 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_saturation), 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_hue)       , 2, 'f', 4, QLatin1Char('0'))
        .arg(static_cast<qreal>(m_alpha)     , 2, 'f', 4, QLatin1Char('0'))
        .arg(m_fullRange ? "Full" : "Limited")
        .arg(primary));

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        QString stream;
        QDebug debug(&stream);
        debug << *this;
        if (primary)
            debug << m_primaryMatrix;
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

    int csp      = Frame->colorspace;
    int primary  = Frame->colorprimaries;
    int transfer = Frame->colortransfer;
    int raw      = csp;
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
    if (range ==  AVCOL_RANGE_UNSPECIFIED)
        range = AVCOL_RANGE_MPEG;
    int depth = ColorDepth(format_is_hw(frametype) ? softwaretype : frametype);
    if (csp == AVCOL_SPC_UNSPECIFIED)
        csp = (Frame->width < 1280) ? AVCOL_SPC_BT470BG : AVCOL_SPC_BT709;
    if (primary == AVCOL_PRI_UNSPECIFIED)
        primary = (Frame->width < 1280) ? AVCOL_PRI_BT470BG : AVCOL_PRI_BT709;
    if (transfer == AVCOL_TRC_UNSPECIFIED)
        transfer = (Frame->width < 1280) ? AVCOL_TRC_GAMMA28 : AVCOL_TRC_BT709;
    if ((csp == m_colourSpace) && (m_colourSpaceDepth == depth) &&
        (m_range == range) && (m_colourShifted == Frame->colorshifted) &&
        (primary == m_colourPrimaries))
    {
        return false;
    }

    m_colourSpace      = csp;
    m_colourSpaceDepth = depth;
    m_range            = range;
    m_colourShifted    = Frame->colorshifted;
    m_colourPrimaries  = primary;
    m_colourTransfer   = transfer;

    if (forced)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Forcing inconsistent colourspace - frame format %1")
            .arg(format_description(Frame->codec)));

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Input : %1(%2) Depth:%3 %4Range:%5")
        .arg(av_color_space_name(static_cast<AVColorSpace>(m_colourSpace)))
        .arg(m_colourSpace == raw ? "Detected" : "Guessed")
        .arg(m_colourSpaceDepth)
        .arg((m_colourSpaceDepth > 8) ? (m_colourShifted ? "(Pre-scaled) " : "(Fixed point) ") : "")
        .arg((AVCOL_RANGE_JPEG == m_range) ? "Full" : "Limited"));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Input : Primaries:%1 Transfer: %2")
        .arg(av_color_primaries_name(static_cast<AVColorPrimaries>(m_colourPrimaries)))
        .arg(av_color_transfer_name(static_cast<AVColorTransferCharacteristic>(m_colourTransfer))));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Output: Range:%1 Primaries: %2")
        .arg(m_fullRange ? "Full" : "Limited")
        .arg(m_customDisplayPrimaries ? "Custom (screen)" :
                av_color_primaries_name(static_cast<AVColorPrimaries>(m_displayPrimaries))));

    Update();

    if (!m_primaryMatrix.isIdentity())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Enabled colourspace primaries conversion from %1 to %2")
            .arg(av_color_primaries_name(static_cast<AVColorPrimaries>(m_colourPrimaries)))
            .arg(m_customDisplayPrimaries ? "Custom (screen)" :
                 av_color_primaries_name(static_cast<AVColorPrimaries>(m_displayPrimaries))));
    }
    return true;
}

void VideoColourSpace::SetFullRange(bool FullRange)
{
    m_fullRange = FullRange;
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
    m_hue = Value * -3.6F;
    Update();
}

void VideoColourSpace::SetSaturation(int Value)
{
    m_saturation = Value * 0.02F;
    Update();
}

void VideoColourSpace::SetAlpha(int Value)
{
    m_alpha = 100.0F / Value;
    Update();
}

QStringList VideoColourSpace::GetColourMappingDefines(void)
{
    QStringList result;
    if (m_primaryMatrix.isIdentity())
        return result;

    result << "COLOURMAPPING";
    return result;
}

QMatrix4x4 VideoColourSpace::GetPrimaryMatrix(void)
{
    return m_primaryMatrix;
}

float VideoColourSpace::GetColourGamma(void)
{
    return m_colourGamma;
}

float VideoColourSpace::GetDisplayGamma(void)
{
    return m_displayGamma;
}

PrimariesMode VideoColourSpace::GetPrimariesMode(void)
{
    return m_primariesMode;
}

void VideoColourSpace::SetPrimariesMode(PrimariesMode Mode)
{
    m_primariesMode = Mode;
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

    if (!dbName.isEmpty())
        gCoreContext->SaveSetting(dbName, Value);

    m_dbSettings[AttributeType] = Value;
}

QMatrix4x4 VideoColourSpace::GetPrimaryConversion(int Source, int Dest)
{
    QMatrix4x4 result; // identity
    auto source = static_cast<AVColorPrimaries>(Source);
    auto dest   = static_cast<AVColorPrimaries>(Dest);

    if ((source == dest) || (m_primariesMode == PrimariesDisabled))
        return result;

    ColourPrimaries srcprimaries;
    ColourPrimaries dstprimaries;
    GetPrimaries(source, srcprimaries, m_colourGamma);
    GetPrimaries(dest,   dstprimaries, m_displayGamma);

    // Auto will only enable if there is a significant difference between source
    // and destination. Most people will not notice the difference bt709 and bt610 etc
    // and we avoid extra GPU processing.
    // BT2020 is currently the main target - which is easily differentiated by its gamma.
    if ((m_primariesMode == PrimariesAuto) && qFuzzyCompare(m_colourGamma + 1.0F, m_displayGamma + 1.0F))
        return result;

    // N.B. Custom primaries are not yet implemented but will, some day soon,
    // be read from the EDID
    if (m_customDisplayPrimaries != nullptr)
    {
        dstprimaries = *m_customDisplayPrimaries;
        m_displayGamma = m_customDisplayGamma;
    }

    return (RGBtoXYZ(srcprimaries) * RGBtoXYZ(dstprimaries).inverted());
}

void VideoColourSpace::GetPrimaries(int Primary, ColourPrimaries &Out, float &Gamma)
{
    auto primary = static_cast<AVColorPrimaries>(Primary);
    Gamma = 2.2F;
    switch (primary)
    {
        case AVCOL_PRI_BT470BG:
        case AVCOL_PRI_BT470M:    Out = kBT610_625; return;
        case AVCOL_PRI_SMPTE170M:
        case AVCOL_PRI_SMPTE240M: Out = kBT610_525; return;
        case AVCOL_PRI_BT2020:    Out = kBT2020; Gamma = 2.4F; return;
        default: Out = kBT709; return;
    }
}

inline float CalcBy(const float p[3][2], const float w[2])
{
    float val = ((1-w[0])/w[1] - (1-p[0][0])/p[0][1]) * (p[1][0]/p[1][1] - p[0][0]/p[0][1]) -
    (w[0]/w[1] - p[0][0]/p[0][1]) * ((1-p[1][0])/p[1][1] - (1-p[0][0])/p[0][1]);
    val /= ((1-p[2][0])/p[2][1] - (1-p[0][0])/p[0][1]) * (p[1][0]/p[1][1] - p[0][0]/p[0][1]) -
    (p[2][0]/p[2][1] - p[0][0]/p[0][1]) * ((1-p[1][0])/p[1][1] - (1-p[0][0])/p[0][1]);
    return val;
}

inline float CalcGy(const float p[3][2], const float w[2], const float By)
{
    float val = w[0]/w[1] - p[0][0]/p[0][1] - By * (p[2][0]/p[2][1] - p[0][0]/p[0][1]);
    val /= p[1][0]/p[1][1] - p[0][0]/p[0][1];
    return val;
}

inline float CalcRy(const float By, const float Gy)
{
    return 1.0F - Gy - By;
}

/*! \brief Create a conversion matrix for RGB to XYZ with the given primaries
 *
 * This is a joyous mindbender. There are various explanations on the interweb
 * but this is based on the Kodi implementation - with due credit to Team Kodi.
 *
 * \note We use QMatrix4x4 because QMatrix3x3 has no inverted method.
 */
QMatrix4x4 VideoColourSpace::RGBtoXYZ(ColourPrimaries Primaries)
{
    float By = CalcBy(Primaries.primaries, Primaries.whitepoint);
    float Gy = CalcGy(Primaries.primaries, Primaries.whitepoint, By);
    float Ry = CalcRy(By, Gy);

    float temp[4][4];
    temp[0][0] = Ry * Primaries.primaries[0][0] / Primaries.primaries[0][1];
    temp[0][1] = Gy * Primaries.primaries[1][0] / Primaries.primaries[1][1];
    temp[0][2] = By * Primaries.primaries[2][0] / Primaries.primaries[2][1];
    temp[1][0] = Ry;
    temp[1][1] = Gy;
    temp[1][2] = By;
    temp[2][0] = Ry / Primaries.primaries[0][1] * (1- Primaries.primaries[0][0] - Primaries.primaries[0][1]);
    temp[2][1] = Gy / Primaries.primaries[1][1] * (1- Primaries.primaries[1][0] - Primaries.primaries[1][1]);
    temp[2][2] = By / Primaries.primaries[2][1] * (1- Primaries.primaries[2][0] - Primaries.primaries[2][1]);
    temp[0][3] = temp[1][3] = temp[2][3] = temp[3][0] = temp[3][1] = temp[3][2] = 0.0F;
    temp[3][3] = 1.0F;
    return QMatrix4x4(temp[0]);
}
