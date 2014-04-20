#ifndef _MYTH_FILE_RECORDER_H_
#define _MYTH_FILE_RECORDER_H_

#include <QObject>
#include <QString>
#include <QMutex>
#include <QWaitCondition>

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

class Streamer : public QObject
{
    Q_OBJECT

  public slots:
    void StreamData(void);

  public:
    Streamer(void);
    virtual ~Streamer(void);
    void Open(const QString & filename, bool loopinput, bool poll_mode);
    bool IsOpen(void) const;
    bool Close(void);
    void Send(int sz);
    void Pause(bool val) { m_paused = val; }
    bool IsPaused(void) const { return m_paused; }
    void Stop(void);
    QString ErrorString(void) const { return m_error; }

    void WakeUp(void);

  private:
    QString m_filename;
    QString m_error;
    int     m_fd;

    int     m_pkt_size;
    uint8_t *m_data;
    int     m_bufsize;
    int     m_remainder;
    bool    m_paused;
    bool    m_run;
    bool    m_loop;
    bool    m_poll_mode;

    mutable QMutex streamlock;
    QWaitCondition streamWait;
};

class Commands : public QObject
{
    Q_OBJECT

  public:
    Commands(void);
    virtual ~Commands(void);
    bool Run(const QString & filename, bool loopinput);

  protected:
    bool send_status(const QString & status) const;
    bool process_command(QString & cmd);

  private:
    QString   m_filename;
    Streamer *m_streamer;
    int       m_timeout;
    bool      m_run;
    bool      m_poll_mode;
};

#endif
