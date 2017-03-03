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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
class QUrl;
#else
class QUrlQuery;
#endif

class CetonStreamHandler : public IPTVStreamHandler
{
  public:
    static CetonStreamHandler *Get(const QString &devicename,
                                   int recorder_id = -1);
    static void Return(CetonStreamHandler * & ref, int recorder_id = -1);

    bool IsConnected(void) const;
    bool IsCableCardInstalled() const { return _using_cablecard; };

    // Commands
    bool EnterPowerSavingMode(void);
    bool TuneFrequency(uint frequency, const QString &modulation);
    bool TuneProgram(uint program);
    bool TuneVChannel(const QString &vchannel);

    uint GetProgramNumber(void) const;

  private:
    explicit CetonStreamHandler(const QString &);

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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                     const QUrl &params,
#else
                     const QUrlQuery &params,
#endif
                     QString &response, uint &status_code) const;


  private:
    QString     _ip_address;
    uint        _card;
    uint        _tuner;
    QString     _device_path;
    bool        _using_cablecard;
    bool        _connected;
    bool               _valid;

    uint        _last_frequency;
    QString     _last_modulation;
    uint        _last_program;
    QString     _last_vchannel;
    QTime       _read_timer;

    // for implementing Get & Return
    static QMutex                               _handlers_lock;
    static QMap<QString, CetonStreamHandler*>   _handlers;
    static QMap<QString, uint>                  _handlers_refcnt;
    static QMap<QString, bool>                  _info_queried;
};

#endif // _CETONSTREAMHANDLER_H_
