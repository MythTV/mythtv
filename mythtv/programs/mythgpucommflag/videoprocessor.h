#ifndef _VIDEOPROCESSOR_H
#define _VIDEOPROCESSOR_H

#include <QList>
#include <QString>

#include "openclinterface.h"
#include "resultslist.h"
#include "videopacket.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

typedef FlagResults *(*VideoProcessorFunc)(OpenCLDevice *, AVFrame *frame,
                                           AVFrame *wavelet);

typedef struct {
    QString name;
    VideoProcessorFunc func;
} VideoProcessorInit;

class VideoProcessor
{
  public:
    VideoProcessor(QString name, VideoProcessorFunc func) :
        m_name(name), m_func(func)  {};
    ~VideoProcessor();

    QString m_name;
    VideoProcessorFunc m_func;
};

class VideoProcessorList : public QList<VideoProcessor *>
{
  public:
    VideoProcessorList(VideoProcessorInit initList[]);
    ~VideoProcessorList();
};

extern VideoProcessorList *openCLVideoProcessorList;
extern VideoProcessorList *softwareVideoProcessorList;

void InitVideoProcessors(void);
void InitOpenCLVideoProcessors(void);
void InitSoftwareVideoProcessors(void);

void SoftwareWavelet(AVFrame *frame, AVFrame *wavelet);
void OpenCLCombineYUV(OpenCLDevice *dev, VideoSurface *frame,
                      VideoSurface *yuvframe);
void OpenCLCombineFFMpegYUV(OpenCLDevice *dev, VideoSurface *frame,
                            VideoSurface *yuvframe);
void OpenCLWavelet(OpenCLDevice *dev, VideoSurface *frame,
                   VideoSurface *wavelet);
void OpenCLWaveletInverse(OpenCLDevice *dev, VideoSurface *wavelet,
                          VideoSurface *frame);
void OpenCLYUVToRGB(OpenCLDevice *dev, VideoSurface *yuvframe,
                    VideoSurface *rgbframe);
void OpenCLYUVToSNORM(OpenCLDevice *dev, VideoSurface *inframe,
                      VideoSurface *outframe);
void OpenCLYUVFromSNORM(OpenCLDevice *dev, VideoSurface *inframe,
                        VideoSurface *outframe);
void OpenCLZeroRegion(OpenCLDevice *dev, VideoSurface *inframe,
                      VideoSurface *outframe, int x1, int y1, int x2, int y2);
void OpenCLThreshSat(OpenCLDevice *dev, VideoSurface *inframe,
                     VideoSurface *outframe, int threshold);
void OpenCLCopyLogoROI(OpenCLDevice *dev, VideoSurface *inframe,
                       VideoSurface *outframe, int roiWidth, int roiHeight);
void OpenCLLogoMSE(OpenCLDevice *dev, VideoSurface *ref, VideoSurface *in,
                   VideoSurface *out);
void OpenCLInvert(OpenCLDevice *dev, VideoSurface *in, VideoSurface *out);
void OpenCLMultiply(OpenCLDevice *dev, VideoSurface *ref, VideoSurface *in,
                    VideoSurface *out);
void OpenCLDilate3x3(OpenCLDevice *dev, VideoSurface *in, VideoSurface *out);
void OpenCLHistogram64(OpenCLDevice *dev, VideoSurface *in,
                       VideoHistogram *out);
void OpenCLCrossCorrelate(OpenCLDevice *dev, VideoHistogram *prev,
                          VideoHistogram *current, VideoHistogram *correlation);
void OpenCLDiffCorrelation(OpenCLDevice *dev, VideoHistogram *prev,
                           VideoHistogram *current, VideoHistogram *delta);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
