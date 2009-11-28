#ifndef LOGVIEWER_H_
#define LOGVIEWER_H_

// qt
#include <QTimer>

// myth
#include <mythscreentype.h>

class MythUIButton;
class MythUIButtonList;
class MythUIText;

void showLogViewer(void);

class LogViewer : public MythScreenType
{
  Q_OBJECT

  public:

    LogViewer(MythScreenStack *parent);
   ~LogViewer(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);

    void setFilenames(const QString &progressLog, const QString &fullLog);

  protected slots:
    void cancelClicked(void);
    void updateClicked(void);
    void updateTimerTimeout(void);
    void toggleAutoUpdate(void);
    bool loadFile(QString filename, QStringList &list, int startline);
    void showProgressLog(void);
    void showFullLog(void);
    void showMenu(void);
    void updateLogItem(MythUIButtonListItem *item);

  private:
    void Init(void);
    QString getSetting(const QString &key);

    bool                m_autoUpdate;
    int                 m_updateTime;
    QTimer             *m_updateTimer;

    QString             m_currentLog;
    QString             m_progressLog;
    QString             m_fullLog;

    MythUIButtonList   *m_logList;
    MythUIText         *m_logText;

    MythUIButton       *m_exitButton;
    MythUIButton       *m_cancelButton;
    MythUIButton       *m_updateButton;
};

#endif
