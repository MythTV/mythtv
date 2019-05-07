/** -*- Mode: c++ -*-
 *  CetonStreamHandler
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2009-2011 Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _CETONSTREAMHANDLER_H_
#define _CETONSTREAMHANDLER_H_

// Qt headers
#include <QString>
#include <QMutex>
#include <QTime>
#include <QMap>

// MythTV headers
#include "iptvstreamhandler.h"
#include "mythmiscutil.h"

class CetonStreamHandler;
class CetonChannel;
class QUrlQuery;

class CetonStreamHandler : public IPTVStreamHandler
{
  public:
    static CetonStreamHandler *Get(const QString &devname, int inputid);
    static void Return(CetonStreamHandler * & ref, int inputid);

    bool IsConnected(void) const;
    bool IsCableCardInstalled() const { return m_using_cablecard; };

    // Commands
    bool EnterPowerSavingMode(void);
    bool TuneFrequency(uint frequency, const QString &modulation);
    bool TuneProgram(uint program);
    bool TuneVChannel(const QString &vchannel);

    uint GetProgramNumber(void) const;

  private:
    explicit CetonStreamHandler(const QString &, int inputid);

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
    QString     m_ip_address;
    uint        m_card            {0};
    uint        m_tuner           {0};
    QString     m_device_path;
    bool        m_using_cablecard {false};
    bool        m_connected       {false};
    bool        m_valid           {false};

    uint        m_last_frequency  {0};
    QString     m_last_modulation;
    uint        m_last_program    {0};
    QString     m_last_vchannel;
    QTime       m_read_timer;

    // for implementing Get & Return
    static QMutex                               s_handlers_lock;
    static QMap<QString, CetonStreamHandler*>   s_handlers;
    static QMap<QString, uint>                  s_handlers_refcnt;
    static QMap<QString, bool>                  s_info_queried;
};

#endif // _CETONSTREAMHANDLER_H_
