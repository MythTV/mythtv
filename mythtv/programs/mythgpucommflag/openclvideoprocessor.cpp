#include <QFile>
#include <QMap>
#include <strings.h>

#include "mythlogging.h"
#include "flagresults.h"
#include "openclinterface.h"
#include "videoprocessor.h"
#include "videopacket.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

// Prototypes
FlagFindings *OpenCLEdgeDetect(OpenCLDevice *dev, AVFrame *frame,
                               AVFrame *wavelet);
FlagFindings *OpenCLSceneChangeDetect(OpenCLDevice *dev, AVFrame *frame,
                                      AVFrame *wavelet);
FlagFindings *OpenCLBlankFrameDetect(OpenCLDevice *dev, AVFrame *frame,
                                     AVFrame *wavelet);
FlagFindings *OpenCLAspectChangeDetect(OpenCLDevice *dev, AVFrame *frame,
                                       AVFrame *wavelet);

VideoProcessorList *openCLVideoProcessorList;

VideoProcessorInit openCLVideoProcessorInit[] = {
    { "Blank Frame Detect", OpenCLBlankFrameDetect },
    { "Aspect Change Detect", OpenCLAspectChangeDetect },
    { "Edge Detect", OpenCLEdgeDetect },
    { "Scene Change Detect", OpenCLSceneChangeDetect },
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
    LOG(VB_GPUVIDEO, LOG_DEBUG,
        QString("Wavelet: totDims: %1x%2, useDims: %3x%4 workDims: %5x%6")
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

void OpenCLHistogram64(OpenCLDevice *dev, VideoSurface *in, VideoHistogram *out)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Histogram64");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoHistogram64",        KERNEL_HISTOGRAM_CL },
        { NULL, "videoHistogramReduce",    KERNEL_HISTOGRAM_CL },
        { NULL, "videoHistogramNormalize", KERNEL_HISTOGRAM_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    size_t bins = out->m_binCount;
    int totDims[2] = { in->m_realWidth, in->m_realHeight };
    uint32_t pixelCount = totDims[0] * totDims[1] * 2;
    int widthR  = PAD_VALUE(totDims[0], dev->m_workGroupSizeX);
    int heightR = PAD_VALUE(totDims[1], dev->m_workGroupSizeY);

    size_t globalWorkDims[2] = { widthR, heightR };
    size_t localWorkDims[2]  = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };
    size_t reduceWorkDims[2] = { widthR  / dev->m_workGroupSizeX,
                                 heightR / dev->m_workGroupSizeY };
    size_t histcount  = bins * reduceWorkDims[0] * reduceWorkDims[1];
    size_t histcount2 = bins * reduceWorkDims[1];

    static OpenCLBufferPtr memBufs = NULL;

    if (!memBufs)
    { 
        memBufs = new OpenCLBuffers(3);
        if (!memBufs)
        {
            LOG(VB_GPU, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return;
        }

        memBufs->m_bufs[0] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_uint) * histcount,
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating histogram internal")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[1] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_uint) * histcount2,
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating histogram column")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[2] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_uint) * bins,
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating histogram column")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }
    }

    VideoSurface binned(dev, kSurfaceRGB, in->m_width, in->m_height);

    // for histogram64
    LOG(VB_GPUVIDEO, LOG_INFO, QString("Histogram64: %1x%2 -> %3x%4")
        .arg(totDims[0]) .arg(totDims[1]) .arg(reduceWorkDims[0])
        .arg(reduceWorkDims[1]));
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &in->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &in->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem),
                               &binned.m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_uint) * bins, NULL);
    ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &memBufs->m_bufs[0]);
    ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_int2), totDims);

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

    // for histogramReduce first run
    globalWorkDims[0] = reduceWorkDims[0];
    globalWorkDims[1] = reduceWorkDims[1];

    localWorkDims[0] = reduceWorkDims[0];
    localWorkDims[1] = 1;

    LOG(VB_GPUVIDEO, LOG_INFO, QString("Histogram64: %1x%2 -> %3x%4")
        .arg(reduceWorkDims[0]) .arg(reduceWorkDims[1]) .arg(1)
        .arg(reduceWorkDims[1]));
    kernel = kern[1].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs->m_bufs[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_uint) * bins, NULL);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &memBufs->m_bufs[1]);

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
            .arg(kern[1].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    // for histogramReduce second run
    globalWorkDims[0] = 1;
    globalWorkDims[1] = reduceWorkDims[1];

    LOG(VB_GPUVIDEO, LOG_INFO, QString("Histogram64: %1x%2 -> %3x%4")
        .arg(1) .arg(reduceWorkDims[1]) .arg(1) .arg(1));

    kernel = kern[1].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs->m_bufs[1]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_uint) * bins, NULL);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &memBufs->m_bufs[2]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      globalWorkDims, globalWorkDims, 0,
                                      NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[1].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("PixelCount: %1").arg(pixelCount));

    kernel = kern[2].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs->m_bufs[2]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_uint), &pixelCount);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &out->m_buf);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &bins, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[2].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    static int dump = 2;
    static int frameNum = 0;

#define DEBUG_HISTOGRAM true
#ifdef DEBUG_HISTOGRAM

    if (1 || dump)
    {
        out->Dump("histogram", frameNum++, DEBUG_HISTOGRAM);

#ifdef DEBUG_VIDEO
        in->Dump("rgb", frameNum, 1);
        binned.Dump("binned", frameNum, 1);
#undef DEBUG_VIDEO
#endif
        dump--;
    }
#undef DEBUG_HISTOGRAM
#endif

    delete memBufs;
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Histogram64 Done");
}

#define KERNEL_CROSS_CORRELATE_CL "videoXCorrelate.cl"

void OpenCLCrossCorrelate(OpenCLDevice *dev, VideoHistogram *prev,
                          VideoHistogram *current, VideoHistogram *correlation)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Cross Correlate");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoCrossCorrelation", KERNEL_CROSS_CORRELATE_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    int bins = prev->m_binCount;

    size_t globalWorkDim = (2 * bins) - 1;

    // for cross-correlate
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &prev->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &current->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &correlation->m_buf);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &globalWorkDim, &globalWorkDim,
                                      0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        return;
    }

#define DEBUG_HISTOGRAM true
#ifdef DEBUG_HISTOGRAM
    static bool dumped = false;
    static int frameNum = 1;

    if (1 || !dumped)
    {
        correlation->Dump("correlate", frameNum++, DEBUG_HISTOGRAM);
        dumped = true;
    }
#undef DEBUG_HISTOGRAM
#endif

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Cross Correlate Done");
}

void OpenCLDiffCorrelation(OpenCLDevice *dev, VideoHistogram *prev,
                           VideoHistogram *current, VideoHistogram *delta)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Diff Correlate");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoDiffCorrelation", KERNEL_CROSS_CORRELATE_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    size_t bins = prev->m_binCount;

    // for cross-correlate
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &prev->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &current->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &delta->m_buf);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &bins, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        return;
    }

#define DEBUG_HISTOGRAM true
#ifdef DEBUG_HISTOGRAM
    static bool dumped = false;
    static int frameNum = 1;

    if (1 || !dumped)
    {
        delta->Dump("delta", frameNum++, DEBUG_HISTOGRAM);
        dumped = true;
    }
#undef DEBUG_HISTOGRAM
#endif

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Diff Correlate Done");
}

void OpenCLThreshDiff0(OpenCLDevice *dev, VideoHistogram *delta, bool *found)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Threshold Diff 0 Start");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoThreshDiff0", KERNEL_CROSS_CORRELATE_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    size_t workSize = 1;

    static OpenCLBufferPtr memBufs = NULL;

    if (!memBufs)
    { 
        memBufs = new OpenCLBuffers(1);
        if (!memBufs)
        {
            LOG(VB_GPU, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return;
        }

        memBufs->m_bufs[0] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_int), NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }
    }

    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &delta->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs->m_bufs[0]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &workSize, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    int resultInt;

    ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs->m_bufs[0],
                                    CL_TRUE, 0, sizeof(cl_int),
                                    &resultInt, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 reading results data")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    *found = (resultInt != 0);

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Threshold Diff 0 Done");
}

void OpenCLBlankFrame(OpenCLDevice *dev, VideoHistogram *hist, bool *found)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Blank Frame Start");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoBlankFrame", KERNEL_HISTOGRAM_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    // Setup the kernel arguments
    size_t workSize = hist->m_binCount;

    static OpenCLBufferPtr memBufs = NULL;

    if (!memBufs)
    { 
        memBufs = new OpenCLBuffers(1);
        if (!memBufs)
        {
            LOG(VB_GPU, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return;
        }

        memBufs->m_bufs[0] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            sizeof(cl_int), NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }
    }

    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &hist->m_buf);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_int) * workSize, NULL);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &memBufs->m_bufs[0]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &workSize, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    int resultInt;

    ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs->m_bufs[0],
                                    CL_TRUE, 0, sizeof(cl_int),
                                    &resultInt, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 reading results data")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    *found = (resultInt != 0);

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Blank Frame Done");
}

#define KERNEL_ASPECT_CL "videoAspect.cl"
void OpenCLVideoAspect(OpenCLDevice *dev, VideoSurface *rgb,
                       uint32_t *topL, uint32_t *botR)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Aspect Start");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoFindPillarBox", KERNEL_ASPECT_CL },
        { NULL, "videoPillarReduceVert", KERNEL_ASPECT_CL },
        { NULL, "videoPillarReduceHoriz", KERNEL_ASPECT_CL },
        { NULL, "videoFindLetterBox", KERNEL_ASPECT_CL },
        { NULL, "videoLetterReduceHoriz", KERNEL_ASPECT_CL },
        { NULL, "videoLetterReduceVert", KERNEL_ASPECT_CL },
        { NULL, "videoCrop", KERNEL_ASPECT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    int totDims[2] = { rgb->m_realWidth, rgb->m_realHeight };
    int PBwidth = (totDims[1] / 3) * 8; // 4:3 in fields
    int LBheight = (PBwidth / 32) * 9;  // 16:9 in fields

    int PBbox[2] = { (totDims[0] - PBwidth) / 2, totDims[1] };
    size_t PBDims[2] = { PAD_VALUE(PBbox[0], dev->m_workGroupSizeX),
                         PAD_VALUE(PBbox[1], dev->m_workGroupSizeY) };
    size_t PBgsizex = PBDims[0] / dev->m_workGroupSizeX;
    size_t PBgsizey = PBDims[1] / dev->m_workGroupSizeY;
    int PBoutsize = PBgsizey * PBDims[0];
    size_t locDims[2] = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };
    size_t cropDims[2] = { PAD_VALUE(totDims[0], dev->m_workGroupSizeX),
                           PAD_VALUE(totDims[1], dev->m_workGroupSizeY) };
    VideoSurface *cropSrc = rgb;
    VideoSurface *midCrop = NULL;
    uint32_t cropWidth  = rgb->m_width;
    uint32_t cropHeight = rgb->m_height;

    // Setup the kernel arguments

    static OpenCLBufferPtr memBufs = NULL;

    if (!memBufs)
    { 
        memBufs = new OpenCLBuffers(9);
        if (!memBufs)
        {
            LOG(VB_GPU, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return;
        }

        memBufs->m_bufs[0] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            PBoutsize * sizeof(cl_uchar), 
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[1] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            PBoutsize * sizeof(cl_uchar), 
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[2] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            PBgsizex * sizeof(cl_ushort), 
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[3] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            PBgsizex * sizeof(cl_ushort), 
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        memBufs->m_bufs[4] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                            2 * sizeof(cl_uint), 
                                            NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
                .arg(ciErrNum));
            delete memBufs;
            return;
        }

        // 5-8 defined below
    }

    topL[0] = 0;
    topL[1] = 0;
    botR[0] = rgb->m_realWidth - 1;
    botR[1] = rgb->m_realHeight - 1;

    if (PBbox[0])
    {
        // videoFindPillarBox
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &rgb->m_clBuffer[0]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &rgb->m_clBuffer[1]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), PBbox);
        ciErrNum |= clSetKernelArg(kernel, 4,
                                   sizeof(cl_uchar2) * locDims[0] * locDims[1],
                                   NULL);
        ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem),
                                   &memBufs->m_bufs[0]);
        ciErrNum |= clSetKernelArg(kernel, 6, sizeof(cl_mem),
                                   &memBufs->m_bufs[1]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            delete memBufs;
            return;
        }

        LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Pillar: %1x%2 - %3x%4")
            .arg(PBDims[0]) .arg(PBDims[1]) .arg(locDims[0]) .arg(locDims[1]));
        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          PBDims, locDims, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[0].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            delete memBufs;
            return;
        }

        int PBbox2[2] = { PBDims[0], PBgsizey };

        // videoPillarReduceVert
        kernel = kern[1].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &memBufs->m_bufs[0]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &memBufs->m_bufs[1]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), PBbox2);
        ciErrNum |= clSetKernelArg(kernel, 4,
                                   sizeof(cl_ushort2) * locDims[0], NULL);
        ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem),
                                   &memBufs->m_bufs[2]);
        ciErrNum |= clSetKernelArg(kernel, 6, sizeof(cl_mem),
                                   &memBufs->m_bufs[3]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            delete memBufs;
            return;
        }

        LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Pillar: %1 - %2")
            .arg(PBDims[0]) .arg(locDims[0]));
        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                          &PBDims[0], &locDims[0],
                                          0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[1].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            delete memBufs;
            return;
        }

        // videoPillarReduceHoriz
        kernel = kern[2].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &memBufs->m_bufs[2]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &memBufs->m_bufs[3]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int), &locDims[0]);
        ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_mem),
                                   &memBufs->m_bufs[4]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            delete memBufs;
            return;
        }

        LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Pillar: %1 - %2")
            .arg(PBgsizex) .arg(PBgsizex));
        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                          &PBgsizex, &PBgsizex, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
                .arg(kern[2].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            delete memBufs;
            return;
        }

        uint32_t PBresults[2];

        ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs->m_bufs[4],
                                        CL_TRUE, 0, sizeof(cl_uint) * 2,
                                        PBresults, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, QString("Error reading results data: %1 (%2)")
                .arg(ciErrNum) .arg(openCLErrorString(ciErrNum)));
            delete memBufs;
            return;
        }

        topL[0] = PBresults[0];
        botR[0] = PBresults[1];

        cropWidth = botR[0] - topL[0] + 1;

        midCrop = new VideoSurface(dev, kSurfaceRGB, cropWidth, cropHeight);

        // videoCrop (remove pillarbox)
        kernel = kern[6].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                                   &rgb->m_clBuffer[0]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                                   &rgb->m_clBuffer[1]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), topL);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), botR);
        ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_mem),
                                   &midCrop->m_clBuffer[0]);
        ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem),
                                   &midCrop->m_clBuffer[1]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
            delete memBufs;
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Crop: %1x%2 -> %3x%4")
            .arg(cropDims[0]) .arg(cropDims[1] * 2) .arg(cropWidth)
            .arg(cropHeight));

        ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                          cropDims, locDims, 0, NULL, NULL);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GPU, LOG_ERR,
                QString("Error running kernel %1: %2 (%3)")
                .arg(kern[6].entry) .arg(ciErrNum)
                .arg(openCLErrorString(ciErrNum)));
            delete memBufs;
            return;
        }

        cropSrc = midCrop;
    }

    // Now to do the letterboxing
    totDims[0] = cropWidth;

    int LBbox[2] = { totDims[0], (totDims[1] - LBheight) / 2 };
    size_t LBDims[2] = { PAD_VALUE(LBbox[0], dev->m_workGroupSizeX),
                         PAD_VALUE(LBbox[1], dev->m_workGroupSizeY) };
    size_t LBgsizex = LBDims[0] / dev->m_workGroupSizeX;
    size_t LBgsizey = LBDims[1] / dev->m_workGroupSizeY;
    int LBoutsize = LBgsizex * LBDims[1];

    memBufs->m_bufs[5] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                        LBoutsize * sizeof(cl_uchar), 
                                        NULL, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    memBufs->m_bufs[6] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                        LBoutsize * sizeof(cl_uchar), 
                                        NULL, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    memBufs->m_bufs[7] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                        LBgsizey * sizeof(cl_ushort), 
                                        NULL, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    memBufs->m_bufs[8] = clCreateBuffer(dev->m_context, CL_MEM_READ_WRITE,
                                        LBgsizey * sizeof(cl_ushort), 
                                        NULL, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error %1 creating result buffer")
            .arg(ciErrNum));
        delete memBufs;
        return;
    }

    // videoFindLetterBox
    kernel = kern[3].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &cropSrc->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem),
                               &cropSrc->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), LBbox);
    ciErrNum |= clSetKernelArg(kernel, 4,
                               sizeof(cl_uchar2) * locDims[0] * locDims[1],
                               NULL);
    ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem), &memBufs->m_bufs[5]);
    ciErrNum |= clSetKernelArg(kernel, 6, sizeof(cl_mem), &memBufs->m_bufs[6]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Letter: %1x%2 - %3x%4")
        .arg(LBDims[0]) .arg(LBDims[1]) .arg(locDims[0]) .arg(locDims[1]));

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      LBDims, locDims, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[3].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    if (midCrop)
        midCrop->DownRef();

    int LBbox2[2] = { LBgsizex, LBDims[1] };

    // videoLetterReduceHoriz
    kernel = kern[4].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs->m_bufs[5]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs->m_bufs[6]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), LBbox2);
    ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_ushort2) * locDims[1],
                               NULL);
    ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem), &memBufs->m_bufs[7]);
    ciErrNum |= clSetKernelArg(kernel, 6, sizeof(cl_mem), &memBufs->m_bufs[8]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Letter: %1 - %2")
        .arg(LBDims[1]) .arg(locDims[1]));

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &LBDims[1], &locDims[1], 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[4].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    // videoLetterReduceVert
    kernel = kern[5].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs->m_bufs[7]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs->m_bufs[8]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), totDims);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int), &locDims[1]);
    ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &memBufs->m_bufs[4]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        delete memBufs;
        return;
    }

    LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Letter: %1 - %2")
        .arg(LBgsizey) .arg(LBgsizey));

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &LBgsizey, &LBgsizey, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error running kernel %1: %2 (%3)")
            .arg(kern[5].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    uint32_t LBresults[2];

    ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs->m_bufs[4],
                                    CL_TRUE, 0, sizeof(cl_uint) * 2,
                                    LBresults, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, QString("Error reading results data: %1 (%2)")
            .arg(ciErrNum) .arg(openCLErrorString(ciErrNum)));
        delete memBufs;
        return;
    }

    topL[1] = LBresults[0];
    botR[1] = LBresults[1];

    LOG(VB_GPUVIDEO, LOG_DEBUG, QString("Aspect:  TopL: %1x%2,  BotR: %3x%4")
        .arg(topL[0]) .arg(topL[1]) .arg(botR[0]) .arg(botR[1]));

    delete memBufs;
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Aspect Done");
}


void OpenCLCrop(OpenCLDevice *dev, VideoSurface *src, VideoSurface *dst,
                VideoAspect *aspect)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Crop Start");

    static OpenCLKernelDef kern[] = {
        { NULL, "videoCrop", KERNEL_ASPECT_CL }
    };
    static int kernCount = sizeof(kern)/sizeof(kern[0]);

    int ciErrNum;

    cl_kernel kernel;

    if (!dev->OpenCLLoadKernels(kern, kernCount, NULL))
        return;

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    size_t totDims[2] = { src->m_realWidth, src->m_realHeight };
    size_t cropDims[2] = { PAD_VALUE(totDims[0], dev->m_workGroupSizeX),
                           PAD_VALUE(totDims[1], dev->m_workGroupSizeY) };
    size_t locDims[2] = { dev->m_workGroupSizeX, dev->m_workGroupSizeY };
    uint32_t topL[2] = { aspect->m_xTL, aspect->m_yTL };
    uint32_t botR[2] = { aspect->m_xBR, aspect->m_yBR };

    LOG(VB_GENERAL, LOG_INFO, QString("Cropping %1x%2 to %3x%4")
        .arg(totDims[0]) .arg(totDims[1] * 2) .arg(aspect->Width())
        .arg(aspect->Height()));

    // videoCrop (remove letterbox)
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &src->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &src->m_clBuffer[1]);
    ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_int2), topL);
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int2), botR);
    ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &dst->m_clBuffer[0]);
    ciErrNum |= clSetKernelArg(kernel, 5, sizeof(cl_mem), &dst->m_clBuffer[1]);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR, "Error setting kernel arguments");
        return;
    }

    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      cropDims, locDims, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GPU, LOG_ERR,
            QString("Error running kernel %1: %2 (%3)")
            .arg(kern[0].entry) .arg(ciErrNum)
            .arg(openCLErrorString(ciErrNum)));
        return;
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Crop Done");
}

// Processors

FlagFindings *OpenCLEdgeDetect(OpenCLDevice *dev, AVFrame *frame,
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

#ifdef DEBUG_VIDEO
    if (videoPacket->m_num <= 100)
    {
        // Convert to RGB to display for debugging
        VideoSurface edgeRGB(dev, kSurfaceLogoRGB, surfWidth, surfHeight);
        OpenCLYUVFromSNORM(dev, logoROI, &edge);
        OpenCLYUVToRGB(dev, &edge, &edgeRGB);
        edgeRGB.Dump("edgeRGB", videoPacket->m_num);
    }
#undef DEBUG_VIDEO
#endif

    LOG(VB_GPUVIDEO, LOG_INFO, "Done OpenCL Edge Detect");
    return NULL;
}


FlagFindings *OpenCLSceneChangeDetect(OpenCLDevice *dev, AVFrame *frame,
                                      AVFrame *wavelet)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Scene Change Detect");

    VideoPacket *videoPacket = videoPacketMap.Lookup(frame);
    if (!videoPacket)
    {
        LOG(VB_GPU, LOG_ERR, "Video packet not in map");
        return NULL;
    }

    static int frameNum = 0;
    FlagFindings *findings = NULL;
    (void)wavelet;

    if (videoPacket->m_prevCorrelation)
    {
        VideoHistogram deltaCorrelation(dev, 127, -63);

        OpenCLDiffCorrelation(dev, videoPacket->m_prevCorrelation,
                              videoPacket->m_correlation, &deltaCorrelation);

        bool found;
        // Threshhold
        OpenCLThreshDiff0(dev, &deltaCorrelation, &found);

        if (found)
        {
#ifdef DEBUG_VIDEO
            videoPacket->m_prevFrameRGB->Dump("rgb", frameNum-1, 1);
            videoPacket->m_croppedRGB->Dump("rgb", frameNum, 1);
#undef DEBUG_VIDEO
#endif

            findings = new FlagFindings(kFindingVideoSceneChange, true);
        }
    }

    frameNum++;

    LOG(VB_GPUVIDEO, LOG_INFO, "Done OpenCL Scene Change Detect");
    return findings;
}


FlagFindings *OpenCLBlankFrameDetect(OpenCLDevice *dev, AVFrame *frame,
                                     AVFrame *wavelet)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Blank Frame Detect");

    VideoPacket *videoPacket = videoPacketMap.Lookup(frame);
    if (!videoPacket)
    {
        LOG(VB_GPU, LOG_ERR, "Video packet not in map");
        return NULL;
    }

    FlagFindings *findings = NULL;
    (void)wavelet;

    bool found;
    // Threshhold
    OpenCLBlankFrame(dev, videoPacket->m_histogram, &found);

    if (found)
    {
        findings = new FlagFindings(kFindingVideoBlankFrame, true);
        videoPacket->m_blank = true;
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "Done OpenCL Blank Frame Detect");
    return findings;
}

FlagFindings *OpenCLAspectChangeDetect(OpenCLDevice *dev, AVFrame *frame,
                                       AVFrame *wavelet)
{
    LOG(VB_GPUVIDEO, LOG_INFO, "OpenCL Aspect Change Detect");

    VideoPacket *videoPacket = videoPacketMap.Lookup(frame);
    if (!videoPacket)
    {
        LOG(VB_GPU, LOG_ERR, "Video packet not in map");
        return NULL;
    }

    FlagFindings *findings = NULL;
    (void)wavelet;

    if (videoPacket->m_blank)
    {
        videoPacket->m_aspect->DownRef();
        videoPacket->m_aspect = videoPacket->m_prevAspect;
        if (videoPacket->m_aspect)
            videoPacket->m_aspect->UpRef();
        return NULL;
    }

    if (videoPacket->m_prevAspect &&
        !videoPacket->m_aspect->Compare(videoPacket->m_prevAspect))
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Aspect changed from %1x%2 (%3) to %4x%5 (%6)")
            .arg(videoPacket->m_prevAspect->Width())
            .arg(videoPacket->m_prevAspect->Height())
            .arg(videoPacket->m_prevAspect->NearestRatio())
            .arg(videoPacket->m_aspect->Width())
            .arg(videoPacket->m_aspect->Height())
            .arg(videoPacket->m_aspect->NearestRatio()));

        findings = new FlagFindings(kFindingVideoAspectChange, true);
    }

    LOG(VB_GPUVIDEO, LOG_INFO, "Done OpenCL Aspect Change Detect");
    return findings;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
