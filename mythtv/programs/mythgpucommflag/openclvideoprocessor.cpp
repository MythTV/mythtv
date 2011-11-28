#include <QMap>
#include <strings.h>

#include "mythlogging.h"
#include "resultslist.h"
#include "openclinterface.h"
#include "videoprocessor.h"
#include "videopacket.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

// Prototypes
FlagResults *OpenCLEdgeDetect(OpenCLDevice *dev, AVFrame *frame,
                              AVFrame *wavelet);

VideoProcessorList *openCLVideoProcessorList;

VideoProcessorInit openCLVideoProcessorInit[] = {
    { "Edge Detect", OpenCLEdgeDetect },
    { "", NULL }
};

void InitOpenCLVideoProcessors(void)
{
    openCLVideoProcessorList =
        new VideoProcessorList(openCLVideoProcessorInit);
}

#define PAD_VALUE(x,y)  (((x) + (y) - 1) & ~((y) - 1))

#define KERNEL_WAVELET_CL "videoWavelet.cl"
void OpenCLWavelet(OpenCLDevice *dev, VideoSurface *frame,
                   VideoSurface *wavelet)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Wavelet");

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
    int totDims[2] = { wavelet->m_realWidth, wavelet->m_realHeight };
    int useDims[2] = { wavelet->m_realWidth / 2, wavelet->m_realHeight / 2 };
    int widthR  = PAD_VALUE(totDims[0], dev->m_workGroupSizeX);
    int heightR = PAD_VALUE(totDims[1], dev->m_workGroupSizeY);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };

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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, localWorkDims,
                                          0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, localWorkDims,
                                          0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Wavelet Done");
}

void OpenCLWaveletInverse(OpenCLDevice *dev, VideoSurface *wavelet,
                          VideoSurface *frame)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Wavelet Inverse");

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
    int totDims[2] = { wavelet->m_realWidth, wavelet->m_realHeight };
    int useDims[2] = { wavelet->m_realWidth / 2, wavelet->m_realHeight / 2 };
    int widthR  = PAD_VALUE(totDims[0], dev->m_workGroupSizeX);
    int heightR = PAD_VALUE(totDims[1], dev->m_workGroupSizeY);
    LOG(VB_GPUVIDEO, LOG_INFO,
        QString("totDims: %1x%2, useDims: %3x%4 workDims: %5x%6")
        .arg(totDims[0]) .arg(totDims[1]) .arg(useDims[0]) .arg(useDims[1])
        .arg(widthR) .arg(heightR));

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };

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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, localWorkDims,
                                          0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, localWorkDims,
                                          0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Wavelet Inverse Done");
}

#define KERNEL_CONVERT_CL "videoConvert.cl"
void OpenCLCombineYUV(OpenCLDevice *dev, VideoSurface *frame,
                      VideoSurface *yuvframe)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CombineYUV");

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
    int width   = frame->m_realWidth;
    int height  = frame->m_realHeight;

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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CombineYUV Done");
}

void OpenCLCombineFFMpegYUV(OpenCLDevice *dev, VideoSurface *frame,
                            VideoSurface *yuvframe)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CombineFFMpegYUV");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoCombineFFMpegYUV", KERNEL_CONVERT_CL }
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
    int height  = frame->m_height;

    size_t globalWorkDims[2] = { width, height };

    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &frame->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                               &yuvframe->m_clBuffer[1]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        return;
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CombineFFMpegYUV Done");
}

void OpenCLYUVToRGB(OpenCLDevice *dev, VideoSurface *yuvframe,
                    VideoSurface *rgbframe)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVToRGB");

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
    int width   = MIN(yuvframe->m_realWidth, rgbframe->m_realWidth);
    int height  = MIN(yuvframe->m_realHeight, rgbframe->m_realHeight);

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
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVToRGB Done");
}

void OpenCLYUVToSNORM(OpenCLDevice *dev, VideoSurface *inframe,
                      VideoSurface *outframe)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVToSNORM");

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
    int width   = inframe->m_realWidth;
    int height  = inframe->m_realHeight;
    int count   = MIN(inframe->m_bufCount, outframe->m_bufCount);

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
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVToSNORM Done");
}


void OpenCLYUVFromSNORM(OpenCLDevice *dev, VideoSurface *inframe,
                        VideoSurface *outframe)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVFromSNORM");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoYUVFromSNORM",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = MIN(inframe->m_realWidth, outframe->m_realWidth);
    int height  = MIN(inframe->m_realHeight, outframe->m_realHeight);
    int count   = MIN(inframe->m_bufCount, outframe->m_bufCount);

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
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL YUVFromSNORM Done");
}

void OpenCLZeroRegion(OpenCLDevice *dev, VideoSurface *inframe,
                      VideoSurface *outframe, int x1, int y1, int x2, int y2)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL ZeroRegion");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoZeroRegion", KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments, first level
    int regionStart[2] = { x1, y1 };
    int regionEnd[2] = { x2, y2 };

    size_t globalWorkDims[2] = { inframe->m_realWidth, inframe->m_realHeight };

    for (int i = 0; i < 2; i++)
    {
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &inframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &outframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), regionStart);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), regionEnd);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL ZeroRegion Done");
}


void OpenCLCopyLogoROI(OpenCLDevice *dev, VideoSurface *inframe,
                       VideoSurface *outframe, int roiWidth, int roiHeight)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CopyLogoROI");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoCopyLogoROI",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = 2 * roiWidth;
    int height  = 2 * roiHeight;
    int count   = MIN(inframe->m_bufCount, outframe->m_bufCount);

    int roiCenterTL[2] = { roiWidth, roiHeight };
    int roiCenterBR[2] = { inframe->m_realWidth - roiWidth,
                           inframe->m_realHeight - roiHeight };
    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &inframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &outframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), roiCenterTL);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), roiCenterBR);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL CopyLogoROI Done");
}



void OpenCLThreshSat(OpenCLDevice *dev, VideoSurface *inframe,
                     VideoSurface *outframe, int threshold)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL ThreshSat");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoThreshSaturate",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = inframe->m_realWidth;
    int height  = inframe->m_realHeight;
    int count   = MIN(inframe->m_bufCount, outframe->m_bufCount);

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &inframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &outframe->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int), &threshold);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL ThreshSat Done");
}

void OpenCLLogoMSE(OpenCLDevice *dev, VideoSurface *ref, VideoSurface *in,
                   VideoSurface *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL LogoMSE");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoLogoMSE",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = ref->m_realWidth;
    int height  = ref->m_realHeight;
    int count   = ref->m_bufCount;

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &ref->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &in->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                                   &out->m_clBuffer[i]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL LogoMSE Done");
}

#define KERNEL_INVERT_CL "videoInverse.cl"
void OpenCLInvert(OpenCLDevice *dev, VideoSurface *in, VideoSurface *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Invert");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoInvert",     KERNEL_INVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = in->m_realWidth;
    int height  = in->m_realHeight;
    int count   = in->m_bufCount;

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &in->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &out->m_clBuffer[i]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Invert Done");
}

void OpenCLMultiply(OpenCLDevice *dev, VideoSurface *ref, VideoSurface *in,
                    VideoSurface *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Multiply");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoMultiply",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = ref->m_realWidth;
    int height  = ref->m_realHeight;
    int count   = ref->m_bufCount;

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &ref->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &in->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                                   &out->m_clBuffer[i]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Multiply Done");
}

void OpenCLDilate3x3(OpenCLDevice *dev, VideoSurface *in, VideoSurface *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Dilate3x3");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoDilate3x3",     KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int width   = in->m_realWidth;
    int height  = in->m_realHeight;
    int count   = in->m_bufCount;

    size_t globalWorkDims[2] = { width, height };

    for (int i = 0; i < count; i++)
    {
        // for videoYUVToSNORM
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &in->m_clBuffer[i]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &out->m_clBuffer[i]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error setting kernel arguments, #%1")
                .arg(i));
            return;
        }

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          globalWorkDims, NULL, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3), #%4")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)) .arg(i));
            return;
        }
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Dilate3x3 Done");
}

#define KERNEL_HISTOGRAM_CL "videoHistogram.cl"

void OpenCLHistogram64(OpenCLDevice *dev, VideoSurface *in, uint32_t *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Histogram64");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoHistogram64",     KERNEL_CONVERT_CL },
        { NULL, "videoHistogramReduce", KERNEL_CONVERT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int totDims[2] = { in->m_realWidth, in->m_realHeight };
    int widthR  = PAD_VALUE(totDims[0], dev->m_workGroupSizeX);
    int heightR = PAD_VALUE(totDims[1], dev->m_workGroupSizeY);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };
    size_t reduceWorkDims[2] = { widthR  / dev->m_workGroupSizeX,
                                 heightR / dev->m_workGroupSizeY };
    size_t histcount = 64 * reduceWorkDims[0] * reduceWorkDims[1];

    static OpenCLBufferPtr memBufs = NULL;

    if (!memBufs)
    { 
        memBufs = new OpenCLBuffers(2);
        if (!memBufs)
        {
            LOG(VB_GPU, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return;
        }

        memBufs->m_bufs[0] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_uint4) * histcount,
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating histogram")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[1] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_uint4) * 64,
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating histogram(top)")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }
    }

    // for histogram64
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &in->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &in->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), memBufs->m_bufs[0]);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_uint2), totDims);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, localWorkDims, 0,
                                      NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    // for histogramReduce
    globalWorkDims[0] = (reduceWorkDims[0] + 1) / 2;
    globalWorkDims[1] = (reduceWorkDims[1] + 1) / 2;

    kernel = kern[1].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), memBufs->m_bufs[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), memBufs->m_bufs[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_uint2), reduceWorkDims);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, localWorkDims, 0,
                                      NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    // Read back the results (finally!)
    ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs->m_bufs[1],
                                    CL_TRUE, 0, 64 * sizeof(cl_uint4),
                                    out, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 reading results data")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    delete memBufs;

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Histogram64 Done");
}

// Processors

FlagResults *OpenCLEdgeDetect(OpenCLDevice *dev, AVFrame *frame,
                              AVFrame *wavelet)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Edge Detect");

    VideoPacket *videoPacket = videoPacketMap.Lookup(frame);
    if (!videoPacket)
    {
        LOG(VB_GPU, LOG_ERR, "Video packet not in map");
        return NULL;
    }

    static VideoSurface *logoROI = NULL;

    (void)wavelet;
    int surfWidth = videoPacket->m_wavelet->m_width;
    int surfHeight = videoPacket->m_wavelet->m_height;
    int width = videoPacket->m_wavelet->m_realWidth;
    int height = videoPacket->m_wavelet->m_realHeight;

    // Zero out the approximation section
    VideoSurface edgeWavelet(dev, kSurfaceYUV, surfWidth, surfHeight);
    OpenCLZeroRegion(dev, videoPacket->m_wavelet, &edgeWavelet, 0, 0,
                     width / 4, height / 4);
                    
    // Do an inverse wavelet transform
    VideoSurface edge(dev, kSurfaceYUV2, surfWidth, surfHeight);
    OpenCLWaveletInverse(dev, &edgeWavelet, &edge);
    // edge.Dump("edgeYUV", videoPacket->m_num);

    // If value > threshold, set to 1.0, else 0.0
    OpenCLThreshSat(dev, &edge, &edgeWavelet, 0x08);
    // edgeWavelet.Dump("edgeThresh", videoPacket->m_num);

    // Dilate the thresholded edge with a 3x3 matrix
    OpenCLDilate3x3(dev, &edgeWavelet, &edge);
    // edge.Dump("edgeDilate", videoPacket->m_num);

    // Pull out only the part of frame where we care to find logos
    int width43 = (surfHeight / 3) * 4;
    int widthROI = ((width - width43) / 2) + (width43 / 4);
    int heightROI = height / 4;
    VideoSurface *edgeROI = new VideoSurface(dev, kSurfaceLogoROI, surfWidth,
                                             surfHeight);
    OpenCLCopyLogoROI(dev, &edge, edgeROI, widthROI, heightROI);
    // edgeROI->Dump("edgeROI", videoPacket->m_num);

    if (logoROI)
    {
        // Do a Mean-Squared Errors to correlate this to the accumulated logo
        VideoSurface roiCombined(dev, kSurfaceLogoROI, surfWidth, surfHeight);
        OpenCLLogoMSE(dev, logoROI, edgeROI, &roiCombined);

        // Invert the correlated logo frame
        VideoSurface roiInvert(dev, kSurfaceLogoROI, surfWidth, surfHeight);
        OpenCLInvert(dev, &roiCombined, &roiInvert);

        // Multiply the correlated frame with the current frame
        // so non-correlated areas zero out
        OpenCLMultiply(dev, &roiInvert, logoROI, edgeROI);
        delete logoROI;
    }
    // Store the combined frame for next time
    logoROI = edgeROI;

    if (videoPacket->m_num <= 100)
    {
        // Convert to RGB to display for debugging
        VideoSurface edgeRGB(dev, kSurfaceLogoRGB, surfWidth, surfHeight);
        OpenCLYUVFromSNORM(dev, logoROI, &edge);
        OpenCLYUVToRGB(dev, &edge, &edgeRGB);
        edgeRGB.Dump("edgeRGB", videoPacket->m_num);
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "Done OpenCL Edge Detect");
    return NULL;
}



/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
