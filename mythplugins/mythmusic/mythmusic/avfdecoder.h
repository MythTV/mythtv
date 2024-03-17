#ifndef AVFECODER_H_
#define AVFECODER_H_

// C++ headers
#include <cstdint>

// QT headers
#include <QObject>

// MythTV headers
#include <libmyth/audio/audiooutputsettings.h>
#include <libmythtv/mythavutil.h>

// Mythmusic Headers
#include "decoder.h"
#include "remoteavformatcontext.h"

class QTimer;

class avfDecoder : public QObject, public Decoder
{
  Q_OBJECT

  public:
    avfDecoder(const QString &file, DecoderFactory *d, AudioOutput *o);
    ~avfDecoder(void) override;

    bool initialize() override; // Decoder
    double lengthInSeconds();
    void seek(double pos) override; // Decoder
    void stop() override; // Decoder

  protected slots:
    void checkMetatdata(void);

  private:
    void run() override; // MThread

    void deinit();

    bool m_inited                         {false};
    bool m_userStop                       {false};
    int  m_stat                           {0};
    uint8_t *m_outputBuffer               {nullptr};

    bool m_finish                         {false};
    long m_freq                           {0};
    long m_bitrate                        {0};
    int m_channels                        {0};
    double m_seekTime                     {-1.0};

    QString m_devicename;

    const AVInputFormat *m_inputFormat    {nullptr};
    RemoteAVFormatContext *m_inputContext {nullptr};
    AVCodecContext *m_audioDec            {nullptr};
    MythCodecMap m_codecMap;

    bool m_inputIsFile                    {false};

    QTimer *m_mdataTimer                  {nullptr};
    QString m_lastMetadata;
    QString m_lastMetadataParsed;

    int m_errCode                         {0};
};

#endif

