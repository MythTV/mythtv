/// -*- Mode: c++ -*-

#ifndef ASI_CHANNEL_H
#define ASI_CHANNEL_H

#include <vector>

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"

class ASIChannel : public DTVChannel
{
  public:
    ASIChannel(TVRec *parent, QString device);
    ~ASIChannel(void) override;

    // Commands
    bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase

    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex &/*tuning*/) override // DTVChannel
        { return true; }
    bool Tune(const QString &/*freqid*/, int /*finetune*/) override // ChannelBase
        { return true; }
    bool Tune(uint64_t /*frequency*/) override // DTVChannel
        { return true; }
    // Gets
    bool IsOpen(void) const  override // ChannelBase
        { return m_isOpen; }
    QString GetDevice(void) const override // ChannelBase
        { return m_device; }
    std::vector<DTVTunerType> GetTunerTypes(void) const override // DTVChannel
        { return m_tunerTypes; }
    bool IsPIDTuningSupported(void) const override // DTVChannel
        { return true; }

  private:
    std::vector<DTVTunerType> m_tunerTypes;
    QString                  m_device;
    bool                     m_isOpen {false};
};

#endif // ASI_CHANNEL_H
