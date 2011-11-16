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
void OpenCLWavelet(OpenCLDevice *dev, VideoPacket *frame, VideoPacket *wavelet)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet");

    static OpenCLKernelDef kern[2] = {
        { NULL, "videoWavelet",     KERNEL_WAVELET_CL }
    };

    VideoSurface *inSurface  = frame->m_frame;
    VideoSurface *outSurface = wavelet->m_frame; 
    int ciErrNum;

    cl_kernel kernel;

    for (int i = 0; i < 1; i++)
    {
        if (!kern[i].kernel)
        {
            kern[i].kernel = dev->m_kernelMap.value(kern[i].entry, NULL);
            if (!kern[i].kernel)
            {
                if (dev->RegisterKernel(kern[i].entry, kern[i].filename))
                    kern[i].kernel = dev->GetKernel(kern[i].entry);

                if (!kern[i].kernel)
                {
                    return;
                }
            }
        }
    }

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = inSurface->m_width;
    int height  = inSurface->m_height / 2;
    int widthR  = PAD_VALUE(width,  KERNEL_WAVELET_WORKSIZE);
    int heightR = PAD_VALUE(height, KERNEL_WAVELET_WORKSIZE);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { KERNEL_WAVELET_WORKSIZE,
                                 KERNEL_WAVELET_WORKSIZE };

    // for videoWavelet - top field
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &inSurface->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &outSurface->m_clBuffer[0]);
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
                               &inSurface->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &outSurface->m_clBuffer[1]);
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


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
