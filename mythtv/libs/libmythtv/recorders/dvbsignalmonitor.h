// -*- Mode: c++ -*-

#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

// Qt headers
#include <QStringList>
#include <QCoreApplication>

// MythTV headers
#include "dtvsignalmonitor.h"

class DVBChannel;
class DVBStreamHandler;
class DVBSignalMonitorListener;

class DVBSignalMonitor: public DTVSignalMonitor
{
    Q_DECLARE_TR_FUNCTIONS(DVBSignalMonitor);

  public:
    DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                     bool _release_stream,
                     uint64_t _flags =
                     kSigMon_WaitForSig    | kDVBSigMon_WaitForSNR |
                     kDVBSigMon_WaitForBER | kDVBSigMon_WaitForUB);
    ~DVBSignalMonitor() override;

    QStringList GetStatusList(void) const override; // DTVSignalMonitor
    void Stop(void) override; // SignalMonitor

    void SetRotorTarget(float target) override; // DTVSignalMonitor
    void GetRotorStatus(bool &was_moving, bool &is_moving) override; // DTVSignalMonitor
    void SetRotorValue(int val) override // DTVSignalMonitor
    {
        QMutexLocker locker(&m_statusLock);
        m_rotorPosition.SetValue(val);
    }

    void EmitStatus(void) override; // SignalMonitor

    // MPEG
    void HandlePMT(uint program_num, const ProgramMapTable *pmt) override; // DTVSignalMonitor

    // ATSC Main
    void HandleSTT(const SystemTimeTable *stt) override; // DTVSignalMonitor

    // DVB Main
    void HandleTDT(const TimeDateTable *tdt) override; // DTVSignalMonitor

  protected:
    DVBSignalMonitor(void);
    DVBSignalMonitor(const DVBSignalMonitor&);

    void UpdateValues(void) override; // SignalMonitor
    void EmitDVBSignals(void);

    DVBChannel *GetDVBChannel(void);

  protected:
    SignalMonitorValue m_signalToNoise;
    SignalMonitorValue m_bitErrorRate;
    SignalMonitorValue m_uncorrectedBlocks;
    SignalMonitorValue m_rotorPosition;

    bool               m_streamHandlerStarted;
    DVBStreamHandler  *m_streamHandler;
};

#endif // DVBSIGNALMONITOR_H
