#ifndef MYTHOPTICALBUFFER_H
#define MYTHOPTICALBUFFER_H

// MythTV
#include "libmythtv/io/mythmediabuffer.h"

class MythOpticalBuffer : public MythMediaBuffer
{
  public:
    explicit MythOpticalBuffer(MythBufferType Type);

    bool         IsInMenu            (void) const override;
    bool         IsStreamed          (void) override { return true; }
    virtual bool GetNameAndSerialNum (QString& Name, QString& SerialNumber) = 0;

  protected:
    enum MythOpticalState : std::uint8_t
    {
        PROCESS_NORMAL,
        PROCESS_REPROCESS,
        PROCESS_WAIT
    };

    MythOpticalState  m_processState { PROCESS_NORMAL };
    int               m_currentAngle { 0     };
    bool              m_inMenu       { false };
    QString           m_discName;
    QString           m_discSerialNumber;
};

#endif // MYTHOPTICALBUFFER_H
