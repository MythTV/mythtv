#ifndef AVFECODER_H_
#define AVFECODER_H_

#include <stdint.h>

#include <QObject>

#include "decoder.h"

#include <audiooutputsettings.h>
#include "remoteavformatcontext.h"

class QTimer;

class avfDecoder : public QObject, public Decoder
{
  Q_OBJECT

  public:
    avfDecoder(const QString &file, DecoderFactory *, AudioOutput *);
    virtual ~avfDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

  protected slots:
    void checkMetatdata(void);

  private:
    void run();

    void deinit();

    bool m_inited, m_userStop;
    int m_stat;
    uint8_t *m_outputBuffer;

    bool m_finish;
    long m_freq, m_bitrate;
    int m_channels;
    double m_seekTime;

    QString m_devicename;

    AVInputFormat *m_inputFormat;
    RemoteAVFormatContext *m_inputContext;
    AVCodecContext *m_audioDec;

    bool m_inputIsFile;

    QTimer *m_mdataTimer;
    QString m_lastMetadata;

    int m_errCode;
};

#endif

