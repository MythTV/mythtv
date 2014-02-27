#ifndef FILEWRITERBASE_H
#define FILEWRITERBASE_H

#include <QString>

#include "frame.h"
#include "audiooutputsettings.h"

class MTV_PUBLIC FileWriterBase
{
 public:
                 FileWriterBase();
    virtual     ~FileWriterBase();

    virtual bool Init(void) { return true; }
    virtual bool OpenFile(void) { return true; }
    virtual bool CloseFile(void) { return true; }

    virtual int  WriteVideoFrame(VideoFrame *frame);
    virtual int  WriteAudioFrame(unsigned char *buf, int fnum, long long &timecode);
    virtual int  WriteTextFrame(int vbimode, unsigned char *buf, int len,
                                long long timecode, int pagenr) { return 1; }
    virtual int  WriteSeekTable(void) { return 1; }

    virtual bool SwitchToNextFile(void) { return false; }

    void SetFilename(QString fname)     { m_filename = fname; }
    void SetContainer(QString cont)     { m_container = cont; }
    void SetVideoCodec(QString codec)   { m_videoCodec = codec; }
    void SetVideoBitrate(int bitrate)   { m_videoBitrate = bitrate; }
    void SetWidth(int width)            { m_width = width; }
    void SetHeight(int height)          { m_height = height; }
    void SetAspect(float aspect)        { m_aspect = aspect; }
    void SetFramerate(double rate)      { m_frameRate = rate; }
    void SetKeyFrameDist(int dist)      { m_keyFrameDist = dist; }
    void SetAudioCodec(QString codec)   { m_audioCodec = codec; }
    void SetAudioBitrate(int bitrate)   { m_audioBitrate = bitrate; }
    void SetAudioChannels(int channels) { m_audioChannels = channels; }
    void SetAudioFrameRate(int rate)    { m_audioFrameRate = rate; }
    void SetAudioFormat(AudioFormat f)  { m_audioFormat = f; }
    void SetThreadCount(int count)      { m_encodingThreadCount = count; }
    void SetTimecodeOffset(long long o) { m_startingTimecodeOffset = o; }
    void SetEncodingPreset(QString preset) { m_encodingPreset = preset; }
    void SetEncodingTune(QString tune)  { m_encodingTune = tune; }

    long long GetFramesWritten(void)  const { return m_framesWritten; }
    long long GetTimecodeOffset(void) const { return m_startingTimecodeOffset; }
    /**
     * number of audio samples (per channel) in an AVFrame
     */
    int       GetAudioFrameSize(void) const { return m_audioFrameSize; }

 protected:
    QString     m_filename;
    QString     m_container;
    QString     m_videoCodec;
    int         m_videoBitrate;
    int         m_width;
    int         m_height;
    float       m_aspect;
    double      m_frameRate;
    int         m_keyFrameDist;
    QString     m_audioCodec;
    int         m_audioBitrate;
    int         m_audioChannels;
    int         m_audioFrameRate;
    AudioFormat m_audioFormat;
    int         m_audioFrameSize;
    int         m_encodingThreadCount;
    long long   m_framesWritten;
    long long   m_startingTimecodeOffset;
    QString     m_encodingPreset;
    QString     m_encodingTune;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

