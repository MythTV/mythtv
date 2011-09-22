#include "openglvideo.h"
#include "mythlogging.h"
#include "mythxdisplay.h"
#include "mythcodecid.h"
#include "frame.h"
#include "vaapicontext.h"
#include "mythmainwindow.h"

#define LOC QString("VAAPI: ")
#define ERR QString("VAAPI Error: ")
#define NUM_VAAPI_BUFFERS 24

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

#define CREATE_CHECK(arg1, arg2) \
  if (ok) \
  { \
      ok = arg1; \
      if (!ok) \
          LOG(VB_GENERAL, LOG_ERR, LOC + arg2); \
  } while(0)

QString profileToString(VAProfile profile);
QString entryToString(VAEntrypoint entry);
VAProfile preferredProfile(MythCodecID codec);

QString profileToString(VAProfile profile)
{
    if (VAProfileMPEG2Simple == profile)         return "MPEG2Simple";
    if (VAProfileMPEG2Main == profile)           return "MPEG2Main";
    if (VAProfileMPEG4Simple == profile)         return "MPEG4Simple";
    if (VAProfileMPEG4AdvancedSimple == profile) return "MPEG4AdvSimple";
    if (VAProfileMPEG4Main == profile)           return "MPEG4Main";
    if (VAProfileH264Baseline == profile)        return "H264Base";
    if (VAProfileH264Main == profile)            return "H264Main";
    if (VAProfileH264High == profile)            return "H264High";
    if (VAProfileVC1Simple == profile)           return "VC1Simple";
    if (VAProfileVC1Main == profile)             return "VC1Main";
    if (VAProfileVC1Advanced == profile)         return "VC1Advanced";
    if (VAProfileH263Baseline == profile)        return "H263Base";
    return "Unknown";
}

QString entryToString(VAEntrypoint entry)
{
    if (VAEntrypointVLD == entry)        return "VLD ";
    if (VAEntrypointIZZ == entry)        return "IZZ (UNSUPPORTED) ";
    if (VAEntrypointIDCT == entry)       return "IDCT (UNSUPPORTED) ";
    if (VAEntrypointMoComp == entry)     return "MC (UNSUPPORTED) ";
    if (VAEntrypointDeblocking == entry) return "Deblock (UNSUPPORTED) ";
    if (VAEntrypointEncSlice == entry)   return "EncSlice (UNSUPPORTED) ";
    return "Unknown";
}

VAProfile preferredProfile(MythCodecID codec)
{
    // FIXME handle unsupported codecs properly
    if (kCodec_H263_VAAPI  == codec) return VAProfileMPEG4AdvancedSimple;
    if (kCodec_MPEG4_VAAPI == codec) return VAProfileMPEG4AdvancedSimple;
    if (kCodec_H264_VAAPI  == codec) return VAProfileH264High;
    if (kCodec_VC1_VAAPI   == codec) return VAProfileVC1Advanced;
    if (kCodec_WMV3_VAAPI  == codec) return VAProfileVC1Main;
    return VAProfileMPEG2Main;
}

class VAAPIDisplay
{
  protected:
    VAAPIDisplay() : m_va_disp(NULL), m_x_disp(NULL), m_display(NULL),
                     m_ref_count(0) { }
  public:
   ~VAAPIDisplay()
    {
        if (m_va_disp)
        {
            INIT_ST;
            XLOCK(m_x_disp, va_status = vaTerminate(m_va_disp));
            CHECK_ST;
        }
        if (m_x_disp)
        {
            m_x_disp->Sync(true);
            delete m_x_disp;
        }
    }

    bool Create(void)
    {
        MythMainWindow *mw = GetMythMainWindow();
        if (!mw)
            return false;

        MythRenderOpenGL *gl =
            static_cast<MythRenderOpenGL*>(mw->GetRenderDevice());

        if (!gl)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("Failed to get OpenGL context - you must use the "
                        "OpenGL UI painter for VAAPI support."));
            return false;
        }
        gl->makeCurrent();
        m_display = glXGetCurrentDisplay();
        gl->doneCurrent();

        m_x_disp = OpenMythXDisplay();
        if (!m_x_disp)
            return false;

        MythXLocker locker(m_x_disp);
        int major_ver, minor_ver;

        //m_va_disp = vaGetDisplayGLX(m_x_disp->GetDisplay());
        m_va_disp = vaGetDisplayGLX(m_display);

        if (!m_va_disp)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VADisplay");
            return false;
        }

        INIT_ST;
        va_status = vaInitialize(m_va_disp, &major_ver, &minor_ver);
        CHECK_ST;

        static bool debugged = false;
        if (ok && !debugged)
        {
            debugged = true;
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Version: %1.%2")
                                        .arg(major_ver).arg(minor_ver));
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Vendor : %1")
                                        .arg(vaQueryVendorString(m_va_disp)));
        }
        if (ok)
        {
            UpRef();
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created VAAPI GLX display");
        }
        return ok;
    }

    void UpRef(void)
    {
        XLOCK(m_x_disp, m_ref_count++)
    }

    void DownRef(void)
    {
        m_x_disp->Lock();
        m_ref_count--;
        if (m_ref_count <= 0)
        {
            if (gVAAPIDisplay == this)
                gVAAPIDisplay = NULL;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting VAAPI display.");
            m_x_disp->Unlock();
            delete this;
            return;
        }
        m_x_disp->Unlock();
    }

    static VAAPIDisplay* GetDisplay(void)
    {
        if (gVAAPIDisplay)
        {
            gVAAPIDisplay->UpRef();
            return gVAAPIDisplay;
        }

        gVAAPIDisplay = new VAAPIDisplay();
        if (gVAAPIDisplay && gVAAPIDisplay->Create())
            return gVAAPIDisplay;

        delete gVAAPIDisplay;
        gVAAPIDisplay = NULL;
        return NULL;
    }

    static VAAPIDisplay *gVAAPIDisplay;
    void                *m_va_disp;
    MythXDisplay        *m_x_disp;
    Display             *m_display;
    int                  m_ref_count;
};

VAAPIDisplay* VAAPIDisplay::gVAAPIDisplay = NULL;

bool VAAPIContext::IsFormatAccelerated(QSize size, MythCodecID codec,
                                       PixelFormat &pix_fmt)
{
    bool result = false;
    VAAPIContext *ctx = new VAAPIContext(codec);
    if (ctx && ctx->CreateDisplay(size))
    {
        pix_fmt = ctx->GetPixelFormat();
        result = pix_fmt == PIX_FMT_VAAPI_VLD;
    }
    delete ctx;
    return result;
}

VAAPIContext::VAAPIContext(MythCodecID codec)
  : m_codec(codec),
    m_display(NULL),
    m_vaProfile(VAProfileMPEG2Main)/* ?? */,
    m_vaEntrypoint(VAEntrypointEncSlice),
    m_pix_fmt(PIX_FMT_YUV420P), m_numSurfaces(NUM_VAAPI_BUFFERS),
    m_surfaces(NULL), m_surfaceData(NULL), m_pictureAttributes(NULL),
    m_pictureAttributeCount(0)
{
    memset(&m_ctx, 0, sizeof(vaapi_context));
}

VAAPIContext::~VAAPIContext()
{
    delete [] m_pictureAttributes;

    ClearGLXSurfaces();

    if (m_display)
    {
        m_display->m_x_disp->Lock();

        INIT_ST;
        if (m_ctx.context_id)
        {
            va_status = vaDestroyContext(m_ctx.display, m_ctx.context_id);
            CHECK_ST;
        }
        if (m_ctx.config_id)
        {
            va_status = vaDestroyConfig(m_ctx.display, m_ctx.config_id);
            CHECK_ST;
        }
        if (m_surfaces)
        {
            va_status = vaDestroySurfaces(m_ctx.display, m_surfaces, m_numSurfaces);
            CHECK_ST;
        }
    }

    if (m_surfaces)
        delete [] m_surfaces;
    if (m_surfaceData)
        delete [] m_surfaceData;

    if (m_display)
    {
        m_display->m_x_disp->Unlock();
        m_display->DownRef();
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleted context");
}

bool VAAPIContext::CreateDisplay(QSize size)
{
    m_size = size;
    bool ok = true;
    m_display = VAAPIDisplay::GetDisplay();
    CREATE_CHECK(!m_size.isEmpty(), "Invalid size");
    CREATE_CHECK(m_display != NULL, "Invalid display");
    CREATE_CHECK(InitDisplay(),     "Invalid VADisplay");
    CREATE_CHECK(InitProfiles(),    "No supported profiles");
    if (ok)
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created context (%1x%2->%3x%4)")
            .arg(size.width()).arg(size.height())
            .arg(m_size.width()).arg(m_size.height()));
    return ok;
}

void VAAPIContext::InitPictureAttributes(VideoColourSpace &colourspace)
{
    if (!m_display)
        return;
    if (!m_display->m_va_disp)
        return;

    delete [] m_pictureAttributes;
    m_pictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QList<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(m_display->m_va_disp);
    VADisplayAttribute* attribs = new VADisplayAttribute[num];

    int actual = 0;
    INIT_ST;
    va_status = vaQueryDisplayAttributes(m_display->m_va_disp, attribs, &actual);
    CHECK_ST;

    for (int i = 0; i < actual; i++)
    {
        int type = attribs[i].type;
        if ((attribs[i].flags & VA_DISPLAY_ATTRIB_SETTABLE) &&
            (type == VADisplayAttribBrightness ||
             type == VADisplayAttribContrast ||
             type == VADisplayAttribHue ||
             type == VADisplayAttribSaturation))
        {
            supported.push_back(attribs[i]);
            if (type == VADisplayAttribBrightness)
                supported_controls += kPictureAttributeSupported_Brightness;
            if (type == VADisplayAttribHue)
                supported_controls += kPictureAttributeSupported_Hue;
            if (type == VADisplayAttribContrast)
                supported_controls += kPictureAttributeSupported_Contrast;
            if (type == VADisplayAttribSaturation)
                supported_controls += kPictureAttributeSupported_Colour;
        }
    }

    colourspace.SetSupportedAttributes((PictureAttributeSupported)supported_controls);
    delete [] attribs;

    if (!supported.size())
        return;

    m_pictureAttributeCount = supported.size();
    m_pictureAttributes = new VADisplayAttribute[m_pictureAttributeCount];
    for (int i = 0; i < m_pictureAttributeCount; i++)
        m_pictureAttributes[i] = supported.at(i);

    if (supported_controls & kPictureAttributeSupported_Brightness)
        SetPictureAttribute(kPictureAttribute_Brightness,
            colourspace.GetPictureAttribute(kPictureAttribute_Brightness));
    if (supported_controls & kPictureAttributeSupported_Hue)
        SetPictureAttribute(kPictureAttribute_Hue,
            colourspace.GetPictureAttribute(kPictureAttribute_Hue));
    if (supported_controls & kPictureAttributeSupported_Contrast)
        SetPictureAttribute(kPictureAttribute_Contrast,
            colourspace.GetPictureAttribute(kPictureAttribute_Contrast));
    if (supported_controls & kPictureAttributeSupported_Colour)
        SetPictureAttribute(kPictureAttribute_Colour,
            colourspace.GetPictureAttribute(kPictureAttribute_Colour));
}

int VAAPIContext::SetPictureAttribute(PictureAttribute attribute, int newValue)
{
    if (!m_display)
        return newValue;
    if (!m_display->m_va_disp)
        return newValue;

    VADisplayAttribType attrib = VADisplayAttribBrightness;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            attrib = VADisplayAttribBrightness;
            break;
        case kPictureAttribute_Contrast:
            attrib = VADisplayAttribContrast;
            break;
        case kPictureAttribute_Hue:
            attrib = VADisplayAttribHue;
            break;
        case kPictureAttribute_Colour:
            attrib = VADisplayAttribSaturation;
            break;
        default:
            return -1;
    }

    bool found = false;
    for (int i = 0; i < m_pictureAttributeCount; i++)
    {
        if (m_pictureAttributes[i].type == attrib)
        {
            int min = m_pictureAttributes[i].min_value;
            int max = m_pictureAttributes[i].max_value;
            int val = min + (int)(((float)newValue / 100.0) * (max - min));
            m_pictureAttributes[i].value = val;
            found = true;
            break;
        }
    }

    if (found)
    {
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_display->m_va_disp,
                                           m_pictureAttributes,
                                           m_pictureAttributeCount);
        CHECK_ST;
        return newValue;
    }
    return -1;
}

bool VAAPIContext::CreateBuffers(void)
{
    bool ok = true;
    CREATE_CHECK(!m_size.isEmpty(), "Invalid size");
    CREATE_CHECK(InitBuffers(),     "Failed to create buffers.");
    CREATE_CHECK(InitContext(),     "Failed to create context");
    if (ok)
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created %1 buffers").arg(m_numSurfaces));
    return ok;
}

bool VAAPIContext::InitDisplay(void)
{
    if (!m_display)
        return false;
    m_ctx.display = m_display->m_va_disp;
    return m_ctx.display;
}

bool VAAPIContext::InitProfiles(void)
{
    if (!(codec_is_vaapi(m_codec)) || !m_ctx.display)
        return false;

    MythXLocker locker(m_display->m_x_disp);
    int max_profiles, max_entrypoints;
    VAProfile profile_wanted = preferredProfile(m_codec);
    VAProfile profile_found  = VAProfileMPEG2Main;   // FIXME
    VAEntrypoint entry_found = VAEntrypointEncSlice; // unsupported value

    max_profiles          = vaMaxNumProfiles(m_ctx.display);
    max_entrypoints       = vaMaxNumEntrypoints(m_ctx.display);
    VAProfile *profiles   = new VAProfile[max_profiles];
    VAEntrypoint *entries = new VAEntrypoint[max_entrypoints];

    static bool debugged = false;
    if (profiles && entries)
    {
        INIT_ST;
        int act_profiles, act_entries;
        va_status = vaQueryConfigProfiles(m_ctx.display,
                                          profiles,
                                         &act_profiles);
        CHECK_ST;
        if (ok && act_profiles > 0)
        {
            for (int i = 0; i < act_profiles; i++)
            {
                va_status = vaQueryConfigEntrypoints(m_ctx.display,
                                                     profiles[i],
                                                     entries,
                                                    &act_entries);
                if (va_status == VA_STATUS_SUCCESS && act_entries > 0)
                {
                    if (profiles[i] == profile_wanted)
                    {
                        profile_found = profile_wanted;
                        for (int j = 0; j < act_entries; j++)
                            if (entries[j] < entry_found)
                                entry_found = entries[j];
                    }

                    if (!debugged)
                    {
                        QString entrylist = "Entrypoints: ";
                        for (int j = 0; j < act_entries; j++)
                            entrylist += entryToString(entries[j]);
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            QString("Profile: %1 %2")
                                .arg(profileToString(profiles[i]))
                                .arg(entrylist));
                    }
                }
            }
        }
        debugged = true;
    }
    delete [] profiles;
    delete [] entries;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Desired profile for '%1': %2")
        .arg(toString(m_codec)).arg(profileToString(profile_wanted)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found profile %1 with entry %2")
        .arg(profileToString(profile_found)).arg(entryToString(entry_found)));

    if (profile_wanted != profile_found)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find supported profile.");
        return false;
    }

    if (entry_found > VAEntrypointVLD)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find suitable entry point.");
        return false;
    }

    m_vaProfile = profile_wanted;
    m_vaEntrypoint = entry_found;
    if (VAEntrypointVLD == m_vaEntrypoint)
        m_pix_fmt = PIX_FMT_VAAPI_VLD;
    return true;
}

bool VAAPIContext::InitBuffers(void)
{
    if (!m_ctx.display)
        return false;

    MythXLocker locker(m_display->m_x_disp);
    m_surfaces    = new VASurfaceID[m_numSurfaces];
    m_surfaceData = new vaapi_surface[m_numSurfaces];

    if (!m_surfaces || !m_surfaceData)
        return false;

    memset(m_surfaces, 0, sizeof(m_surfaces));
    memset(m_surfaceData, 0, sizeof(m_surfaceData));

    INIT_ST;
    va_status = vaCreateSurfaces(m_ctx.display, m_size.width(), m_size.height(),
                                 VA_RT_FORMAT_YUV420, m_numSurfaces,
                                 m_surfaces);
    CHECK_ST;

    for (int i = 0; i < m_numSurfaces; i++)
        m_surfaceData[i].m_id = m_surfaces[i];
    return ok;
}

bool VAAPIContext::InitContext(void)
{
    if (!m_ctx.display || m_vaEntrypoint > VAEntrypointVLD)
        return false;

    MythXLocker locker(m_display->m_x_disp);
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    INIT_ST;
    va_status = vaGetConfigAttributes(m_ctx.display, m_vaProfile,
                                      m_vaEntrypoint, &attrib, 1);
    CHECK_ST;

    if (!ok || !(attrib.value & VA_RT_FORMAT_YUV420))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to confirm YUV420 chroma");
        return false;
    }

    va_status = vaCreateConfig(m_ctx.display, m_vaProfile, m_vaEntrypoint,
                               &attrib, 1, &m_ctx.config_id);
    CHECK_ST;
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create decoder config.");
        return false;
    }

    va_status = vaCreateContext(m_ctx.display, m_ctx.config_id,
                                m_size.width(), m_size.height(), VA_PROGRESSIVE,
                                m_surfaces, m_numSurfaces,
                                &m_ctx.context_id);
    CHECK_ST;
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create decoder context.");
        return false;
    }
    return true;
}

void* VAAPIContext::GetVideoSurface(int i)
{
    if (i < 0 || i >= m_numSurfaces)
        return NULL;
    return &m_surfaceData[i];
}

uint8_t* VAAPIContext::GetSurfaceIDPointer(void* buf)
{
    if (!buf)
        return NULL;

    const vaapi_surface *surf = (vaapi_surface*)buf;
    if (!surf->m_id)
        return NULL;

    INIT_ST;
    va_status = vaSyncSurface(m_ctx.display, surf->m_id);
    CHECK_ST;
    return (uint8_t*)(uintptr_t)surf->m_id;
}

bool VAAPIContext::CopySurfaceToTexture(const void* buf, uint texture,
                                        uint texture_type, FrameScanType scan)
{
    if (!buf)
        return false;

    const vaapi_surface *surf = (vaapi_surface*)buf;
    void* glx_surface = GetGLXSurface(texture, texture_type);
    if (!glx_surface)
        return false;

    int field = VA_FRAME_PICTURE;
    //if (scan == kScan_Interlaced)
    //    field = VA_TOP_FIELD;
    //else if (scan == kScan_Intr2ndField)
    //    field = VA_BOTTOM_FIELD;

    //INIT_ST;
    //va_status = vaSyncSurface(m_ctx.display, surf->m_id);
    //CHECK_ST;

    INIT_ST;
    va_status = vaCopySurfaceGLX(m_ctx.display, glx_surface, surf->m_id, field);
    CHECK_ST;
    return true;
}

void* VAAPIContext::GetGLXSurface(uint texture, uint texture_type)
{
    if (m_glxSurfaces.contains(texture))
        return m_glxSurfaces.value(texture);

    void *glx_surface = NULL;
    INIT_ST;
    va_status = vaCreateSurfaceGLX(m_ctx.display, texture_type,
                                   texture, &glx_surface);
    CHECK_ST;
    if (!glx_surface)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLX surface.");
        return NULL;
    }

    m_glxSurfaces.insert(texture, glx_surface);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Number of VAAPI GLX surfaces: %1")
        .arg(m_glxSurfaces.size()));
    return glx_surface;
}

void VAAPIContext::ClearGLXSurfaces(void)
{
    if (!m_display)
        return;

    m_display->m_x_disp->Lock();
    INIT_ST;
    foreach (void* surface, m_glxSurfaces)
    {
        va_status = vaDestroySurfaceGLX(m_ctx.display, surface);
        CHECK_ST;
    }
    m_glxSurfaces.clear();
    m_display->m_x_disp->Unlock();
}
