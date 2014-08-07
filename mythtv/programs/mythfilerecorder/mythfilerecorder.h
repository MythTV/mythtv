#ifndef _FILE_RECORDER_H_
#define _FILE_RECORDER_H_

#include <QObject>
#include <QFile>
#include <QString>

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

class Commands;

class Streamer : public QObject
{
    Q_OBJECT

  public slots:
    void CloseFile(void);
    void SendBytes(void);

  public:
    Streamer(Commands *parent, const QString &filename, bool loopinput);
    virtual ~Streamer(void);
    void BlockSize(int val) { m_blockSize = val; }
    bool IsOpen(void) const { return m_file; }
    QString ErrorString(void) const { return m_error; }

  protected:
    void OpenFile(void);

  private:
    Commands *m_parent;
    QString   m_fileName;
    QFile    *m_file;
    bool      m_loop;

    QString m_error;

    QByteArray  m_buffer;
    int     m_bufferMax;
    QAtomicInt  m_blockSize;
};

class Commands : public QObject
{
    Q_OBJECT

  signals:
    void CloseFile(void);
    void SendBytes(void);

  public:
    Commands(void);
    virtual ~Commands(void);
    bool Run(const QString & filename, bool loopinput);
    void setEoF(void) { m_eof = true; }

  protected:
    bool send_status(const QString & status) const;
    bool process_command(QString & cmd);

  private:
    QString   m_fileName;
    Streamer *m_streamer;
    int       m_timeout;
    bool      m_run;
    QAtomicInt m_eof;
};

#endif
