/// -*- Mode: c++ -*-

#ifndef _ASI_CHANNEL_H_
#define _ASI_CHANNEL_H_

#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"

class ASIChannel : public DTVChannel
{
  public:
    ASIChannel(TVRec *parent, QString device);
    ~ASIChannel(void);

    // Commands
    bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase

    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex&) override // DTVChannel
        { return true; }
    bool Tune(const QString&, int) override // ChannelBase
        { return true; }
    bool Tune(uint64_t) override // DTVChannel
        { return true; }
    // Gets
    bool IsOpen(void) const  override // ChannelBase
        { return m_isopen; }
    QString GetDevice(void) const override // ChannelBase
        { return m_device; }
    vector<DTVTunerType> GetTunerTypes(void) const override // DTVChannel
        { return m_tuner_types; }
    bool IsPIDTuningSupported(void) const override // DTVChannel
        { return true; }

  private:
    vector<DTVTunerType>     m_tuner_types;
    QString                  m_device;
    bool                     m_isopen {false};
};

#endif // _ASI_CHANNEL_H_
