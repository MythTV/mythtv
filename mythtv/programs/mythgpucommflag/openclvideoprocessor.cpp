#include <QMap>
#include <strings.h>

#include "mythlogging.h"
#include "resultslist.h"
#include "openclinterface.h"
#include "videoprocessor.h"
#include "videopacket.h"

// Prototypes

VideoProcessorList *openCLVideoProcessorList;

VideoProcessorInit openCLVideoProcessorInit[] = {
    { "", NULL }
};

void InitOpenCLVideoProcessors(void)
{
    openCLVideoProcessorList =
        new VideoProcessorList(openCLVideoProcessorInit);
}

#define PAD_VALUE(x,y)  (((x) + (y) - 1) & ~((y) - 1))

#define KERNEL_WAVELET_CL "videoWavelet.cl"
#define KERNEL_WAVELET_WORKSIZE 32
void OpenCLWavelet(OpenCLDevice *dev, VideoSurface *frame,
                   VideoSurface *wavelet)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoWavelet",     KERNEL_WAVELET_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = frame->m_width;
    int height  = frame->m_height / 2;
    int widthR  = PAD_VALUE(width,  KERNEL_WAVELET_WORKSIZE);
    int heightR = PAD_VALUE(height, KERNEL_WAVELET_WORKSIZE);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { KERNEL_WAVELET_WORKSIZE,
                                 KERNEL_WAVELET_WORKSIZE };

    // for videoWavelet - top field
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &frame->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &wavelet->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int), &width);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int), &height);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, localWorkDims,
                                      0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }

    // for videoWavelet - bottom field
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &frame->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &wavelet->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int), &width);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int), &height);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, localWorkDims,
                                      0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet Done");
}

#define KERNEL_CONVERT_CL "videoConvert.cl"
void OpenCLCombineYUV(OpenCLDevice *dev, VideoSurface *frame,
                      VideoSurface *yuvframe)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL CombineYUV");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoCombineYUV",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = frame->m_width;
    int height  = frame->m_height / 2;

    size_t globalWorkDims[2] = { width, height };

    // for videoCombineYUV - top field
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &frame->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &frame->m_clBuffer[2]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[0]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }

    // for videoCombineYUV - bottom field
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &frame->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &frame->m_clBuffer[3]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[1]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }
    LOG(VB_GENERAL, LOG_INFO, "OpenCL CombineYUV Done");
}

void OpenCLYUVToRGB(OpenCLDevice *dev, VideoSurface *yuvframe,
                    VideoSurface *rgbframe)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL YUVToRGB");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoYUVToRGB",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = yuvframe->m_width;
    int height  = yuvframe->m_height / 2;

    size_t globalWorkDims[2] = { width, height };

    // for videoYUVToRGB - top field
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &rgbframe->m_clBuffer[0]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }

    // for videoYUVToRGB - bottom field
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &rgbframe->m_clBuffer[1]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }
    LOG(VB_GENERAL, LOG_INFO, "OpenCL YUVToRGB Done");
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
