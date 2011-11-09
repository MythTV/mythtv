#ifndef _VIDEOPROCESSOR_H
#define _VIDEOPROCESSOR_H

#include <QList>
#include <QString>

#include "openclinterface.h"
#include "resultslist.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

typedef FlagResults *(*VideoProcessorFunc)(OpenCLDevice *, AVFrame *frame);

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

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
