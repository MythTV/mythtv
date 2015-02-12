#ifndef _RECORDING_FILE_H_
#define _RECORDING_FILE_H_

#include <QString>
#include <QSize>

#include "mythtvexp.h"
#include "programinfo.h"

class RecordingRule;

typedef enum AVContainerFormats
{
    formatUnknown  = 0,
    formatNUV      = 1,
    formatMPEG2_TS = 2,
    formatMPEG2_PS = 3
} AVContainer;

/** \class RecordingFile
 *  \brief Holds information on a recording file and it's video and audio streams
 *
 *  In constrast to RecordingInfo that contains metadata pertaining to a recorded
 *  program this class stores information on the physical file. In future it may
 *  be that each 'recording' is represented by multiple files, for example as the
 *  result of transcoding for streaming or a multi-part recording created because
 *  a recording was interupted or split by the broadcaster around a news bulletin
 */
class MTV_PUBLIC RecordingFile
{
  public:
    RecordingFile();
   ~RecordingFile();

    bool Load();
    bool Save();

    uint m_recordingId;

    QString m_storageDeviceID; // aka Hostname in old parlance
    QString m_storageGroup;

    uint m_fileId;
    QString m_fileName;
    uint64_t m_fileSize;

    AVContainer m_containerFormat;

    QString m_videoCodec; // ff_codec_id_string
    QSize m_videoResolution;
    double m_videoAspectRatio;
    double m_videoFrameRate;
    //int   m_videoBitrate;

    QString m_audioCodec; // Main audio stream or best quality stream?
    int m_audioChannels;
    double m_audioSampleRate;
    int m_audioBitrate;

    static QString AVContainerToString(AVContainer format);
    static AVContainer AVContainerFromString(const QString &formatStr);
};

#endif
