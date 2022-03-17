/** -*- Mode: c++ -*-
 *  CetonStreamHandler
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2009-2011 Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONSTREAMHANDLER_H
#define CETONSTREAMHANDLER_H

// Qt headers
#include <QString>
#include <QMutex>
#include <QTime>
#include <QMap>

// MythTV headers
#include "libmythbase/mythmiscutil.h"
#include "iptvstreamhandler.h"

class CetonStreamHandler;
class CetonChannel;
class QUrlQuery;

class CetonStreamHandler : public IPTVStreamHandler
{
  public:
    static CetonStreamHandler *Get(const QString &devname, int inputid);
    static void Return(CetonStreamHandler * & ref, int inputid);

    bool IsConnected(void) const;
    bool IsCableCardInstalled() const { return m_usingCablecard; };

    // Commands
    bool EnterPowerSavingMode(void);
    bool TuneFrequency(uint frequency, const QString &modulation);
    bool TuneProgram(uint program);
    bool TuneVChannel(const QString &vchannel);

    uint GetProgramNumber(void) const;

  private:
    explicit CetonStreamHandler(const QString &device, int inputid);

    bool Connect(void);

    bool Open(void);
    void Close(void);

    bool VerifyTuning(void);
    void RepeatTuning(void);

    bool TunerOff(void);
    bool PerformTuneVChannel(const QString &vchannel);
    void ClearProgramNumber(void);

    QString GetVar(const QString &section, const QString &variable) const;
    QStringList GetProgramList();
    bool HttpRequest(const QString &method, const QString &script,
                     const QUrlQuery &params,
                     QString &response, uint &status_code) const;


  private:
    QString     m_ipAddress;
    uint        m_card            {0};
    uint        m_tuner           {0};
    bool        m_usingCablecard  {false};
    bool        m_connected       {false};
    bool        m_valid           {false};

    uint        m_lastFrequency   {0};
    QString     m_lastModulation;
    uint        m_lastProgram     {0};
    QString     m_lastVchannel;

    // for implementing Get & Return
    static QMutex                               s_handlersLock;
    static QMap<QString, CetonStreamHandler*>   s_handlers;
    static QMap<QString, uint>                  s_handlersRefCnt;
    static QMap<QString, bool>                  s_infoQueried;
};

#endif // CETONSTREAMHANDLER_H
