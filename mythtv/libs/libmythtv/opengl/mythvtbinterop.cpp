// MythTV
#include "mythmainwindow.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythvtbinterop.h"

#define LOC QString("VTBInterop: ")

MythOpenGLInterop::Type MythVTBInterop::GetInteropType(VideoFrameType Format)
{
    if ((FMT_VTB != Format) || !gCoreContext->IsUIThread())
        return Unsupported;

    MythRenderOpenGL* context = MythRenderOpenGL::GetOpenGLRender();
    if (!context)
        return Unsupported;

    if (context->isOpenGLES() || context->IsEGL())
        return Unsupported;

    if (context->hasExtension("GL_ARB_texture_rg"))
        return VTBSURFACE;
    return VTBOPENGL;
}

MythVTBInterop* MythVTBInterop::Create(MythRenderOpenGL *Context, MythOpenGLInterop::Type Type)
{
    if (Context)
    {
        if (Type == VTBSURFACE)
            return new MythVTBSurfaceInterop(Context);
        else if (Type == VTBOPENGL)
            return new MythVTBInterop(Context, VTBOPENGL);
    }
    return nullptr;
}

MythVTBInterop::MythVTBInterop(MythRenderOpenGL *Context, MythOpenGLInterop::Type Type)
  : MythOpenGLInterop(Context, Type)
{
}

MythVTBInterop::~MythVTBInterop()
{
}

vector<MythVideoTexture*> MythVTBInterop::Acquire(MythRenderOpenGL *Context,
                                                  VideoColourSpace *ColourSpace,
                                                  VideoFrame *Frame,
                                                  FrameScanType)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    if (Context && (Context != m_context))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Lock
    OpenGLLocker locker(m_context);

    // There are only ever one set of textures which are updated on each pass.
    // Hence we cannot use reference frames without significant additional work here -
    // but MythVTBSurfaceInterop will almost certainly always be used - so just drop back
    // to Linearblend
    Frame->deinterlace_allowed = (Frame->deinterlace_allowed & ~DEINT_HIGH) | DEINT_MEDIUM;

    // Update colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve pixel buffer
    // N.B. Buffer was retained in MythVTBContext and will be released in VideoBuffers
    CVPixelBufferRef buffer = reinterpret_cast<CVPixelBufferRef>(Frame->buf);
    if (!buffer)
        return result;

    int planes = CVPixelBufferGetPlaneCount(buffer);
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

    // FIXME? There is no P010 support here as I cannot test it - but SurfaceInterop
    // should be used at all times - so this code is not going to be hit.
    // Note - and the reason this code is retained is because it may at some point
    // be useful for an iOS port (iOS has no surace interop)

    // Create necessary textures
    if (m_openglTextures.isEmpty())
    {
        for (int plane = 0; plane < planes; ++plane)
        {
            int width  = CVPixelBufferGetWidthOfPlane(buffer, plane);
            int height = CVPixelBufferGetHeightOfPlane(buffer, plane);
            QSize size(width, height);
            MythVideoTexture *texture = MythVideoTexture::CreateTexture(m_context, size);
            if (texture)
            {
                texture->m_frameType = FMT_VTB;
                texture->m_frameFormat = FMT_NV12;
                texture->m_plane = plane;
                texture->m_planeCount = planes;
                texture->m_allowGLSLDeint = true;
                result.push_back(texture);
            }
        }

        if (result.size() != static_cast<uint>(planes))
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all textures");
        m_openglTextures.insert(DUMMY_INTEROP_ID, result);
    }

    if (m_openglTextures.contains(DUMMY_INTEROP_ID))
    {
        result = m_openglTextures[DUMMY_INTEROP_ID];
        m_context->glEnable(QOpenGLTexture::Target2D);
        
        // Update textures
        for (int plane = 0; plane < planes; ++plane)
        {
            if (static_cast<uint>(plane) >= result.size())
                continue;
        
            int width  = CVPixelBufferGetWidthOfPlane(buffer, plane);
            int height = CVPixelBufferGetHeightOfPlane(buffer, plane);
            int bytes  = CVPixelBufferGetBytesPerRowOfPlane(buffer, plane) / width;
            void* buf  = CVPixelBufferGetBaseAddressOfPlane(buffer, plane);
            result[plane]->m_texture->bind();
            if ((bytes == 1) && buf)
            {
                m_context->glTexImage2D(QOpenGLTexture::Target2D, 0, QOpenGLTexture::RGBA, width, height,
                                        0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, buf);
            }
            else if ((bytes == 2) && buf)
            {
                m_context->glTexImage2D(QOpenGLTexture::Target2D, 0, QOpenGLTexture::RGBA, width, height,
                                        0, QOpenGLTexture::RG, QOpenGLTexture::UInt8, buf);
            }
            result[plane]->m_texture->release();
        }
    }
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    return result;
}

MythVTBSurfaceInterop::MythVTBSurfaceInterop(MythRenderOpenGL *Context)
  : MythVTBInterop(Context, VTBSURFACE)
{
}

MythVTBSurfaceInterop::~MythVTBSurfaceInterop()
{
}

vector<MythVideoTexture*> MythVTBSurfaceInterop::Acquire(MythRenderOpenGL *Context,
                                                         VideoColourSpace *ColourSpace,
                                                         VideoFrame *Frame,
                                                         FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    if (Context && (Context != m_context))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Lock
    OpenGLLocker locker(m_context);

    // Update colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve pixel buffer
    // N.B. Buffer was retained in MythVTBContext and will be released in VideoBuffers
    CVPixelBufferRef buffer = reinterpret_cast<CVPixelBufferRef>(Frame->buf);
    if (!buffer)
        return result;

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(buffer);
    if (!surface)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve IOSurface from CV buffer");
        return result;
    }

    IOSurfaceID surfaceid = IOSurfaceGetID(surface);
    if (!surfaceid)
        return result;

    // check whether we need to return multiple frames for GLSL kernel deinterlacing
    bool needreferences = false;
    if (is_interlaced(Scan))
    {
        MythDeintType shader = GetDoubleRateOption(Frame, DEINT_SHADER);
        if (shader)
            needreferences = shader == DEINT_HIGH;
        else
            needreferences = GetSingleRateOption(Frame, DEINT_SHADER) == DEINT_HIGH;
    }

    if (needreferences)
    {
        if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
            m_referenceFrames.clear();
        RotateReferenceFrames(surfaceid);
    }
    else
    {
        m_referenceFrames.clear();
    }
    m_discontinuityCounter = Frame->frameCounter;

    // return cached textures if available
    if (m_openglTextures.contains(surfaceid))
    {
        if (needreferences)
            return GetReferenceFrames();
        return m_openglTextures[surfaceid];
    }

    // Create new and attach to IOSurface
    int planes = IOSurfaceGetPlaneCount(surface);
    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, nullptr);

    // Create raw rectangular textures
    vector<QSize> sizes;
    for (int plane = 0; plane < planes; ++plane)
    {
        int width  = IOSurfaceGetWidthOfPlane(surface, plane);
        int height = IOSurfaceGetHeightOfPlane(surface, plane);
        sizes.push_back(QSize(width, height));
    }
    // NB P010 support is untested
    // NB P010 support was added to FFmpeg in https://github.com/FFmpeg/FFmpeg/commit/036b4b0f85933f49a709
    // which has not yet been merged into the MythTV FFmpeg version (as of 22/6/19)
    VideoFrameType frameformat = PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));
    if ((frameformat != FMT_NV12) && (frameformat != FMT_P010))
    {
        IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported frame format %1")
            .arg(format_description(frameformat)));
        return result;
    }

    result = MythVideoTexture::CreateTextures(m_context, FMT_VTB, frameformat, sizes, QOpenGLTexture::TargetRectangle);

    for (uint plane = 0; plane < result.size(); ++plane)
    {
       MythVideoTexture* texture = result[plane];
        if (!texture)
            continue;
        texture->m_allowGLSLDeint = true;
        m_context->glBindTexture(texture->m_target, texture->m_textureId);

        GLenum format = (plane == 0) ? QOpenGLTexture::Red : QOpenGLTexture::RG;;
        GLenum dataformat = (frameformat == FMT_NV12) ? QOpenGLTexture::UInt8 : QOpenGLTexture::UInt16;

        CGLError error = CGLTexImageIOSurface2D(
                CGLGetCurrentContext(), QOpenGLTexture::TargetRectangle, format,
                texture->m_size.width(), texture->m_size.height(),
                format, dataformat, surface, plane);
        if (error != kCGLNoError)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("CGLTexImageIOSurface2D error %1").arg(error));
        m_context->glBindTexture(QOpenGLTexture::TargetRectangle, 0);
    }

    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
    m_openglTextures.insert(surfaceid, result);

    if (needreferences)
        return GetReferenceFrames();
    return result;
}

void MythVTBSurfaceInterop::RotateReferenceFrames(IOSurfaceID Buffer)
{
    if (!Buffer)
        return;

    // don't retain twice for double rate
    if ((m_referenceFrames.size() > 0) && (m_referenceFrames[0] == Buffer))
        return;

    m_referenceFrames.push_front(Buffer);

    // release old frames
    while (m_referenceFrames.size() > 3)
        m_referenceFrames.pop_back();
}

vector<MythVideoTexture*> MythVTBSurfaceInterop::GetReferenceFrames(void)
{
    vector<MythVideoTexture*> result;
    int size = m_referenceFrames.size();
    if (size < 1)
        return result;

    IOSurfaceID next = m_referenceFrames[0];
    IOSurfaceID current = m_referenceFrames[size > 1 ? 1 : 0];
    IOSurfaceID last = m_referenceFrames[size > 2 ? 2 : 0];

    if (!m_openglTextures.contains(next) || !m_openglTextures.contains(current) ||
        !m_openglTextures.contains(last))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Reference frame error");
        return result;
    }

    result = m_openglTextures[last];
    foreach (MythVideoTexture* tex, m_openglTextures[current])
        result.push_back(tex);
    foreach (MythVideoTexture* tex, m_openglTextures[next])
        result.push_back(tex);
    return result;
}
