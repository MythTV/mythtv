#ifndef MYTH_FILE_RECORDER_H
#define MYTH_FILE_RECORDER_H

#include <QObject>
#include <QFile>
#include <QString>

#include <cstdint>
#include <sys/types.h>
#include <unistd.h>

#include "libmythbase/mythdate.h"

class Commands;

class Streamer : public QObject
{
    Q_OBJECT

  public slots:
    void CloseFile(void);
    void SendBytes(void);

  public:
    Streamer(Commands *parent, QString fname, int data_rate,
             bool loopinput);
    ~Streamer(void) override;
    void BlockSize(int val) { m_blockSize = val; }
    bool IsOpen(void) const { return m_file; }
    QString ErrorString(void) const { return m_error; }

  protected:
    void OpenFile(void);

  private:
    Commands   *m_parent    { nullptr };
    QString     m_fileName;
    QFile      *m_file      { nullptr };
    bool        m_loop;

    QString     m_error;

    QByteArray  m_buffer;
    int         m_bufferMax { 188 * 100000 };
    QAtomicInt  m_blockSize { m_bufferMax / 4 };

    // Regulate data rate
    uint        m_dataRate;        // bytes per second
    QDateTime   m_startTime;       // When the first packet was processed
    quint64     m_dataRead  { 0 }; // How many bytes have been sent
};

class Commands : public QObject
{
    Q_OBJECT

  signals:
    void CloseFile(void);
    void SendBytes(void);

  public:
    Commands(void);
    ~Commands(void) override = default;
    bool Run(const QString & filename, int data_rate, bool loopinput);
    void setEoF(void) { m_eof = 1; }

  protected:
    bool send_status(const QString & status) const;
    bool process_command(QString & cmd);

  private:
    QString   m_fileName;
    Streamer *m_streamer { nullptr };
    int       m_timeout  {      10 };
    bool      m_run      {    true };
    QAtomicInt m_eof     {       0 };
};

#endif // MYTH_FILE_RECORDER_H
