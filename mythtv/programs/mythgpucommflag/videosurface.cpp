#include <QFile>

#include <strings.h>

#include "mythlogging.h"
#include "openclinterface.h"
#include "videosurface.h"
#include "vdpauvideodecoder.h"
#include "openglsupport.h"

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#include <oclUtils.h>

VideoSurface::VideoSurface(OpenCLDevice *dev, uint32_t width, uint32_t height,
                           uint id, VdpVideoSurface vdpSurface) : 
    m_type(kSurfaceVDPAURender), m_id(id), m_dev(dev), m_width(width),
    m_height(height), m_vdpSurface(vdpSurface), m_bufCount(4), m_valid(false)
{
    memset(&m_render, 0, sizeof(m_render));
    m_clBuffer = new cl_mem[m_bufCount];
    memset(m_clBuffer, 0, m_bufCount * sizeof(cl_mem));
    m_render.surface = m_vdpSurface;

    m_realWidth = m_width;
    m_realHeight = m_height / 2;
    char *zeros = new char[m_realWidth*m_realHeight];

    glGenTextures(4, m_glVDPAUTex);
    glGenTextures(4, m_glOpenCLTex);
    glGenFramebuffersEXT(4, m_glFBO);

    m_glSurface = glVDPAURegisterVideoSurfaceNV((GLvoid*)(size_t)m_vdpSurface,
                                                GL_TEXTURE_RECTANGLE_ARB, 4,
                                                m_glVDPAUTex);

    for (int i = 0; i < 4; i++)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_glVDPAUTex[i]);
        glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                        GL_NEAREST);

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_glFBO[i & 2]);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                  GL_COLOR_ATTACHMENT0_EXT + (i & 1),
                                  GL_TEXTURE_RECTANGLE_ARB, m_glVDPAUTex[i], 0);

        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_glOpenCLTex[i]);
        glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                        GL_NEAREST);

        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0,
                     (i < 2) ? GL_RED : GL_RG,
                     (i < 2) ? m_realWidth : m_realWidth / 2,
                     (i < 2) ? m_realHeight : m_realHeight / 2,
                     0, 
                     (i < 2) ? GL_RED : GL_RG,
                     GL_UNSIGNED_BYTE, 
                     zeros);

        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_glFBO[(i & 2) + 1]);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                  GL_COLOR_ATTACHMENT0_EXT + (i & 1),
                                  GL_TEXTURE_RECTANGLE_ARB, m_glOpenCLTex[i],
                                  0);

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        cl_int ciErrNum;
        m_clBuffer[i] =
            clCreateFromGLTexture2D(m_dev->m_context, CL_MEM_READ_ONLY,
                                    GL_TEXTURE_RECTANGLE_ARB, 0,
                                    m_glOpenCLTex[i], &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("VDPAU: OpenCL binding #%1 failed: %2 (%3)")
                .arg(i) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
            return;
        }
    }

    delete [] zeros;
    m_valid = true;
}

VideoSurface::VideoSurface(OpenCLDevice *dev, VideoSurfaceType type,
                           uint32_t width, uint32_t height) :
    m_type(type), m_id(0), m_dev(dev), m_width(width), m_height(height)
{
    int ciErrNum;
    cl_image_format format;
    uint32_t bufWidth;
    uint32_t bufHeight;
    uint32_t temp = 0;

    m_bufCount = 2;
    bufWidth = m_width;
    bufHeight = m_height / 2;

    switch (m_type)
    {
        case kSurfaceWavelet:
        case kSurfaceYUV2:
            format.image_channel_data_type = CL_SNORM_INT8;
            m_bufCount = 4;
            break;
        case kSurfaceLogoROI:
            bufHeight /= 2;
            temp = (m_height / 3) * 4;
            bufWidth = m_width - (temp / 2);
        case kSurfaceYUV:
            format.image_channel_data_type = CL_SNORM_INT8;
            break;
        case kSurfaceLogoRGB:
            bufHeight /= 2;
            temp = (m_height / 3) * 4;
            bufWidth = m_width - (temp / 2);
        case kSurfaceRGB:
            format.image_channel_data_type = CL_UNORM_INT8;
            break;
        default:
            return;
    }

    m_realWidth = bufWidth;
    m_realHeight = bufHeight;

    m_clBuffer = new cl_mem[m_bufCount];
    memset(m_clBuffer, 0, m_bufCount * sizeof(cl_mem));

    format.image_channel_order = CL_RGBA;

    for (int i = 0; i < m_bufCount; i++)
    {
        m_clBuffer[i] = clCreateImage2D(m_dev->m_context, CL_MEM_READ_WRITE,
                                        &format, bufWidth, bufHeight, 0,
                                        NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("VDPAU: OpenCL Image Create #%1 failed: %2 (%3)")
                .arg(i) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
            return;
        }
    }
    m_valid = true;
}

VideoSurface::~VideoSurface()
{
    if (m_clBuffer)
    {
        for (int i = 0; i < m_bufCount; i++)
        {
            if (m_clBuffer[i])
                clReleaseMemObject(m_clBuffer[i]);
        }
        delete [] m_clBuffer;
    }

    if (m_id)
        glVDPAUUnregisterSurfaceNV(m_glSurface);
}

void VideoSurface::Bind(void)
{
    glVDPAUMapSurfacesNV(1, &m_glSurface);

    // Blit from the VDPAU-sourced textures to the OpenCL-reading textures
    for (int i = 0; i < 2; i++)
    {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_glFBO[i*2]);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_glFBO[(i*2)+1]);

        for (int j = 0; j < 2; j++)
        {
            GLenum bufnum = GL_COLOR_ATTACHMENT0_EXT + j;
            uint32_t width  = (i < 1) ? m_realWidth : m_realWidth / 2;
            uint32_t height = (i < 1) ? m_realHeight : m_realHeight / 2; 

            glReadBuffer(bufnum);
            glDrawBuffer(bufnum);
            glBlitFramebufferEXT(0, 0, width, height,
                                 0, 0, width, height,
                                 GL_COLOR_BUFFER_BIT,
                                 GL_NEAREST);
        }
    }

    glVDPAUUnmapSurfacesNV(1, &m_glSurface);
}

void VideoSurface::Dump(QString basename, int framenum)
{
    cl_int ciErrNum;
    QString extension;

    switch (m_type)
    {
        case kSurfaceVDPAURender:
            break;
        case kSurfaceWavelet:
        case kSurfaceYUV2:
            extension = "ppm";
            break;
        case kSurfaceYUV:
        case kSurfaceRGB:
        case kSurfaceLogoROI:
        case kSurfaceLogoRGB:
            extension = "ppm";
            break;
        default:
            return;
    }

    for (int i = 0; i < m_bufCount; i++)
    {
        cl_image_format format;

        ciErrNum = clGetImageInfo(m_clBuffer[i], CL_IMAGE_FORMAT,
                                  sizeof(format), &format, NULL);
        LOG(VB_GENERAL, LOG_INFO,
            QString("Buffer %1: Format - Order %2, Type %3")
            .arg(i) .arg(oclImageFormatString(format.image_channel_order))
            .arg(oclImageFormatString(format.image_channel_data_type)));

        size_t elemSize, pitch, width, height;
        ciErrNum = clGetImageInfo(m_clBuffer[i], CL_IMAGE_ELEMENT_SIZE,
                                  sizeof(elemSize), &elemSize, NULL);
        LOG(VB_GENERAL, LOG_INFO, QString("Element Size: %1") .arg(elemSize));

        ciErrNum = clGetImageInfo(m_clBuffer[i], CL_IMAGE_ROW_PITCH,
                                  sizeof(pitch), &pitch, NULL);
        LOG(VB_GENERAL, LOG_INFO, QString("Row Pitch: %1") .arg(pitch));

        ciErrNum = clGetImageInfo(m_clBuffer[i], CL_IMAGE_WIDTH,
                                  sizeof(width), &width, NULL);
        ciErrNum = clGetImageInfo(m_clBuffer[i], CL_IMAGE_HEIGHT,
                                  sizeof(height), &height, NULL);
        LOG(VB_GENERAL, LOG_INFO, QString("Pixels: %1x%2") .arg(width)
            .arg(height));

        char *buf = new char[pitch*height];
        size_t origin[3] = {0, 0, 0};
        size_t region[3] = {width, height, 1};
        ciErrNum = clEnqueueReadImage(m_dev->m_commandQ, m_clBuffer[i], CL_TRUE,
                                      origin, region, pitch, 0, buf, NULL, 0,
                                      NULL);
        if (m_type == kSurfaceVDPAURender)
        {
            if (elemSize == 1)
                extension = "pgm";
            else
                extension = "bin";
        }
        QString filename = QString("out/%1%2-%3.%4") .arg(basename) .arg(i)
            .arg(framenum) .arg(extension);
        LOG(VB_GENERAL, LOG_INFO, QString("Saving to %1").arg(filename));
        QFile file(filename);
        file.open(QIODevice::WriteOnly);

        if (elemSize == 4)
        {
            QString line = QString("P6\n%1 %2\n255\n").arg(width).arg(height);
            file.write(line.toLocal8Bit().constData());
            for(int j = 0; j < (int)(width * height * elemSize); j += elemSize)
            {
                file.write(&buf[j], 3);
            }
        }
        else
        {
            if (elemSize == 1)
            {
                QString line = QString("P5\n%1 %2\n255\n") .arg(width)
                    .arg(height);
                file.write(line.toLocal8Bit().constData());
            }
            file.write(buf, pitch*height);
        }
        file.close();

        delete [] buf;
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
