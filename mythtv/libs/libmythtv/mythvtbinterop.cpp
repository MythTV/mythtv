// MythTV
#include "mythmainwindow.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythvtbinterop.h"

#define LOC QString("VTBInterop: ")

MythOpenGLInterop::Type MythVTBInterop::GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context)
{
    if (!codec_is_vtb(CodecId) || !gCoreContext->IsUIThread())
        return Unsupported;

    if (!Context)
    {
        MythMainWindow* win = MythMainWindow::getMainWindow();
        if (!win)
            return Unsupported;
        Context = static_cast<MythRenderOpenGL*>(win->GetRenderDevice());
    }
    if (!Context)
        return Unsupported;

    if (Context->isOpenGLES() || Context->IsEGL())
        return Unsupported;

    if (Context->hasExtension("GL_ARB_texture_rg"))
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

#define DUMMY_TEXTURE_ID 1

vector<MythGLTexture*> MythVTBInterop::Acquire(MythRenderOpenGL *Context,
                                               VideoColourSpace *ColourSpace,
                                               VideoFrame *Frame,
                                               FrameScanType)
{
    vector<MythGLTexture*> result;
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
        {
            ColourSpace->SetSupportedAttributes(static_cast<PictureAttributeSupported>
                                               (kPictureAttributeSupported_Brightness |
                                                kPictureAttributeSupported_Contrast |
                                                kPictureAttributeSupported_Colour |
                                                kPictureAttributeSupported_Hue |
                                                kPictureAttributeSupported_StudioLevels));
        }
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve pixel buffer
    // N.B. Buffer was retained in MythVTBContext and will be released in VideoBuffers
    CVPixelBufferRef buffer = reinterpret_cast<CVPixelBufferRef>(Frame->buf);
    if (!buffer)
        return result;

    int planes = CVPixelBufferGetPlaneCount(buffer);
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

    // Create necessary textures
    if (m_openglTextures.isEmpty())
    {
        for (int i = 0; i < planes; ++i)
        {
            int width  = CVPixelBufferGetWidthOfPlane(buffer, i);
            int height = CVPixelBufferGetHeightOfPlane(buffer, i);
            QSize size(width, height);
            MythGLTexture *texture = m_context->CreateTexture(size);
            if (texture)
                result.push_back(texture);
        }

        if (result.size() != static_cast<uint>(planes))
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all textures");
        m_openglTextures.insert(DUMMY_TEXTURE_ID, result);
    }

    if (m_openglTextures.contains(DUMMY_TEXTURE_ID))
    {
        result = m_openglTextures[DUMMY_TEXTURE_ID];
        m_context->glEnable(QOpenGLTexture::Target2D);
        
        // Update textures
        for (int i = 0; i < planes; ++i)
        {
            if (static_cast<uint>(i) >= result.size())
                continue;
        
            int width  = CVPixelBufferGetWidthOfPlane(buffer, i);
            int height = CVPixelBufferGetHeightOfPlane(buffer, i);
            int bytes  = CVPixelBufferGetBytesPerRowOfPlane(buffer, i) / width;
            void* buf  = CVPixelBufferGetBaseAddressOfPlane(buffer, i);
            result[i]->m_texture->bind();
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
            result[i]->m_texture->release();
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

vector<MythGLTexture*> MythVTBSurfaceInterop::Acquire(MythRenderOpenGL *Context,
                                                      VideoColourSpace *ColourSpace,
                                                      VideoFrame *Frame,
                                                      FrameScanType)
{
    vector<MythGLTexture*> result;
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
        {
            ColourSpace->SetSupportedAttributes(static_cast<PictureAttributeSupported>
                                               (kPictureAttributeSupported_Brightness |
                                                kPictureAttributeSupported_Contrast |
                                                kPictureAttributeSupported_Colour |
                                                kPictureAttributeSupported_Hue |
                                                kPictureAttributeSupported_StudioLevels));
        }
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

    // return cached textures if available
    if (m_openglTextures.contains(surfaceid))
        return m_openglTextures[surfaceid];

    // Create new and attach to IOSurface
    int planes = IOSurfaceGetPlaneCount(surface);
    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, nullptr);

    for (int plane = 0; plane < planes; ++plane)
    {
        int width  = IOSurfaceGetWidthOfPlane(surface, plane);
        int height = IOSurfaceGetHeightOfPlane(surface, plane);
        QSize size(width, height);

        MythGLTexture* texture = m_context->CreateExternalTexture(size, false);
        if (!texture)
            continue;
        texture->m_target = QOpenGLTexture::TargetRectangle;
        m_context->EnableTextures(QOpenGLTexture::TargetRectangle);
        m_context->glBindTexture(QOpenGLTexture::TargetRectangle, texture->m_textureId);

        // TODO proper handling of all formats - this assumes NV12
        GLenum format = plane == 0 ? QOpenGLTexture::Red : QOpenGLTexture::RG;
        CGLError error = CGLTexImageIOSurface2D(
                CGLGetCurrentContext(), QOpenGLTexture::TargetRectangle, format, width, height,
                format, QOpenGLTexture::UInt8, surface, plane);
        if (error != kCGLNoError)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("CGLTexImageIOSurface2D error %1").arg(error));

        m_context->glBindTexture(QOpenGLTexture::TargetRectangle, 0);
        result.push_back(texture);
    }

    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);

    m_openglTextures.insert(surfaceid, result);
    return result;
}
