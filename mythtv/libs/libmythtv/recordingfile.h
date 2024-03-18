#ifndef RECORDING_FILE_H
#define RECORDING_FILE_H

#include <QString>
#include <QSize>

#include "libmythbase/programinfo.h"
#include "libmythtv/mythtvexp.h"

class RecordingRule;

enum AVContainer : std::uint8_t
{
    formatUnknown  = 0,
    formatNUV      = 1,
    formatMPEG2_TS = 2,
    formatMPEG2_PS = 3
};

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
    RecordingFile() = default;
   ~RecordingFile() = default;

    bool Load();
    bool Save();

    uint        m_recordingId      {0};

    QString     m_storageDeviceID; // aka Hostname in old parlance
    QString     m_storageGroup;

    uint        m_fileId           {0};
    QString     m_fileName;
    uint64_t    m_fileSize         {0};

    AVContainer m_containerFormat  {formatUnknown};

    QString     m_videoCodec; // avcodec_get_name
    QSize       m_videoResolution;
    double      m_videoAspectRatio {0.0};
    double      m_videoFrameRate   {0.0};

    QString     m_audioCodec; // Main audio stream or best quality stream?
    int         m_audioChannels    {0};
    double      m_audioSampleRate  {0.0};
    int         m_audioBitrate     {0};

    static QString AVContainerToString(AVContainer format);
    static AVContainer AVContainerFromString(const QString &formatStr);
};

#endif // RECORDING_FILE_H
