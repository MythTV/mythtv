#ifndef SATIPRECORDER_H
#define SATIPRECORDER_H

// Qt includes
#include <QString>

// MythTV includes
#include "dtvrecorder.h"

class SatIPChannel;
class SatIPStreamHandler;

class SatIPRecorder : public DTVRecorder
{
  public:
    SatIPRecorder(TVRec *rec, SatIPChannel *channel);

    void run(void) override;  // RecorderBase

    bool Open(void);
    bool IsOpen(void) const { return m_streamHandler; }
    void Close(void);
    void StartNewFile(void) override; // RecorderBase

    QString GetSIStandard(void) const override; // DTVRecorder

  private:
    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override;  // RecorderBase

  private:
    SatIPChannel       *m_channel         {nullptr};
    SatIPStreamHandler *m_streamHandler   {nullptr};
    int                 m_inputId         {0};
};

#endif // SATIPRECORDER_H
