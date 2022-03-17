#ifndef FILEWRITERBASE_H
#define FILEWRITERBASE_H

// QT
#include <QString>

// MythTV
#include "libmyth/audio/audiooutputsettings.h"
#include "libmythtv/mythframe.h"

class MTV_PUBLIC MythMediaWriter
{
 public:
                 MythMediaWriter() = default;
    virtual     ~MythMediaWriter() = default;

    virtual bool Init              (void) = 0;
    virtual bool OpenFile          (void) = 0;
    virtual bool CloseFile         (void) = 0;
    virtual int  WriteVideoFrame   (MythVideoFrame *Frame) = 0;
    virtual int  WriteAudioFrame   (unsigned char *Buffer, int FrameNumber,
                                    std::chrono::milliseconds &Timecode) = 0;
    virtual int  WriteTextFrame    (int VBIMode, unsigned char *Buffer, int Length,
                                    std::chrono::milliseconds Timecode, int PageNumber) = 0;
    virtual int  WriteSeekTable    (void) = 0;
    virtual bool SwitchToNextFile  (void) = 0;

    void         SetFilename       (const QString& FileName);
    void         SetContainer      (const QString& Cont);
    void         SetVideoCodec     (const QString& Codec);
    void         SetVideoBitrate   (int Bitrate);
    void         SetWidth          (int Width);
    void         SetHeight         (int Height);
    void         SetAspect         (float Aspect);
    void         SetFramerate      (double Rate);
    void         SetKeyFrameDist   (int Dist);
    void         SetAudioCodec     (const QString& Codec);
    void         SetAudioBitrate   (int Bitrate);
    void         SetAudioChannels  (int Channels);
    void         SetAudioFrameRate (int Rate);
    void         SetAudioFormat    (AudioFormat Format);
    void         SetThreadCount    (int Count);
    void         SetTimecodeOffset (std::chrono::milliseconds Offset);
    void         SetEncodingPreset (const QString& Preset);
    void         SetEncodingTune   (const QString& Tune);
    long long    GetFramesWritten  (void) const;
    std::chrono::milliseconds GetTimecodeOffset (void) const;
    int          GetAudioFrameSize (void) const; // Number of audio samples (per channel) in an AVFrame

 protected:
    QString     m_filename;
    QString     m_container;
    QString     m_videoCodec;
    int         m_videoBitrate           { 800000 };
    int         m_width                  { 0      };
    int         m_height                 { 0      };
    float       m_aspect                 { 1.333333F };
    double      m_frameRate              { 29.97  };
    int         m_keyFrameDist           { 15     };
    QString     m_audioCodec;
    int         m_audioBitrate           { 0      };
    int         m_audioChannels          { 2      };
    int         m_audioFrameRate         { 48000  };
    AudioFormat m_audioFormat            { FORMAT_S16 };
    int         m_audioFrameSize         { -1     };
    int         m_encodingThreadCount    { 1      };
    long long   m_framesWritten          { 0      };
    std::chrono::milliseconds  m_startingTimecodeOffset { -1ms   };
    QString     m_encodingPreset;
    QString     m_encodingTune;
};

#endif

