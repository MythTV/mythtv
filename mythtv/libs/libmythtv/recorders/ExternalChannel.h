/// -*- Mode: c++ -*-

#ifndef _EXTERNAL_CHANNEL_H_
#define _EXTERNAL_CHANNEL_H_

#include <vector>
using namespace std;

#include <stdint.h>

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"
#include "ExternalStreamHandler.h"

class ExternalChannel : public DTVChannel
{
  public:
    ExternalChannel(TVRec *parent, const QString & device);
    ~ExternalChannel(void);

    // Commands
    virtual bool Open(void);
    virtual void Close(void);

    // ATSC/DVB scanning/tuning stuff
    using DTVChannel::Tune;
    virtual bool Tune(const DTVMultiplex&) { return true; }
    virtual bool Tune(const QString &channum);
    virtual bool Tune(const QString &freqid, int /*finetune*/);

    virtual bool EnterPowerSavingMode(void);

    // Gets
    virtual bool IsOpen(void) const { return m_stream_handler; }
    virtual QString GetDevice(void) const { return m_device; }
    virtual bool IsPIDTuningSupported(void) const { return true; }

  private:
    QString                  m_device;
    QStringList              m_args;
    ExternalStreamHandler   *m_stream_handler;
};

#endif // _EXTERNAL_CHANNEL_H_
