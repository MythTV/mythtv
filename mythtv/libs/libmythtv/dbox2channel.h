/**
 *  Dbox2Channel
 *  Copyright (c) 2005 by Levent Gündogdu
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef DBOX2CHANNEL_H
#define DBOX2CHANNEL_H

#include <qstring.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "tv_rec.h"
#include "channelbase.h"
#include "sitypes.h"
#include "dbox2epg.h"

class DBox2EPG;

typedef struct dbox2channel
{
    pthread_mutex_t lock;
} dbox2_channel_t;

class DBox2Channel : public QObject, public ChannelBase
{
    Q_OBJECT
  public:
    DBox2Channel(TVRec *parent, DBox2DBOptions *dbox2_options, int cardid);
    ~DBox2Channel(void);

    bool SetChannelByString(const QString &chan);
    bool Open();
    bool IsOpen(void) const { return m_recorderAlive; }
    void Close();
    void SwitchToLastChannel();
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; return false; }

    QString GetChannelNameFromNumber(const QString&);
    QString GetChannelNumberFromName(const QString& channelName);
    QString GetChannelID(const QString&);

    void RecorderAlive(bool);

    int GetFd() { return -1; }

  signals:
    void ChannelChanged();
    void ChannelChanging();

  public slots:
    void HttpChannelChangeDone(bool error);
    void HttpRequestDone(bool error);
    void EPGFinished();

  private:
    void Log(QString string);
    void LoadChannels();
    void RequestChannelChange(QString);
    void ScanNextEPG();
    QString GetDefaultChannel();

  private:
    DBox2DBOptions   *m_dbox2options;
    int               m_cardid;
    bool              m_channelListReady;
    QString           m_lastChannel;
    QString           m_requestChannel;
    DBox2EPG         *m_epg;
    bool              m_recorderAlive;

    QHttp            *http;
    QHttp            *httpChanger;

    int               m_dbox2channelcount;
    QMap<int,QString> m_dbox2channelids;
    QMap<int,QString> m_dbox2channelnames;
};

#endif
