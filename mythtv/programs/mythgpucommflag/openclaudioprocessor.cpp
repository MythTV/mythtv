#include <QMap>
#include <strings.h>

#include "mythlogging.h"
#include "resultslist.h"
#include "openclinterface.h"
#include "audioprocessor.h"
#include "audiopacket.h"

// Prototypes
unsigned int nextPow2(unsigned int x);
void OpenCLVolumeLevelCleanup(cl_mem **bufs);
FlagResults *OpenCLVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                               int count, int64_t pts, int rate);

AudioProcessorList *openCLAudioProcessorList;

AudioProcessorInit openCLAudioProcessorInit[] = {
    { "Volume Level", OpenCLVolumeLevel },
    { "", NULL }
};

void InitOpenCLAudioProcessors(void)
{
    openCLAudioProcessorList =
        new AudioProcessorList(openCLAudioProcessorInit);
}

unsigned int nextPow2( unsigned int x )
{
    unsigned int y;
    int bits;

    if (x == 0)
        return 0;

    --x;

    for (bits = 1, y = x >> bits; y != 0 && bits <= 32; y = x >> bits)
    {
        x |= y;
        bits <<= 1;
    }

    return ++x;
}


#define KERNEL_VOLUME_CL "audioVolumeLevel.cl"
#define KERNEL_VOLUME_64_CL "audioVolumeLevel64.cl"
void OpenCLVolumeLevelCleanup(cl_mem **bufs)
{
    if (!bufs || !*bufs)
        return;

    cl_mem *memBufs = *bufs;
    *bufs = NULL;

    for (int i = 0; i < 5; i++)
        if (memBufs[i])
            clReleaseMemObject(memBufs[i]);

    delete [] memBufs;
}

FlagResults *OpenCLVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                               int count, int64_t pts, int rate)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Volume Level");

    AudioPacket *audioPacket = audioPacketMap.Lookup(samples);
    if (!audioPacket)
    {
        LOG(VB_GENERAL, LOG_ERR, "packet not in map");
        return NULL;
    }

    int channels = size / count / sizeof(int16_t);
    int maxBlockSize = 64;
    int maxThreadCount = 64;
    int blockSize = count < (maxBlockSize * 2) ?
                    nextPow2((count + 1) / 2) :
                    maxBlockSize;
    int threadCount = MIN(maxThreadCount,
                          (count + (blockSize * 2) - 1) / (blockSize * 2));

    static int oldBlockSize = 0;
    static int oldThreadCount = 0;
    static int oldCount = 0;

    static OpenCLKernelDef kern32[3] = {
        { NULL, "audioVolumeLevelMix",     KERNEL_VOLUME_CL },
        { NULL, "audioVolumeLevelReduce",  KERNEL_VOLUME_CL },
        { NULL, "audioVolumeLevelStats32", KERNEL_VOLUME_CL }
    };
    static OpenCLKernelDef kern64[3] = {
        { NULL, "audioVolumeLevelMix",     KERNEL_VOLUME_CL },
        { NULL, "audioVolumeLevelReduce",  KERNEL_VOLUME_CL },
        { NULL, "audioVolumeLevelStats64", KERNEL_VOLUME_64_CL }
    };
    static OpenCLKernelDef *kern = NULL;
    static cl_long globalResults[4] = { 0, 0, 0, 0 };
    static cl_mem *memBufs = NULL;

    cl_float floatStats[4];
    cl_int   intStats[4];
    cl_int ciErrNum;
    bool init = false;

    void (*cleanup)(cl_mem **bufs) = OpenCLVolumeLevelCleanup;
    cl_kernel kernel;

    if (!kern)
    {
        if (dev->m_float64)
        {
            LOG(VB_GENERAL, LOG_INFO, "Using 64bit float kernels");
            kern = kern64;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Using 32bit float kernels");
            kern = kern32;
        }
    }
    
    for (int i = 0; i < 3; i++)
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
                    return NULL;
                }
            }
            init = true;
        }
    }

    // Make sure previous commands are finished
    clFinish(dev->m_commandQ);

    if (init || !memBufs || oldThreadCount != threadCount || 
        oldCount != count || oldBlockSize != blockSize)
    {
        // Cleanup any old ones (if threadCount changed)
        cleanup(&memBufs);

        oldThreadCount = threadCount;
        oldCount = count;

        // Allocate the buffer structure
        memBufs = new cl_mem[5];
        if (!memBufs)
        {
            LOG(VB_GENERAL, LOG_ERR, "Out of memory allocating OpenCL buffers");
            return NULL;
        }

        memset(memBufs, 0, sizeof(memBufs));

        // Setup the memory buffers
        memBufs[0] = clCreateBuffer(dev->m_context,
                                    CL_MEM_READ_WRITE,
                                    sizeof(cl_long4) * count,
                                    NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating windowData")
                .arg(ciErrNum));
            cleanup(&memBufs);
            return NULL;
        }

        memBufs[1] = clCreateBuffer(dev->m_context,
                                    CL_MEM_READ_WRITE,
                                    sizeof(cl_long4) * threadCount,
                                    NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating reduceData")
                .arg(ciErrNum));
            cleanup(&memBufs);
            return NULL;
        }

        memBufs[2] = clCreateBuffer(dev->m_context,
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(globalResults),
                                    globalResults, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating globalData")
                .arg(ciErrNum));
            cleanup(&memBufs);
            return NULL;
        }

        memBufs[3] = clCreateBuffer(dev->m_context,
                                    CL_MEM_READ_WRITE,
                                    sizeof(intStats),
                                    NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating intStatsData")
                .arg(ciErrNum));
            cleanup(&memBufs);
            return NULL;
        }

        memBufs[4] = clCreateBuffer(dev->m_context,
                                    CL_MEM_READ_WRITE,
                                    sizeof(floatStats),
                                    NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating fltStatsData")
                .arg(ciErrNum));
            cleanup(&memBufs);
            return NULL;
        }

        // Setup the kernel arguments

        // for audioVolumeLevelMix
        kernel = kern[0].kernel->m_kernel;
        ciErrNum  = clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs[0]);

        // for audioVolumeLevelReduce
        kernel = kern[1].kernel->m_kernel;
        ciErrNum |= clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs[0]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs[1]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_long4) * blockSize,
                                   NULL);

        // for audioVolumeLevelStats
        kernel = kern[2].kernel->m_kernel;
        ciErrNum |= clSetKernelArg(kernel, 0, sizeof(cl_mem), &memBufs[1]);
        ciErrNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memBufs[2]);
        ciErrNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &memBufs[3]);
        ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &memBufs[4]);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
            cleanup(&memBufs);
            return NULL;
        }
    }

    // for audioVolumeLevelMix
    kernel = kern[0].kernel->m_kernel;
    ciErrNum  = clSetKernelArg(kernel, 0, sizeof(cl_mem),
                               &audioPacket->m_buffer);

    // for audioVolumeLevelReduce
    kernel = kern[1].kernel->m_kernel;
    ciErrNum |= clSetKernelArg(kernel, 3, sizeof(cl_int), &count);
    ciErrNum |= clSetKernelArg(kernel, 4, sizeof(cl_int), &blockSize);

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error setting kernel arguments");
        cleanup(&memBufs);
        return NULL;
    }

    // Time to run
    size_t workDims[2] = { channels, count };
    kernel = kern[0].kernel->m_kernel;
    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 2, NULL,
                                      workDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 running kernel %2")
            .arg(ciErrNum) .arg(kern[0].entry));
        cleanup(&memBufs);
        return NULL;
    }

    size_t globalWorkDims = blockSize * threadCount;
    size_t localWorkDims = blockSize;
    kernel = kern[1].kernel->m_kernel;
    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &globalWorkDims, &localWorkDims,
                                      0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 running kernel %2")
            .arg(ciErrNum) .arg(kern[1].entry));
        cleanup(&memBufs);
        return NULL;
    }

    globalWorkDims = threadCount;
    kernel = kern[2].kernel->m_kernel;
    ciErrNum = clEnqueueNDRangeKernel(dev->m_commandQ, kernel, 1, NULL,
                                      &globalWorkDims, NULL, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 running kernel %2")
            .arg(ciErrNum) .arg(kern[2].entry));
        cleanup(&memBufs);
        return NULL;
    }

    // Read back the results (finally!)
    ciErrNum  = clEnqueueReadBuffer(dev->m_commandQ, memBufs[2], CL_TRUE,
                                    0, sizeof(globalResults), globalResults,
                                    0, NULL, NULL);
    ciErrNum |= clEnqueueReadBuffer(dev->m_commandQ, memBufs[3], CL_TRUE,
                                    0, sizeof(intStats), intStats,
                                    0, NULL, NULL);
    ciErrNum |= clEnqueueReadBuffer(dev->m_commandQ, memBufs[4], CL_TRUE,
                                    0, sizeof(floatStats), floatStats,
                                    0, NULL, NULL);

    // TODO: get rid of this.
#if 0
    cleanup(&memBufs);
#endif

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 reading results data 3")
            .arg(ciErrNum));
        return NULL;
    }

    float deltaRMSdB = floatStats[0] - floatStats[1];

#if 1
    LOG(VB_GENERAL, LOG_INFO,
        QString("Window RMS: %1 dB, Overall RMS: %2 dB, Delta: %3 dB")
        .arg(floatStats[0]) .arg(floatStats[1]) .arg(deltaRMSdB) +
        QString("  Peak: %1 dB  DNR: %2 dB  (DC: %3) Shift: %4")
        .arg(floatStats[2] + floatStats[0]) .arg(floatStats[2])
        .arg(intStats[2]) .arg(intStats[3]));
#endif

    FlagFindings *finding = NULL;

    if (deltaRMSdB >= 6.0)
        finding = new FlagFindings(kFindingAudioHigh, true);
    else if (deltaRMSdB <= -12.0)
        finding = new FlagFindings(kFindingAudioLow, true);

    if (!finding)
        return NULL;

    FlagFindingsList *findings = new FlagFindingsList();
    findings->append(finding);
    FlagResults *results = new FlagResults(findings);

    return results;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
