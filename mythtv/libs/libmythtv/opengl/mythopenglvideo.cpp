// std
#include <utility>

// Qt
#include <QPen>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythui/opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "opengl/mythopengltonemap.h"
#include "opengl/mythopenglvideo.h"
#include "opengl/mythopenglvideoshaders.h"
#include "tv.h"

// FFmpeg
extern "C" {
#include "libavutil/stereo3d.h"
}

#define LOC QString("GLVid: ")
// static constexpr int8_t MAX_VIDEO_TEXTURES { 10 }; // YV12 Kernel deinterlacer + 1

/**
 * \class MythOpenGLVideo
 *
 * MythOpenGLVideo is responsible for displaying video frames on screen using OpenGL.
 * Frames may be sourced from main or graphics memory and MythOpenGLVideo must
 * handle colourspace conversion, deinterlacing and scaling as required.
 *
 * \note MythOpenGLVideo has no knowledge of buffering, timing and other presentation
 * state. Its role is to render video frames on screen.
*/
MythOpenGLVideo::MythOpenGLVideo(MythRenderOpenGL* Render, MythVideoColourSpace* ColourSpace,
                                 MythVideoBounds* Bounds, const MythVideoProfilePtr& VideoProfile, const QString& Profile)
  : MythVideoGPU(Render, ColourSpace, Bounds, VideoProfile, Profile),
    m_openglRender(Render)
{
    if (!m_openglRender || !m_videoColourSpace)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
        return;
    }

    OpenGLLocker ctx_lock(m_openglRender);
    if (m_openglRender->isOpenGLES())
        m_gles = m_openglRender->format().majorVersion();

    // Set OpenGL feature support
    m_features      = m_openglRender->GetFeatures();
    m_extraFeatures = m_openglRender->GetExtraFeatures();
    m_valid = true;

    m_chromaUpsamplingFilter = gCoreContext->GetBoolSetting("ChromaUpsamplingFilter", true);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Chroma upsampling filter %1")
        .arg(m_chromaUpsamplingFilter ? "enabled" : "disabled"));
}

MythOpenGLVideo::~MythOpenGLVideo()
{
    if (!m_openglRender)
        return;

    m_openglRender->makeCurrent();
    MythOpenGLVideo::ResetFrameFormat();
    delete m_toneMap;
    m_openglRender->doneCurrent();
}

void MythOpenGLVideo::ColourSpaceUpdate(bool PrimariesChanged)
{
    OpenGLLocker locker(m_openglRender);

    // if input/output type are unset - we haven't created the shaders yet
    if (PrimariesChanged && (m_outputType != FMT_NONE))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Primaries conversion changed - recreating shaders");
        SetupFrameFormat(m_inputType, m_outputType, m_videoDim, m_textureTarget);
    }

    float colourgamma  = m_videoColourSpace->GetColourGamma();
    float displaygamma = 1.0F / m_videoColourSpace->GetDisplayGamma();
    QMatrix4x4 primary = m_videoColourSpace->GetPrimaryMatrix();
    for (size_t i = Progressive; i < ShaderCount; ++i)
    {
        m_openglRender->SetShaderProgramParams(m_shaders[i], *m_videoColourSpace, "m_colourMatrix");
        m_openglRender->SetShaderProgramParams(m_shaders[i], primary, "m_primaryMatrix");
        if (m_shaders[i])
        {
            m_shaders[i]->setUniformValue("m_colourGamma", colourgamma);
            m_shaders[i]->setUniformValue("m_displayGamma", displaygamma);
        }
    }
}

void MythOpenGLVideo::UpdateShaderParameters()
{
    if (m_inputTextureSize.isEmpty())
        return;

    OpenGLLocker locker(m_openglRender);
    bool rect = m_textureTarget == QOpenGLTexture::TargetRectangle;
    GLfloat lineheight = rect ? 1.0F : 1.0F / m_inputTextureSize.height();
    GLfloat maxheight  = rect ? m_videoDispDim.height() : m_videoDispDim.height() /
                                static_cast<GLfloat>(m_inputTextureSize.height());
    GLfloat fieldsize  = rect ? 0.5F : m_inputTextureSize.height() / 2.0F;
    QVector4D parameters(lineheight,                                       /* lineheight */
                         static_cast<GLfloat>(m_inputTextureSize.width()), /* 'Y' select */
                         maxheight - lineheight,                           /* maxheight  */
                         fieldsize                                         /* fieldsize  */);

    for (size_t i = Progressive; i < ShaderCount; ++i)
    {
        if (m_shaders[i])
        {
            m_openglRender->EnableShaderProgram(m_shaders[i]);
            m_shaders[i]->setUniformValue("m_frameData", parameters);
            if (BicubicUpsize == i)
            {
                QVector2D size { rect ? 1.0F : static_cast<GLfloat>(m_videoDim.width()),
                                 rect ? 1.0F : static_cast<GLfloat>(m_videoDim.height()) };
                m_shaders[i]->setUniformValue("m_textureSize", size);
            }
        }
    }
}

QString MythOpenGLVideo::GetProfile() const
{
    if (MythVideoFrame::HardwareFormat(m_inputType))
        return TypeToProfile(m_inputType);
    return TypeToProfile(m_outputType);
}

void MythOpenGLVideo::CleanupDeinterlacers()
{
    // If switching off/from basic deinterlacing, then we need to delete and
    // recreate the input textures and sometimes the shaders as well - so start
    // from scratch
    if (m_deinterlacer == DEINT_BASIC && MythVideoFrame::YUVFormat(m_inputType))
    {
        // Note. Textures will be created with linear filtering - which matches
        // no resizing - which should be the case for the basic deinterlacer - and
        // the call to SetupFrameFormat will reset resizing anyway
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Removing single field textures");
        // revert to YUY2 if preferred
        if ((m_inputType == FMT_YV12) && (m_profile == "opengl"))
            m_outputType = FMT_YUY2;
        SetupFrameFormat(m_inputType, m_outputType, m_videoDim, m_textureTarget);
        emit OutputChanged(m_videoDim, m_videoDim, -1.0F);
    }
    m_fallbackDeinterlacer = DEINT_NONE;
    m_deinterlacer = DEINT_NONE;
    m_deinterlacer2x = false;
}

bool MythOpenGLVideo::AddDeinterlacer(const MythVideoFrame* Frame, FrameScanType Scan,
                                      MythDeintType Filter  /* = DEINT_SHADER */,
                                      bool CreateReferences /* = true */)
{
    if (!Frame)
        return false;

    // do we want an opengl shader?
    // shaders trump CPU deinterlacers if selected and driver deinterlacers will only
    // be available under restricted circumstances
    // N.B. there should in theory be no situation in which shader deinterlacing is not
    // available for software formats, hence there should be no need to fallback to cpu

    if (!is_interlaced(Scan) || Frame->m_alreadyDeinterlaced)
    {
        CleanupDeinterlacers();
        return false;
    }

    m_deinterlacer2x = true;
    MythDeintType deinterlacer = Frame->GetDoubleRateOption(Filter);
    MythDeintType other        = Frame->GetDoubleRateOption(DEINT_DRIVER);
    if (other) // another double rate deinterlacer is enabled
    {
        CleanupDeinterlacers();
        return false;
    }

    if (!deinterlacer)
    {
        m_deinterlacer2x = false;
        deinterlacer = Frame->GetSingleRateOption(Filter);
        other        = Frame->GetSingleRateOption(DEINT_DRIVER);
        if (!deinterlacer || other) // no shader deinterlacer needed
        {
            CleanupDeinterlacers();
            return false;
        }
    }

    // if we get this far, we cannot use driver deinterlacers, shader deints
    // are preferred over cpu, we have a deinterlacer but don't actually care whether
    // it is single or double rate
    if (m_deinterlacer == deinterlacer || (m_fallbackDeinterlacer && (m_fallbackDeinterlacer == deinterlacer)))
        return true;

    // Lock
    OpenGLLocker ctx_lock(m_openglRender);

    // delete old reference textures
    MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_prevTextures);
    MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_nextTextures);

    // For basic deinterlacing of software frames, we now create 2 sets of field
    // based textures - which is the same approach taken by the CPU based onefield/bob
    // deinterlacer and the EGL basic deinterlacer. The advantages of this
    // approach are:-
    //  - no dependent texturing in the samplers (it is just a basic YUV to RGB conversion
    //    in the shader)
    //  - better quality (the onefield shader line doubles but does not have the
    //    implicit interpolation/smoothing of using separate textures directly,
    //    which leads to 'blockiness').
    //  - as we are not sampling other fields, there is no need to use an intermediate
    //    framebuffer to ensure accurate sampling - so we can skip the resize stage.
    //
    // YUYV formats are currently not supported as it does not work correctly - force YV12 instead.

    if (deinterlacer == DEINT_BASIC && MythVideoFrame::YUVFormat(m_inputType))
    {
        if (m_outputType == FMT_YUY2)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Forcing OpenGL YV12 for basic deinterlacer");
            m_outputType = FMT_YV12;
        }
        MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_inputTextures);
        QSize size(m_videoDim.width(), m_videoDim.height() >> 1);
        std::vector<QSize> sizes;
        sizes.emplace_back(size);
        // N.B. If we are currently resizing, it will be turned off for this
        // deinterlacer, so the default linear texture filtering is OK.
        m_inputTextures = MythVideoTextureOpenGL::CreateTextures(m_openglRender, m_inputType, m_outputType, sizes);
        // nextTextures will hold the other field
        m_nextTextures = MythVideoTextureOpenGL::CreateTextures(m_openglRender, m_inputType, m_outputType, sizes);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 single field textures")
            .arg(m_inputTextures.size() * 2));
        // Con MythVideoBounds into display the field only
        emit OutputChanged(m_videoDim, size, -1.0F);
    }

    // sanity check max texture units. Should only be an issue on old hardware (e.g. Pi)
    int max = m_openglRender->GetMaxTextureUnits();
    uint refstocreate = ((deinterlacer == DEINT_HIGH) && CreateReferences) ? 2 : 0;
    int totaltextures = static_cast<int>(MythVideoFrame::GetNumPlanes(m_outputType)) * static_cast<int>(refstocreate + 1);
    if (totaltextures > max)
    {
        m_fallbackDeinterlacer = deinterlacer;
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Insufficent texture units for deinterlacer '%1' (%2 < %3)")
            .arg(MythVideoFrame::DeinterlacerName(deinterlacer | DEINT_SHADER, m_deinterlacer2x)).arg(max).arg(totaltextures));
        deinterlacer = DEINT_BASIC;
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Falling back to '%1'")
            .arg(MythVideoFrame::DeinterlacerName(deinterlacer | DEINT_SHADER, m_deinterlacer2x)));
    }

    // create new deinterlacers - the old ones will be deleted
    if (!(CreateVideoShader(InterlacedBot, deinterlacer) && CreateVideoShader(InterlacedTop, deinterlacer)))
        return false;

    // create the correct number of reference textures
    if (refstocreate)
    {
        std::vector<QSize> sizes;
        sizes.emplace_back(m_videoDim);
        m_prevTextures = MythVideoTextureOpenGL::CreateTextures(m_openglRender, m_inputType, m_outputType, sizes);
        m_nextTextures = MythVideoTextureOpenGL::CreateTextures(m_openglRender, m_inputType, m_outputType, sizes);
        // ensure we use GL_NEAREST if resizing is already active and needed
        if ((m_resizing & Sampling) == Sampling)
        {
            MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_prevTextures, QOpenGLTexture::Nearest);
            MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_nextTextures, QOpenGLTexture::Nearest);
        }
    }

    // ensure they work correctly
    UpdateColourSpace(false);
    UpdateShaderParameters();
    m_deinterlacer = deinterlacer;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created deinterlacer '%1' (%2->%3)")
        .arg(MythVideoFrame::DeinterlacerName(m_deinterlacer | DEINT_SHADER, m_deinterlacer2x),
             MythVideoFrame::FormatDescription(m_inputType),
             MythVideoFrame::FormatDescription(m_outputType)));
    return true;
}

/*! \brief Create the appropriate shader for the operation Type
 *
 * \note Shader cost is calculated as 1 per texture read and 2 per dependent read.
 * If there are alternative shader conditions, the worst case is used.
 * (A dependent read is defined as any texture read that does not use the exact
 * texture coordinates passed into the fragment shader)
*/
bool MythOpenGLVideo::CreateVideoShader(VideoShaderType Type, MythDeintType Deint)
{
    if (!m_openglRender || !(m_features & QOpenGLFunctions::Shaders))
        return false;

    // delete the old
    if (m_shaders[Type])
        m_openglRender->DeleteShaderProgram(m_shaders[Type]);
    m_shaders[Type] = nullptr;

    QStringList defines;
    QString vertex = DefaultVertexShader;
    QString fragment;
    int cost = 1;

    if (m_textureTarget == GL_TEXTURE_EXTERNAL_OES)
        defines << "EXTOES";

    if ((Default == Type) || (BicubicUpsize == Type) || (!MythVideoFrame::YUVFormat(m_outputType)))
    {
        QString glsldefines;
        for (const QString& define : std::as_const(defines))
            glsldefines += QString("#define MYTHTV_%1\n").arg(define);
        fragment = glsldefines + YUVFragmentExtensions + ((BicubicUpsize == Type) ? BicubicShader : RGBFragmentShader);

#ifdef USING_MEDIACODEC
        if (FMT_MEDIACODEC == m_inputType)
            vertex = MediaCodecVertexShader;
#endif
    }
    // no interlaced shaders yet (i.e. interlaced chroma upsampling - not deinterlacers)
    else
    {
        fragment = YUVFragmentShader;
        QString extensions = YUVFragmentExtensions;
        QString glsldefines;

        // Any software frames that are not 8bit need to use unsigned integer
        // samplers with GLES3.x - which need more modern shaders
        if ((m_gles > 2) && (MythVideoFrame::ColorDepth(m_inputType) > 8))
        {
            static const QString glsl300("#version 300 es\n");
            fragment   = GLSL300YUVFragmentShader;
            extensions = GLSL300YUVFragmentExtensions;
            vertex     = glsl300 + GLSL300VertexShader;
            glsldefines.append(glsl300);
        }

        bool kernel = false;
        bool topfield = InterlacedTop == Type;
        bool progressive = (Progressive == Type) || (Deint == DEINT_NONE);
        if (MythVideoFrame::FormatIs420(m_outputType) || MythVideoFrame::FormatIs422(m_outputType) || MythVideoFrame::FormatIs444(m_outputType))
        {
            defines << "YV12";
            cost = 3;
        }
        else if (MythVideoFrame::FormatIsNV12(m_outputType))
        {
            defines << "NV12";
            cost = 2;
        }
        else if (FMT_YUY2 == m_outputType)
        {
            defines << "YUY2";
        }

#ifdef USING_VTB
        // N.B. Rectangular texture support is only currently used for VideoToolBox
        // video frames which are NV12. Do not use rectangular textures for the 'default'
        // shaders as it breaks video resizing and would require changes to our
        // FramebufferObject code.
        if ((m_textureTarget == QOpenGLTexture::TargetRectangle) && (Default != Type))
            defines << "RECTS";
#endif
        if (!progressive)
        {
            bool basic = Deint == DEINT_BASIC && MythVideoFrame::YUVFormat(m_inputType);
            // Chroma upsampling filter
            if ((MythVideoFrame::FormatIs420(m_outputType) || MythVideoFrame::FormatIsNV12(m_outputType)) &&
                m_chromaUpsamplingFilter && !basic)
            {
                defines << "CUE";
            }

            // field
            if (topfield && !basic)
                defines << "TOPFIELD";

            switch (Deint)
            {
                case DEINT_BASIC:
                if (!basic)
                {
                    cost *= 2;
                    defines << "ONEFIELD";
                }
                break;
                case DEINT_MEDIUM: cost *= 5;  defines << "LINEARBLEND"; break;
                case DEINT_HIGH:   cost *= 15; defines << "KERNEL"; kernel = true; break;
                default: break;
            }
        }

        // Start building the new fragment shader
        // We do this in code otherwise the shader string becomes monolithic

        // 'expand' calls to sampleYUV for multiple planes
        // do this before we add the samplers
        int count = static_cast<int>(MythVideoFrame::GetNumPlanes(m_outputType));
        for (int i = (kernel ? 2 : 0); (i >= 0) && count; i--)
        {
            QString find = QString("s_texture%1").arg(i);
            QStringList replacelist;
            for (int j = (i * count); j < ((i + 1) * count); ++j)
                replacelist << QString("s_texture%1").arg(j);
            fragment.replace(find, replacelist.join(", "));
        }

        // 'expand' calls to the kernel function
        if (kernel && count)
        {
            for (int i = 1 ; i >= 0; i--)
            {
                QString find1 = QString("sampler2D kernelTex%1").arg(i);
                QString find2 = QString("kernelTex%1").arg(i);
                QStringList replacelist1;
                QStringList replacelist2;
                for (int j = 0; j < count; ++j)
                {
                    replacelist1 << QString("sampler2D kernelTexture%1%2").arg(i).arg(j);
                    replacelist2 << QString("kernelTexture%1%2").arg(i).arg(j);
                }
                fragment.replace(find1, replacelist1.join(", "));
                fragment.replace(find2, replacelist2.join(", "));
            }
        }

        // Retrieve colour mappping defines
        defines << m_videoColourSpace->GetColourMappingDefines();

        // Add defines
        for (const QString& define : std::as_const(defines))
            glsldefines += QString("#define MYTHTV_%1\n").arg(define);

        // Add the required samplers
        int start = 0;
        int end   = count;
        if (kernel)
        {
            end *= 3;
            if (topfield)
                start += count;
            else
                end -= count;
        }
        QString glslsamplers;
        for (int i = start; i < end; ++i)
            glslsamplers += QString("uniform sampler2D s_texture%1;\n").arg(i);

        // construct the final shader string
        fragment = glsldefines + extensions + glslsamplers + fragment;
    }

    m_shaderCost[Type] = cost;
    QOpenGLShaderProgram *program = m_openglRender->CreateShaderProgram(vertex, fragment);
    if (!program)
        return false;

    m_shaders[Type] = program;
    return true;
}

bool MythOpenGLVideo::SetupFrameFormat(VideoFrameType InputType, VideoFrameType OutputType,
                                       QSize Size, GLenum TextureTarget)
{
    QString texnew { "2D" };
    if (TextureTarget == QOpenGLTexture::TargetRectangle)
        texnew = "Rect";
    else if (TextureTarget == GL_TEXTURE_EXTERNAL_OES)
        texnew = "OES";

    QString texold { "2D" };
    if (m_textureTarget == QOpenGLTexture::TargetRectangle)
        texold = "Rect";
    else if (m_textureTarget == GL_TEXTURE_EXTERNAL_OES)
        texold = "OES";

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("New frame format: %1:%2 %3x%4 (Tex: %5) -> %6:%7 %8x%9 (Tex: %10)")
        .arg(MythVideoFrame::FormatDescription(m_inputType),
             MythVideoFrame::FormatDescription(m_outputType),
             QString::number(m_videoDim.width()),
             QString::number(m_videoDim.height()),
             texold,
             MythVideoFrame::FormatDescription(InputType),
             MythVideoFrame::FormatDescription(OutputType),
             QString::number(Size.width()),
             QString::number(Size.height()))
        .arg(texnew));

    ResetFrameFormat();

    m_inputType = InputType;
    m_outputType = OutputType;
    m_textureTarget = TextureTarget;
    m_videoDim = Size;

    // This is only currently needed for RGBA32 frames from composed DRM_PRIME
    // textures that may be half height for simple bob deinterlacing
    if (m_inputType == FMT_DRMPRIME && m_outputType == FMT_RGBA32)
        emit OutputChanged(m_videoDim, m_videoDim, -1.0F);

    if (!MythVideoFrame::HardwareFormat(InputType))
    {
        std::vector<QSize> sizes;
        sizes.push_back(Size);
        m_inputTextures = MythVideoTextureOpenGL::CreateTextures(m_openglRender, m_inputType, m_outputType, sizes);
        if (m_inputTextures.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create input textures");
            return false;
        }

        m_inputTextureSize = m_inputTextures[0]->m_totalSize;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 input textures for '%2'")
            .arg(m_inputTextures.size()).arg(GetProfile()));
    }
    else
    {
        m_inputTextureSize = Size;
    }

    // Create shaders
    if (!CreateVideoShader(Default) || !CreateVideoShader(Progressive))
        return false;

    UpdateColourSpace(false);
    UpdateShaderParameters();
    return true;
}

void MythOpenGLVideo::ResetFrameFormat()
{
    for (auto & shader : m_shaders)
        if (shader)
            m_openglRender->DeleteShaderProgram(shader);
    m_shaders.fill(nullptr);
    m_shaderCost.fill(1);
    MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_inputTextures);
    MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_prevTextures);
    MythVideoTextureOpenGL::DeleteTextures(m_openglRender, m_nextTextures);
    m_textureTarget = QOpenGLTexture::Target2D;
    m_fallbackDeinterlacer = DEINT_NONE;
    m_openglRender->DeleteFramebuffer(m_frameBuffer);
    m_openglRender->DeleteTexture(m_frameBufferTexture);
    m_frameBuffer = nullptr;
    m_frameBufferTexture = nullptr;

    MythVideoGPU::ResetFrameFormat();
}

/// \brief Update the current input texture using the data from the given video frame.
void MythOpenGLVideo::PrepareFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    if (Frame->m_type == FMT_NONE)
        return;

    // Hardware frames are retrieved/updated in PrepareFrame but we need to
    // reset software frames now if necessary
    if (MythVideoFrame::HardwareFormat(Frame->m_type))
    {
        if ((!MythVideoFrame::HardwareFormat(m_inputType)) && (m_inputType != FMT_NONE))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Resetting input format");
            ResetFrameFormat();
        }
        return;
    }

    // Sanitise frame
    if ((Frame->m_width < 1) || (Frame->m_height < 1) || !Frame->m_buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame");
        return;
    }

    // Can we render this frame format
    if (!MythVideoFrame::YUVFormat(Frame->m_type))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Frame format is not supported");
        return;
    }

    // lock
    OpenGLLocker ctx_lock(m_openglRender);

    // check for input changes
    if ((Frame->m_width  != m_videoDim.width()) ||
        (Frame->m_height != m_videoDim.height()) ||
        (Frame->m_type  != m_inputType))
    {
        VideoFrameType frametype = Frame->m_type;
        if ((frametype == FMT_YV12) && (m_profile == "opengl"))
            frametype = FMT_YUY2;
        QSize size(Frame->m_width, Frame->m_height);
        if (!SetupFrameFormat(Frame->m_type, frametype, size, QOpenGLTexture::Target2D))
            return;
    }

    // Setup deinterlacing if required
    AddDeinterlacer(Frame, Scan);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "UPDATE_FRAME_START");

    m_videoColourSpace->UpdateColourSpace(Frame);

    // Rotate textures if necessary
    bool current = true;
    if (m_deinterlacer == DEINT_MEDIUM || m_deinterlacer == DEINT_HIGH)
    {
        if (!m_nextTextures.empty() && !m_prevTextures.empty())
        {
            if (qAbs(Frame->m_frameCounter - m_discontinuityCounter) > 1)
                ResetTextures();
            std::vector<MythVideoTextureOpenGL*> temp = m_prevTextures;
            m_prevTextures = m_inputTextures;
            m_inputTextures = m_nextTextures;
            m_nextTextures = temp;
            current = false;
        }
    }

    m_discontinuityCounter = Frame->m_frameCounter;

    if (m_deinterlacer == DEINT_BASIC)
    {
        // first field. Fake the pitches
        FramePitches pitches = Frame->m_pitches;
        Frame->m_pitches[0] = Frame->m_pitches[0] << 1;
        Frame->m_pitches[1] = Frame->m_pitches[1] << 1;
        Frame->m_pitches[2] = Frame->m_pitches[2] << 1;
        MythVideoTextureOpenGL::UpdateTextures(m_openglRender, Frame, m_inputTextures);
        // second field. Fake the offsets as well.
        FrameOffsets offsets = Frame->m_offsets;
        Frame->m_offsets[0] = Frame->m_offsets[0] + pitches[0];
        Frame->m_offsets[1] = Frame->m_offsets[1] + pitches[1];
        Frame->m_offsets[2] = Frame->m_offsets[2] + pitches[2];
        MythVideoTextureOpenGL::UpdateTextures(m_openglRender, Frame, m_nextTextures);
        Frame->m_pitches = pitches;
        Frame->m_offsets = offsets;
    }
    else
    {
        MythVideoTextureOpenGL::UpdateTextures(m_openglRender, Frame, current ? m_inputTextures : m_nextTextures);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void MythOpenGLVideo::RenderFrame(MythVideoFrame* Frame, bool TopFieldFirst, FrameScanType Scan,
                                  StereoscopicMode StereoOverride, bool DrawBorder)
{
    if (!m_openglRender)
        return;

    OpenGLLocker locker(m_openglRender);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "RENDER_FRAME_START");

    // Set required input textures for the last stage
    // ProcessFrame is always called first, which will create/destroy software
    // textures as needed
    bool hwframes = false;
    bool useframebufferimage = false;

    // for tiled renderers (e.g. Pi), enable scissoring to try and improve performance
    // when not full screen and avoid the resize stage unless absolutely necessary
    bool tiled = m_extraFeatures & kGLTiled;

    // We lose the pause frame when seeking and using VDPAU/VAAPI/NVDEC direct rendering.
    // As a workaround, we always use the resize stage so the last displayed frame
    // should be retained in the Framebuffer used for resizing. If there is
    // nothing to display, then fallback to this framebuffer.
    // N.B. this is now strictly necessary with v4l2 and DRM PRIME direct rendering
    // but ignore now for performance reasons
    VideoResizing resize;
    if (Frame)
        resize = (MythVideoFrame::HardwareFramesFormat(Frame->m_type) ? Framebuffer : None);
    else
        resize = (MythVideoFrame::HardwareFramesFormat(m_inputType)  ? Framebuffer : None);

    std::vector<MythVideoTextureOpenGL*> inputtextures = m_inputTextures;
    if (inputtextures.empty())
    {
        // This is experimental support for direct rendering to a framebuffer (e.g. DRM).
        // It may be removed or refactored (e.g. pass presentation details through to
        // the interop).
        if (Frame)
        {
            Frame->m_displayed = false;
            Frame->m_srcRect = m_videoRect;
            Frame->m_dstRect = m_displayVideoRect;
        }

        // Pull in any hardware frames
        inputtextures = MythOpenGLInterop::Retrieve(m_openglRender, m_videoColourSpace, Frame, Scan);

        if (Frame && Frame->m_displayed)
            return;

        if (!inputtextures.empty())
        {
            hwframes = true;
            QSize newsize = inputtextures[0]->m_size;
            VideoFrameType newsourcetype = inputtextures[0]->m_frameType;
            VideoFrameType newtargettype = inputtextures[0]->m_frameFormat;
            GLenum newtargettexture = inputtextures[0]->m_target;
            if ((m_outputType != newtargettype) || (m_textureTarget != newtargettexture) ||
                (m_inputType != newsourcetype) || (newsize != m_inputTextureSize))
            {
                SetupFrameFormat(newsourcetype, newtargettype, newsize, newtargettexture);
            }

#ifdef USING_MEDIACODEC
            // Set the texture transform for mediacodec
            if (FMT_MEDIACODEC == m_inputType)
            {
                if (inputtextures[0]->m_transform && m_shaders[Default])
                {
                    m_openglRender->EnableShaderProgram(m_shaders[Default]);
                    m_shaders[Default]->setUniformValue("u_transform", *inputtextures[0]->m_transform);
                }
            }
#endif
            // Enable deinterlacing for NVDEC, VTB and VAAPI DRM if VPP is not available
            if (inputtextures[0]->m_allowGLSLDeint)
                AddDeinterlacer(Frame, Scan, DEINT_SHADER | DEINT_CPU, false); // pickup shader or cpu prefs
        }
        else
        {
            if ((resize == Framebuffer) && m_frameBuffer && m_frameBufferTexture)
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, "Using existing framebuffer");
                useframebufferimage = true;
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Nothing to display");
                // if this is live tv startup and the window rect has changed we
                // must set the viewport
                m_openglRender->SetViewPort(QRect(QPoint(), m_masterViewportSize));
                return;
            }
        }
    }

    // Determine which shader to use. This helps optimise the resize check.
    bool deinterlacing = false;
    bool basicdeinterlacing = false;
    bool yuvoutput = MythVideoFrame::YUVFormat(m_outputType);
    VideoShaderType program = yuvoutput ? Progressive : Default;
    if (m_deinterlacer != DEINT_NONE)
    {
        if (Scan == kScan_Interlaced)
        {
            program = TopFieldFirst ? InterlacedTop : InterlacedBot;
            deinterlacing = true;
        }
        else if (Scan == kScan_Intr2ndField)
        {
            program = TopFieldFirst ? InterlacedBot : InterlacedTop;
            deinterlacing = true;
        }

        // select the correct field for the basic deinterlacer
        if (deinterlacing && m_deinterlacer == DEINT_BASIC && MythVideoFrame::YUVFormat(m_inputType))
        {
            basicdeinterlacing = true;
            if (program == InterlacedBot)
                inputtextures = m_nextTextures;
        }
    }

    // Set deinterlacer type for debug OSD
    if (deinterlacing && Frame)
    {
        Frame->m_deinterlaceInuse = m_deinterlacer | DEINT_SHADER;
        Frame->m_deinterlaceInuse2x = m_deinterlacer2x;
    }

    // Tonemapping can only render to a texture
    if (m_toneMap)
        resize |= ToneMap;

    // Decide whether to use render to texture - for performance or quality
    if (yuvoutput && !resize)
    {
        // ensure deinterlacing works correctly when down scaling in height
        // N.B. not needed for the basic deinterlacer
        if (deinterlacing && !basicdeinterlacing && (m_videoDispDim.height() > m_displayVideoRect.height()))
            resize |= Deinterlacer;

        // NB GL_NEAREST introduces some 'minor' chroma sampling errors
        // for the following 2 cases. For YUY2 this may be better handled in the
        // shader. For GLES3.0 10bit textures - Vulkan is probably the better solution.

        // UYVY packed pixels must be sampled exactly with GL_NEAREST
        if (FMT_YUY2 == m_outputType)
            resize |= Sampling;
        // unsigned integer texture formats need GL_NEAREST sampling
        if ((m_gles > 2) && (MythVideoFrame::ColorDepth(m_inputType) > 8))
            resize |= Sampling;

        // don't enable resizing if the cost of a framebuffer switch may be
        // prohibitive (e.g. Raspberry Pi/tiled renderers) or for basic deinterlacing,
        // where we are trying to simplify/optimise rendering (and the framebuffer
        // sizing gets confused by the change to m_videoDispDim)
        if (!resize && !tiled && !basicdeinterlacing)
        {
            // improve performance. This is an educated guess on the relative cost
            // of render to texture versus straight rendering.
            int totexture    = m_videoDispDim.width() * m_videoDispDim.height() * m_shaderCost[program];
            int blitcost     = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[Default];
            int noresizecost = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[program];
            if ((totexture + blitcost) < noresizecost)
                resize |= Performance;
        }
    }

    // Bicubic upsizing - test this after all other resize options have been checked
    // to ensure it is not the only flag set
    if (m_bicubicUpsize)
        SetupBicubic(resize);

    // We don't need an extra stage prior to bicubic if the frame is already RGB (e.g. VDPAU, MediaCodec)
    // So bypass if we only set resize for bicubic.
    bool needresize = resize && (!MythVideoFrame::FormatIsRGB(m_outputType) || (resize != Bicubic));

    // set software frame filtering if resizing has changed
    if (!needresize && m_resizing)
    {
        // remove framebuffer
        if (m_frameBufferTexture)
        {
            m_openglRender->DeleteTexture(m_frameBufferTexture);
            m_frameBufferTexture = nullptr;
        }
        if (m_frameBuffer)
        {
            m_openglRender->DeleteFramebuffer(m_frameBuffer);
            m_frameBuffer = nullptr;
        }
        // set filtering
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_inputTextures, QOpenGLTexture::Linear);
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_prevTextures, QOpenGLTexture::Linear);
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_nextTextures, QOpenGLTexture::Linear);
        m_resizing = None;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled resizing");
    }
    else if (!m_resizing && needresize)
    {
        // framebuffer will be created as needed below
        QOpenGLTexture::Filter filter = ((resize & Sampling) == Sampling) ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear;
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_inputTextures, filter);
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_prevTextures, filter);
        MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, m_nextTextures, filter);
        m_resizing = resize;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Resizing from %1x%2 to %3x%4 for %5")
            .arg(m_videoDispDim.width()).arg(m_videoDispDim.height())
            .arg(m_displayVideoRect.width()).arg(m_displayVideoRect.height())
            .arg(VideoResizeToString(resize)));
    }

    // check hardware frames have the correct filtering
    if (hwframes)
    {
        QOpenGLTexture::Filter filter = (resize.testFlag(Sampling)) ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear;
        if (inputtextures[0]->m_filter != filter)
            MythVideoTextureOpenGL::SetTextureFilters(m_openglRender, inputtextures, filter);
    }

    // texture coordinates
    QRect trect(m_videoRect);

    if (needresize)
    {
        MythVideoTextureOpenGL* nexttexture = nullptr;

        // only render to the framebuffer if there is something to update
        if (useframebufferimage)
        {
            if (m_toneMap)
            {
                nexttexture = m_toneMap->GetTexture();
                trect = QRect(QPoint(0, 0), m_displayVideoRect.size());
            }
            else
            {
                nexttexture = m_frameBufferTexture;
            }
        }
        else if (m_toneMap)
        {
            if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
                m_openglRender->logDebugMarker(LOC + "RENDER_TO_TEXTURE");
            nexttexture = m_toneMap->Map(inputtextures, m_displayVideoRect.size());
            trect = QRect(QPoint(0, 0), m_displayVideoRect.size());
        }
        else
        {
            // render to texture stage
            if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
                m_openglRender->logDebugMarker(LOC + "RENDER_TO_TEXTURE");

            // we need a framebuffer and associated texture
            if (!m_frameBuffer)
            {
                if (auto [fbo, tex] = MythVideoTextureOpenGL::CreateVideoFrameBuffer(m_openglRender, m_outputType, m_videoDispDim);
                    (fbo != nullptr) && (tex != nullptr))
                {
                    delete m_frameBuffer;
                    delete m_frameBufferTexture;
                    m_frameBuffer = fbo;
                    m_frameBufferTexture = tex;
                    m_openglRender->SetTextureFilters(m_frameBufferTexture, QOpenGLTexture::Linear);
                }
            }

            if (!(m_frameBuffer && m_frameBufferTexture))
                return;

            // coordinates
            QRect vrect(QPoint(0, 0), m_videoDispDim);
            QRect trect2 = vrect;
            if (FMT_YUY2 == m_outputType)
                trect2.setWidth(m_videoDispDim.width() >> 1);

            // framebuffer
            m_openglRender->BindFramebuffer(m_frameBuffer);
            m_openglRender->SetViewPort(vrect);

            // bind correct textures
            std::vector<MythGLTexture*> textures {};
            BindTextures(deinterlacing, inputtextures, textures);

            // render
            m_openglRender->DrawBitmap(textures, m_frameBuffer, trect2, vrect,
                                       m_shaders[program], 0);
            nexttexture = m_frameBufferTexture;
        }

        // reset for next stage
        inputtextures.clear();
        inputtextures.push_back(nexttexture);
        program = Default;
        deinterlacing = false;
    }

    // Use the bicubic shader if necessary
    if (resize.testFlag(Bicubic))
        program = BicubicUpsize;

    // render to default framebuffer/screen
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "RENDER_TO_SCREEN");

    // discard stereoscopic fields
    StereoscopicMode stereo = StereoOverride;
    m_lastStereo = Frame ? Frame->m_stereo3D : m_lastStereo;
    // N.B. kStereoscopicModeSideBySideDiscard is a proxy here for discard of all types
    if ((stereo == kStereoscopicModeAuto) &&
        (m_stereoMode == kStereoscopicModeSideBySideDiscard) &&
        (m_lastStereo != AV_STEREO3D_2D))
    {
        if (m_lastStereo == AV_STEREO3D_SIDEBYSIDE)
            stereo = kStereoscopicModeSideBySideDiscard;
        else if (m_lastStereo == AV_STEREO3D_TOPBOTTOM)
            stereo = kStereoscopicModeTopAndBottomDiscard;
    }

    if (kStereoscopicModeSideBySideDiscard == stereo)
        trect = QRect(trect.left() >> 1, trect.top(), trect.width() >> 1, trect.height());
    else if (kStereoscopicModeTopAndBottomDiscard == stereo)
        trect = QRect(trect.left(), trect.top() >> 1, trect.width(), trect.height() >> 1);

    // bind default framebuffer
    m_openglRender->BindFramebuffer(nullptr);
    m_openglRender->SetViewPort(QRect(QPoint(), m_masterViewportSize));

    // PiP border
    if (DrawBorder)
    {
        QRect piprect = m_displayVideoRect.adjusted(-10, -10, +10, +10);
        static const QPen kNopen(Qt::NoPen);
        static const QBrush kRedBrush(QBrush(QColor(127, 0, 0, 255)));
        m_openglRender->DrawRect(nullptr, piprect, kRedBrush, kNopen, 255);
    }

    // bind correct textures
    std::vector<MythGLTexture*> textures;
    BindTextures(deinterlacing, inputtextures, textures);

    // rotation
    if (Frame)
        m_lastRotation = Frame->m_rotation;

    // apply scissoring
    if (tiled)
    {
        // N.B. It's not obvious whether this helps
        m_openglRender->glEnable(GL_SCISSOR_TEST);
        m_openglRender->glScissor(m_displayVideoRect.left() - 1, m_displayVideoRect.top() - 1,
                                  m_displayVideoRect.width() + 2, m_displayVideoRect.height() + 2);
    }

    // draw
    m_openglRender->DrawBitmap(textures, nullptr, trect, m_displayVideoRect,
                               m_shaders[program], m_lastRotation);

    // disable scissoring
    if (tiled)
        m_openglRender->glDisable(GL_SCISSOR_TEST);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "RENDER_FRAME_END");
}

/// \brief Clear reference frames after a seek as they will contain old images.
void MythOpenGLVideo::ResetTextures()
{
    for (auto & texture : m_inputTextures)
        texture->m_valid = false;
    for (auto & texture : m_prevTextures)
        texture->m_valid = false;
    for (auto & texture : m_nextTextures)
        texture->m_valid = false;
}

void MythOpenGLVideo::BindTextures(bool Deinterlacing, std::vector<MythVideoTextureOpenGL*>& Current,
                                   std::vector<MythGLTexture*>& Textures)
{
    if (Deinterlacing && !MythVideoFrame::HardwareFormat(m_inputType))
    {
        if ((m_nextTextures.size() == Current.size()) && (m_prevTextures.size() == Current.size()))
        {
            // if we are using reference frames, we want the current frame in the middle
            // but next will be the first valid, followed by current...
            size_t count = Current.size();
            std::vector<MythVideoTextureOpenGL*>& current = Current[0]->m_valid ? Current : m_nextTextures;
            std::vector<MythVideoTextureOpenGL*>& prev    = m_prevTextures[0]->m_valid ? m_prevTextures : current;

            for (uint i = 0; i < count; ++i)
                Textures.push_back(reinterpret_cast<MythGLTexture*>(prev[i]));
            for (uint i = 0; i < count; ++i)
                Textures.push_back(reinterpret_cast<MythGLTexture*>(current[i]));
            for (uint i = 0; i < count; ++i)
                Textures.push_back(reinterpret_cast<MythGLTexture*>(m_nextTextures[i]));
            return;
        }
    }

    std::transform(Current.cbegin(), Current.cend(), std::back_inserter(Textures),
                   [](MythVideoTextureOpenGL* Tex) { return reinterpret_cast<MythGLTexture*>(Tex); });
}

QString MythOpenGLVideo::TypeToProfile(VideoFrameType Type)
{
    if (MythVideoFrame::HardwareFormat(Type))
        return "opengl-hw";

    switch (Type)
    {
        case FMT_YUY2:    return "opengl"; // compatibility with old profiles
        case FMT_YV12:    return "opengl-yv12";
        case FMT_NV12:    return "opengl-nv12";
        default: break;
    }
    return "opengl";
}

void MythOpenGLVideo::SetupBicubic(VideoResizing& Resize)
{
    if (((m_videoDispDim.width() < m_displayVideoRect.width()) ||
         (m_videoDispDim.height() < m_displayVideoRect.height())))
    {
        if (!m_shaders[BicubicUpsize])
        {
            if (!CreateVideoShader(BicubicUpsize))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create bicubic shader. Disabling");
                m_bicubicUpsize = false;
            }
            else
            {
                UpdateShaderParameters();
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created bicubic sampler");
            }
        }

        if (m_shaders[BicubicUpsize])
            Resize |= Bicubic;
    }
    else
    {
        if (m_shaders[BicubicUpsize] != nullptr)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabling bicubic sampler");
            delete m_shaders[BicubicUpsize];
            m_shaders[BicubicUpsize] = nullptr;
        }
    }
}
