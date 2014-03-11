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
    ASIChannel(TVRec *parent, const QString &device);
    ~ASIChannel(void);

    // Commands
    virtual bool Open(void);
    virtual void Close(void);

    using DTVChannel::Tune;
    virtual bool Tune(const DTVMultiplex&, QString) { return true; }
    virtual bool Tune(const QString&, int)          { return true; }
    virtual bool Tune(uint64_t, QString)            { return true; }
    // Gets
    virtual bool IsOpen(void) const { return m_isopen; }
    virtual QString GetDevice(void) const { return m_device; }
    virtual vector<DTVTunerType> GetTunerTypes(void) const
        { return m_tuner_types; }
    virtual bool IsPIDTuningSupported(void) const { return true; }

  private:
    vector<DTVTunerType>     m_tuner_types;
    QString                  m_device;
    bool                     m_isopen;
};

#endif // _ASI_CHANNEL_H_
