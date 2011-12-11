#ifndef _AUDIOPROCESSOR_H
#define _AUDIOPROCESSOR_H

#include <QList>
#include <QString>

#include "openclinterface.h"
#include "flagresults.h"

typedef FlagFindings *(*AudioProcessorFunc)(OpenCLDevice *, int16_t *, int, int,
                                            int64_t, int);

typedef struct {
    QString name;
    AudioProcessorFunc func;
} AudioProcessorInit;

class AudioProcessor
{
  public:
    AudioProcessor(QString name, AudioProcessorFunc func) :
        m_name(name), m_func(func)  {};
    ~AudioProcessor();

    QString m_name;
    AudioProcessorFunc m_func;
};

class AudioProcessorList : public QList<AudioProcessor *>
{
  public:
    AudioProcessorList(AudioProcessorInit initList[]);
    ~AudioProcessorList();
};

extern AudioProcessorList *openCLAudioProcessorList;
extern AudioProcessorList *softwareAudioProcessorList;

void InitAudioProcessors(void);
void InitOpenCLAudioProcessors(void);
void InitSoftwareAudioProcessors(void);

FlagFindings *CommonChannelCount(OpenCLDevice *dev, int16_t *samples, int size,
                                 int count, int64_t pts, int rate);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
