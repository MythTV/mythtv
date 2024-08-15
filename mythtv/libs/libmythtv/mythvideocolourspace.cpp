// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdisplay.h"
#include "mythavutil.h"
#include "libmythui/mythmainwindow.h"
#include "mythvideocolourspace.h"

// libavutil
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
}

// Std
#include <cmath>

#define LOC QString("ColourSpace: ")

/*! \class MythVideoColourSpace
 *  \brief MythVideoColourSpace contains a QMatrix4x4 that can convert YCbCr data to RGB.
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
 * \note This class is not complete and will have limitations for certain source material
 * and displays. It currently assumes an 8bit display (and GPU framebuffer) with
 * a 'traditional' gamma of 2.2 (most colourspace standards have a reference gamma
 * of 2.2). Hence colour rendition may not be optimal for material that specificies
 * a colourspace that has a different reference gamma (e.g. Rec 2020) or when
 * using an HDR display. Futher work is required.
*/
MythVideoColourSpace::MythVideoColourSpace()
  : ReferenceCounter("Colour"),
    m_updatesDisabled(false)
{
    m_dbSettings[kPictureAttribute_Brightness] = gCoreContext->GetNumSetting("PlaybackBrightness", 50);
    m_dbSettings[kPictureAttribute_Contrast]   = gCoreContext->GetNumSetting("PlaybackContrast",   50);
    m_dbSettings[kPictureAttribute_Colour]     = gCoreContext->GetNumSetting("PlaybackColour",     50);
    m_dbSettings[kPictureAttribute_Hue]        = gCoreContext->GetNumSetting("PlaybackHue",        0);
    m_dbSettings[kPictureAttribute_Range]      = static_cast<int>(gCoreContext->GetBoolSetting("GUIRGBLevels", true));
    m_primariesMode = toPrimariesMode(gCoreContext->GetSetting("ColourPrimariesMode", "auto"));

    SetBrightness(m_dbSettings[kPictureAttribute_Brightness]);
    SetContrast(m_dbSettings[kPictureAttribute_Contrast]);
    SetSaturation(m_dbSettings[kPictureAttribute_Colour]);
    SetHue(m_dbSettings[kPictureAttribute_Hue]);
    SetFullRange(m_dbSettings[kPictureAttribute_Range] != 0);

    // This isn't working as intended (most notable on OSX internal display).
    // Presumably the driver is expecting sRGB/Rec709 and handles any final
    // conversion to the display's colourspace.
    /*
    if (HasMythMainWindow())
    {
        MythDisplay* display = MythDisplay::AcquireRelease();
        MythEDID& edid = display->GetEDID();
        // We assume sRGB/Rec709 by default
        bool custom   = edid.Valid() && !edid.IsSRGB();
        bool likesrgb = custom && edid.IsLikeSRGB() && m_primariesMode != PrimariesExact;
        if (custom)
        {
            // Use sRGB if we don't want exact matching (i.e. close is good enough)
            // and the display primaries are similar to sRGB.
            if (likesrgb && qFuzzyCompare(edid.Gamma() + 1.0F, 2.2F + 1.0F))
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "sRGB primaries preferred as close match to display primaries");
            }
            else
            {
                m_customDisplayGamma = edid.Gamma();
                m_customDisplayPrimaries = new ColourPrimaries;
                MythEDID::Primaries displayprimaries = edid.ColourPrimaries();
                memcpy(m_customDisplayPrimaries, &displayprimaries, sizeof(ColourPrimaries));
            }
        }
        MythDisplay::AcquireRelease(false);
    }
    */
    Update();
}

MythVideoColourSpace::~MythVideoColourSpace()
{
    delete m_customDisplayPrimaries;
}

void MythVideoColourSpace::RefreshState()
{
    emit SupportedAttributesChanged(m_supportedAttributes);
    emit PictureAttributesUpdated(m_dbSettings);
}

PictureAttributeSupported MythVideoColourSpace::SupportedAttributes(void) const
{
    return m_supportedAttributes;
}

/*! \brief Enable the given set of picture attributes.
 *
 * This is determined by the video rendering classes and is usually dependant upon
 * the rendering method in use and type of video frame (e.g. hardware decoded or not).
*/
void MythVideoColourSpace::SetSupportedAttributes(PictureAttributeSupported Supported)
{
    if (Supported == m_supportedAttributes)
        return;
    m_supportedAttributes = Supported;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PictureAttributes: %1").arg(toString(m_supportedAttributes)));
    emit SupportedAttributesChanged(m_supportedAttributes);
}

int MythVideoColourSpace::GetPictureAttribute(PictureAttribute Attribute)
{
    if (auto found = m_dbSettings.find(Attribute); found != m_dbSettings.end())
        return found->second;
    return -1;
}

/*! \brief Update the matrix for the current settings and colourspace.
 *
 * The matrix is built from first principles to help with maintainability.
 * This is an expensive task but it is only recalculated when a change is detected
 * or notified.
*/
void MythVideoColourSpace::Update(void)
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
    float luma_scale   {NAN};
    float chroma_scale {NAN};
    float offset       {NAN};
    if (noop)
    {
        luma_scale   = 1.0F;
        chroma_scale = 1.0F;
        offset       = 0.0F;
    }
    else if (expand)
    {
        luma_scale   = depth / (lumapeak - blacklevel);
        chroma_scale = depth / (chromapeak - blacklevel);
        offset       = -blacklevel / depth;
    } else {
        luma_scale   = (lumapeak - blacklevel) / depth;
        chroma_scale = (chromapeak - blacklevel) / depth;
        offset       = blacklevel / depth;
    }

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
                       !qFuzzyCompare(tmpmatrix,   m_primaryMatrix);
    Debug();
    emit Updated(primchanged);
}

void MythVideoColourSpace::Debug(void)
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

int MythVideoColourSpace::ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value)
{
    if (!(m_supportedAttributes & toMask(Attribute)))
        return -1;

    int current = GetPictureAttribute(Attribute);
    if (current < 0)
        return -1;

    int newvalue = Value;
    if (Value < 0)
    {
        newvalue = current + ((Direction) ? +1 : -1);
        if (kPictureAttribute_Hue == Attribute)
            newvalue = newvalue % 100;
        if ((kPictureAttribute_Range == Attribute) && newvalue > 1)
            newvalue = 1;
    }

    newvalue = std::clamp(newvalue, 0, 100);
    if (newvalue != current)
    {
        switch (Attribute)
        {
            case kPictureAttribute_Brightness:
                SetBrightness(newvalue);
                break;
            case kPictureAttribute_Contrast:
                SetContrast(newvalue);
                break;
            case kPictureAttribute_Colour:
                SetSaturation(newvalue);
                break;
            case kPictureAttribute_Hue:
                SetHue(newvalue);
                break;
            default:
                newvalue = -1;
        }

        emit PictureAttributeChanged(Attribute, newvalue);

        if (newvalue >= 0)
            SaveValue(Attribute, newvalue);
    }

    return newvalue;
}

/*! \brief Set the current colourspace to use.
 *
 * We rely on FFmpeg to detect and report the correct colourspace. In the event
 * that no colourspace is found we use sensible defaults for standard and high
 * definition content (BT470BG/BT601 and BT709 respectively).
*/
bool MythVideoColourSpace::UpdateColourSpace(const MythVideoFrame *Frame)
{
    if (!Frame)
        return false;

    int csp      = Frame->m_colorspace;
    int primary  = Frame->m_colorprimaries;
    int transfer = Frame->m_colortransfer;
    int chroma   = Frame->m_chromalocation;
    int raw      = csp;
    int rawchroma = chroma;
    VideoFrameType frametype = Frame->m_type;
    VideoFrameType softwaretype = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_swPixFmt));

    // workaround for NVDEC. NVDEC defaults to a colorspace of 0 - which happens
    // to equate to RGB. In testing, NVDEC reports the same colourspace as FFmpeg
    // software decode for MPEG2, MPEG4, H.264, HEVC and VP8. VP9 seems to go wrong (with limited samples)
    bool forced = false;
    if (csp == AVCOL_SPC_RGB && (MythVideoFrame::YUVFormat(frametype) || frametype == FMT_NVDEC))
    {
        forced = true;
        csp = AVCOL_SPC_UNSPECIFIED;
    }
    int range = Frame->m_colorrange;
    if (range ==  AVCOL_RANGE_UNSPECIFIED)
        range = AVCOL_RANGE_MPEG;
    int depth = MythVideoFrame::ColorDepth(MythVideoFrame::HardwareFormat(frametype) ? softwaretype : frametype);
    if (csp == AVCOL_SPC_UNSPECIFIED)
        csp = (Frame->m_width < 1280) ? AVCOL_SPC_BT470BG : AVCOL_SPC_BT709;
    if (primary == AVCOL_PRI_UNSPECIFIED)
        primary = (Frame->m_width < 1280) ? AVCOL_PRI_BT470BG : AVCOL_PRI_BT709;
    if (transfer == AVCOL_TRC_UNSPECIFIED)
        transfer = (Frame->m_width < 1280) ? AVCOL_TRC_GAMMA28 : AVCOL_TRC_BT709;
    if (chroma == AVCHROMA_LOC_UNSPECIFIED)
        chroma = AVCHROMA_LOC_LEFT;

    if ((csp == m_colourSpace) && (m_colourSpaceDepth == depth) &&
        (m_range == range) && (m_colourShifted == Frame->m_colorshifted) &&
        (primary == m_colourPrimaries) && (chroma == m_chromaLocation))
    {
        return false;
    }

    m_colourSpace      = csp;
    m_colourSpaceDepth = depth;
    m_range            = range;
    m_colourShifted    = Frame->m_colorshifted;
    m_colourPrimaries  = primary;
    m_colourTransfer   = transfer;
    m_chromaLocation   = chroma;

    if (forced)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Forcing inconsistent colourspace - frame format %1")
            .arg(MythVideoFrame::FormatDescription(Frame->m_type)));

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Input : %1(%2) Depth:%3 %4Range:%5")
        .arg(av_color_space_name(static_cast<AVColorSpace>(m_colourSpace)),
             m_colourSpace == raw ? "Detected" : "Guessed",
             QString::number(m_colourSpaceDepth),
             (m_colourSpaceDepth > 8) ? (m_colourShifted ? "(Pre-scaled) " : "(Fixed point) ") : "",
             (AVCOL_RANGE_JPEG == m_range) ? "Full" : "Limited"));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Input : Primaries:%1 Transfer: %2")
        .arg(av_color_primaries_name(static_cast<AVColorPrimaries>(m_colourPrimaries)),
             av_color_transfer_name(static_cast<AVColorTransferCharacteristic>(m_colourTransfer))));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Output: Range:%1 Primaries: %2")
        .arg(m_fullRange ? "Full" : "Limited",
             m_customDisplayPrimaries ? "Custom (screen)" :
                av_color_primaries_name(static_cast<AVColorPrimaries>(m_displayPrimaries))));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Chroma location: %1 %2")
        .arg(av_chroma_location_name(static_cast<AVChromaLocation>(m_chromaLocation)),
             rawchroma == m_chromaLocation ? "(Detected)" : "(Guessed)"));

    Update();

    if (!m_primaryMatrix.isIdentity())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Enabled colourspace primaries conversion from %1 to %2")
            .arg(av_color_primaries_name(static_cast<AVColorPrimaries>(m_colourPrimaries)),
                 m_customDisplayPrimaries
                 ? "Custom (screen)"
                 : av_color_primaries_name(static_cast<AVColorPrimaries>(m_displayPrimaries))));
    }
    return true;
}

void MythVideoColourSpace::SetFullRange(bool FullRange)
{
    m_fullRange = FullRange;
    Update();
}

void MythVideoColourSpace::SetBrightness(int Value)
{
    m_brightness = (Value * 0.02F) - 1.0F;
    Update();
}

void MythVideoColourSpace::SetContrast(int Value)
{
    m_contrast = Value * 0.02F;
    Update();
}

void MythVideoColourSpace::SetHue(int Value)
{
    m_hue = Value * -3.6F;
    Update();
}

void MythVideoColourSpace::SetSaturation(int Value)
{
    m_saturation = Value * 0.02F;
    Update();
}

void MythVideoColourSpace::SetAlpha(int Value)
{
    m_alpha = 100.0F / Value;
    Update();
}

QStringList MythVideoColourSpace::GetColourMappingDefines(void)
{
    QStringList result;

    // TODO extend this to handle all non-co-sited chroma locations. Left is the
    // most common by far - otherwise topleft seems to be used by some 422 content (but
    // subjectively looks better without adjusting vertically!)
    if (m_chromaLocation == AVCHROMA_LOC_LEFT || m_chromaLocation == AVCHROMA_LOC_TOPLEFT ||
        m_chromaLocation == AVCHROMA_LOC_BOTTOMLEFT)
    {
        result << "CHROMALEFT";
    }

    if (!m_primaryMatrix.isIdentity())
        result << "COLOURMAPPING";
    return result;
}

QMatrix4x4 MythVideoColourSpace::GetPrimaryMatrix(void)
{
    return m_primaryMatrix;
}

float MythVideoColourSpace::GetColourGamma(void) const
{
    return m_colourGamma;
}

float MythVideoColourSpace::GetDisplayGamma(void) const
{
    return m_displayGamma;
}

PrimariesMode MythVideoColourSpace::GetPrimariesMode(void)
{
    return m_primariesMode;
}

int MythVideoColourSpace::GetRange() const
{
    return m_range;
}

int MythVideoColourSpace::GetColourSpace() const
{
    return m_colourSpace;
}

void MythVideoColourSpace::SetPrimariesMode(PrimariesMode Mode)
{
    m_primariesMode = Mode;
    Update();
}

/// \brief Save the PictureAttribute value to the database.
void MythVideoColourSpace::SaveValue(PictureAttribute AttributeType, int Value)
{
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

QMatrix4x4 MythVideoColourSpace::GetPrimaryConversion(int Source, int Dest)
{
    // Default to identity
    QMatrix4x4 result;

    // User isn't interested in quality
    if (PrimariesDisabled == m_primariesMode)
        return result;

    auto source = static_cast<AVColorPrimaries>(Source);
    auto dest   = static_cast<AVColorPrimaries>(Dest);
    auto custom = m_customDisplayPrimaries != nullptr;

    // No-op
    if (!custom && (source == dest))
        return result;

    auto srcprimaries = GetPrimaries(source, m_colourGamma);
    auto dstprimaries = GetPrimaries(dest, m_displayGamma);
    if (custom)
    {
        dstprimaries   = *m_customDisplayPrimaries;
        m_displayGamma = m_customDisplayGamma;
    }

    // If 'exact' is not requested and the primaries and gamma are similar, then
    // ignore. Note: 0.021F should cover any differences between Rec.709/sRGB and Rec.610
    if ((m_primariesMode == PrimariesRelaxed) && qFuzzyCompare(m_colourGamma + 1.0F, m_displayGamma + 1.0F) &&
        MythColourSpace::Alike(srcprimaries, dstprimaries, 0.021F))
    {
        return result;
    }

    return (MythColourSpace::RGBtoXYZ(srcprimaries) *
            MythColourSpace::RGBtoXYZ(dstprimaries).inverted());
}

MythColourSpace MythVideoColourSpace::GetPrimaries(int Primary, float &Gamma)
{
    auto primary = static_cast<AVColorPrimaries>(Primary);
    Gamma = 2.2F;
    switch (primary)
    {
        case AVCOL_PRI_BT470BG:
        case AVCOL_PRI_BT470M:    return MythColourSpace::s_BT610_625;
        case AVCOL_PRI_SMPTE170M:
        case AVCOL_PRI_SMPTE240M: return MythColourSpace::s_BT610_525;
        case AVCOL_PRI_BT2020:    Gamma = 2.4F; return MythColourSpace::s_BT2020;
        default: return MythColourSpace::s_BT709;
    }
}
