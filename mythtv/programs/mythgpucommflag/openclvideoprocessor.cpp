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
        { NULL, "videoWavelet", KERNEL_WAVELET_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments, first level
    int totDims[2] = { wavelet->m_width, wavelet->m_height / 2 };
    int useDims[2] = { wavelet->m_width / 2, wavelet->m_height / 4 };
    int widthR  = PAD_VALUE(totDims[0], KERNEL_WAVELET_WORKSIZE);
    int heightR = PAD_VALUE(totDims[1], KERNEL_WAVELET_WORKSIZE);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { KERNEL_WAVELET_WORKSIZE,
                                 KERNEL_WAVELET_WORKSIZE };

    for (int i = 0; i < 2; i++)
    {
        // for videoWavelet - top field, first level
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &frame->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &wavelet->m_clBuffer[i+2]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), totDims);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }

        // for videoWavelet - top field, second level
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &wavelet->m_clBuffer[i+2]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &wavelet->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), useDims);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet Done");
}

void OpenCLWaveletInverse(OpenCLDevice *dev, VideoSurface *wavelet,
                          VideoSurface *frame)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet Inverse");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoWaveletInverse", KERNEL_WAVELET_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments, first level
    int totDims[2] = { wavelet->m_width, wavelet->m_height / 2 };
    int useDims[2] = { wavelet->m_width / 2, wavelet->m_height / 4 };
    int widthR  = PAD_VALUE(totDims[0], KERNEL_WAVELET_WORKSIZE);
    int heightR = PAD_VALUE(totDims[1], KERNEL_WAVELET_WORKSIZE);
    LOG(VB_GENERAL, LOG_INFO,
        QString("totDims: %1x%2, useDims: %3x%4 workDims: %5x%6")
        .arg(totDims[0]) .arg(totDims[1]) .arg(useDims[0]) .arg(useDims[1])
        .arg(widthR) .arg(heightR));

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { KERNEL_WAVELET_WORKSIZE,
                                 KERNEL_WAVELET_WORKSIZE };

    for (int i = 0; i < 2; i++)
    {
        // for videoWavelet - top field, first level
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &wavelet->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &frame->m_clBuffer[i+2]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), useDims);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }

        // for videoWavelet - top field, second level
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &frame->m_clBuffer[i+2]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &frame->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), totDims);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "OpenCL Wavelet Inverse Done");
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

    for (int i = 0; i < 2; i++)
    {
        // for videoCombineYUV
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &frame->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &frame->m_clBuffer[i+2]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                                   &yuvframe->m_clBuffer[i]);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }
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

    for (int i = 0; i < 2; i++)
    {
        // for videoYUVToRGB
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &yuvframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &rgbframe->m_clBuffer[i]);

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
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "OpenCL YUVToRGB Done");
}

void OpenCLYUVToSNORM(OpenCLDevice *dev, VideoSurface *inframe,
                      VideoSurface *outframe)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL YUVToSNORM");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoYUVToSNORM",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = inframe->m_width;
    int height  = inframe->m_height / 2;
    int count   = (inframe->m_bufCount < outframe->m_bufCount) ?
                  inframe->m_bufCount : outframe->m_bufCount;

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &inframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &outframe->m_clBuffer[i]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), $%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(oclErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "OpenCL YUVToSNORM Done");
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
