/// -*- Mode: c++ -*-

#ifndef _EXTERNAL_CHANNEL_H_
#define _EXTERNAL_CHANNEL_H_

#include <cstdint>
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"
#include "ExternalStreamHandler.h"

class ExternalChannel : public DTVChannel
{
  public:
    ExternalChannel(TVRec *parent, const QString & device)
        : DTVChannel(parent), m_device(device),
          m_loc(ExternalChannel::GetDevice()) {}
    ~ExternalChannel(void);

    // Commands
    bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase

    // ATSC/DVB scanning/tuning stuff
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex&) override // DTVChannel
        { return true; }
    bool Tune(const QString &channum) override; // DTVChannel
    bool Tune(const QString &freqid, int /*finetune*/) override; // DTVChannel

    bool EnterPowerSavingMode(void) override; // DTVChannel

    // Gets
    bool IsOpen(void) const override // ChannelBase
        { return m_stream_handler; }
    QString GetDevice(void) const override // ChannelBase
        { return m_device; }
    bool IsPIDTuningSupported(void) const override // DTVChannel
        { return true; }

    QString UpdateDescription(void);
    QString GetDescription(void);

  protected:
    bool IsExternalChannelChangeSupported(void) override // ChannelBase
        { return true; }

  private:
    QString                  m_device;
    QStringList              m_args;
    ExternalStreamHandler   *m_stream_handler {nullptr};
    QString                  m_loc;
};

#endif // _EXTERNAL_CHANNEL_H_
