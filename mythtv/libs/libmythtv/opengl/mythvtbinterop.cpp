// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythui/mythmainwindow.h"
#include "mythvideocolourspace.h"
#include "opengl/mythvtbinterop.h"

#define LOC QString("VTBInterop: ")

void MythVTBInterop::GetVTBTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types)
{
    if (Render->isOpenGLES() || Render->IsEGL())
        return;
    MythInteropGPU::InteropTypes types;
    if (Render->hasExtension("GL_ARB_texture_rg"))
        types.emplace_back(GL_VTBSURFACE);
    types.emplace_back(GL_VTB);
    Types[FMT_VTB] = types;
}

MythVTBInterop* MythVTBInterop::CreateVTB(MythPlayerUI* Player, MythRenderOpenGL* Context)
{
    if (!(Context && Player))
        return nullptr;

    MythInteropGPU::InteropMap types;
    GetVTBTypes(Context, types);
    if (auto vtb = types.find(FMT_VTB); vtb != types.end())
    {
        for (auto type : vtb->second)
        {
            if (type == GL_VTBSURFACE)
                return new MythVTBSurfaceInterop(Player, Context);
            else if (type == GL_VTB)
                return new MythVTBInterop(Player, Context, GL_VTB);
        }
    }
    return nullptr;
}

MythVTBInterop::MythVTBInterop(MythPlayerUI* Player, MythRenderOpenGL* Context,
                               MythOpenGLInterop::InteropType Type)
  : MythOpenGLInterop(Context, Type, Player)
{
}

MythVTBInterop::~MythVTBInterop()
{
}

CVPixelBufferRef MythVTBInterop::Verify(MythRenderOpenGL* Context, MythVideoColourSpace* ColourSpace, MythVideoFrame* Frame)
{
    if (!Frame)
        return nullptr;

    if (Context && (Context != m_openglContext))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Check size
    QSize surfacesize(Frame->m_width, Frame->m_height);
    if (m_textureSize != surfacesize)
    {
        if (!m_textureSize.isEmpty())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Video texture size changed! %1x%2->%3x%4")
                .arg(m_textureSize.width()).arg(m_textureSize.height())
                .arg(Frame->m_width).arg(Frame->m_height));
        }
        DeleteTextures();
        m_textureSize = surfacesize;
    }

    // Update colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve pixel buffer
    // N.B. Buffer was retained in MythVTBContext and will be released in VideoBuffers
    return reinterpret_cast<CVPixelBufferRef>(Frame->m_buffer);
}

std::vector<MythVideoTextureOpenGL*>
MythVTBInterop::Acquire(MythRenderOpenGL* Context,
                        MythVideoColourSpace* ColourSpace,
                        MythVideoFrame* Frame,
                        FrameScanType)
{
    std::vector<MythVideoTextureOpenGL*> result;
    OpenGLLocker locker(m_openglContext);

    CVPixelBufferRef buffer = Verify(Context, ColourSpace, Frame);
    if (!buffer)
        return result;

    // There are only ever one set of textures which are updated on each pass.
    // Hence we cannot use reference frames without significant additional work here -
    // but MythVTBSurfaceInterop will almost certainly always be used - so just drop back
    // to Linearblend
    Frame->m_deinterlaceAllowed = (Frame->m_deinterlaceAllowed & ~DEINT_HIGH) | DEINT_MEDIUM;

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
            MythVideoTextureOpenGL* texture = MythVideoTextureOpenGL::CreateTexture(m_openglContext, size);
            if (texture)
            {
                texture->m_frameType = FMT_VTB;
                texture->m_frameFormat = FMT_NV12;
                texture->m_plane = static_cast<uint>(plane);
                texture->m_planeCount = static_cast<uint>(planes);
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
        m_openglContext->glEnable(QOpenGLTexture::Target2D);
        
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
                m_openglContext->glTexImage2D(QOpenGLTexture::Target2D, 0, QOpenGLTexture::RGBA, width, height,
                                        0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, buf);
            }
            else if ((bytes == 2) && buf)
            {
                m_openglContext->glTexImage2D(QOpenGLTexture::Target2D, 0, QOpenGLTexture::RGBA, width, height,
                                        0, QOpenGLTexture::RG, QOpenGLTexture::UInt8, buf);
            }
            result[plane]->m_texture->release();
        }
    }
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    return result;
}

MythVTBSurfaceInterop::MythVTBSurfaceInterop(MythPlayerUI* Player, MythRenderOpenGL* Context)
  : MythVTBInterop(Player, Context, GL_VTBSURFACE)
{
}

MythVTBSurfaceInterop::~MythVTBSurfaceInterop()
{
}

std::vector<MythVideoTextureOpenGL*>
MythVTBSurfaceInterop::Acquire(MythRenderOpenGL* Context,
                               MythVideoColourSpace* ColourSpace,
                               MythVideoFrame* Frame,
                               FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
    OpenGLLocker locker(m_openglContext);

    CVPixelBufferRef buffer = Verify(Context, ColourSpace, Frame);
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
        MythDeintType shader = Frame->GetDoubleRateOption(DEINT_SHADER);
        if (shader)
            needreferences = shader == DEINT_HIGH;
        else
            needreferences = Frame->GetSingleRateOption(DEINT_SHADER) == DEINT_HIGH;
    }

    if (needreferences)
    {
        if (qAbs(Frame->m_frameCounter - m_discontinuityCounter) > 1)
            m_referenceFrames.clear();
        RotateReferenceFrames(surfaceid);
    }
    else
    {
        m_referenceFrames.clear();
    }
    m_discontinuityCounter = Frame->m_frameCounter;

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
    std::vector<QSize> sizes;
    for (int plane = 0; plane < planes; ++plane)
    {
        int width  = IOSurfaceGetWidthOfPlane(surface, plane);
        int height = IOSurfaceGetHeightOfPlane(surface, plane);
        sizes.push_back(QSize(width, height));
    }
    // NB P010 support is untested
    // NB P010 support was added to FFmpeg in https://github.com/FFmpeg/FFmpeg/commit/036b4b0f85933f49a709
    // which has not yet been merged into the MythTV FFmpeg version (as of 22/6/19)
    VideoFrameType frameformat = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_swPixFmt));
    if ((frameformat != FMT_NV12) && (frameformat != FMT_P010))
    {
        IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported frame format %1")
            .arg(MythVideoFrame::FormatDescription(frameformat)));
        return result;
    }

    result = MythVideoTextureOpenGL::CreateTextures(m_openglContext, FMT_VTB, frameformat, sizes, QOpenGLTexture::TargetRectangle);

    for (uint plane = 0; plane < result.size(); ++plane)
    {
       MythVideoTextureOpenGL* texture = result[plane];
        if (!texture)
            continue;
        texture->m_allowGLSLDeint = true;
        m_openglContext->glBindTexture(texture->m_target, texture->m_textureId);

        GLenum format = (plane == 0) ? QOpenGLTexture::Red : QOpenGLTexture::RG;;
        GLenum dataformat = (frameformat == FMT_NV12) ? QOpenGLTexture::UInt8 : QOpenGLTexture::UInt16;

        CGLError error = CGLTexImageIOSurface2D(
                CGLGetCurrentContext(), QOpenGLTexture::TargetRectangle, format,
                texture->m_size.width(), texture->m_size.height(),
                format, dataformat, surface, plane);
        if (error != kCGLNoError)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("CGLTexImageIOSurface2D error %1").arg(error));
        m_openglContext->glBindTexture(QOpenGLTexture::TargetRectangle, 0);
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

std::vector<MythVideoTextureOpenGL*> MythVTBSurfaceInterop::GetReferenceFrames(void)
{
    std::vector<MythVideoTextureOpenGL*> result;
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
    std::copy(m_openglTextures[current].cbegin(), m_openglTextures[current].cend(), std::back_inserter(result));
    std::copy(m_openglTextures[next].cbegin(), m_openglTextures[next].cend(), std::back_inserter(result));
    return result;
}
