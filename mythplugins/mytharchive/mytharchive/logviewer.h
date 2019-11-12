#ifndef LOGVIEWER_H_
#define LOGVIEWER_H_

// qt
#include <QTimer>

// myth
#include <mythscreentype.h>

const int DEFAULT_UPDATE_TIME = 5;

class MythUIButton;
class MythUIButtonList;
class MythUIText;

void showLogViewer(void);

class LogViewer : public MythScreenType
{
  Q_OBJECT

  public:

    explicit LogViewer(MythScreenStack *parent);
   ~LogViewer(void);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

    void setFilenames(const QString &progressLog, const QString &fullLog);

  protected slots:
    static void cancelClicked(void);
    void updateClicked(void);
    void updateTimerTimeout(void);
    void toggleAutoUpdate(void);
    static bool loadFile(const QString& filename, QStringList &list, int startline);
    void showProgressLog(void);
    void showFullLog(void);
    void ShowMenu(void) override; // MythScreenType
    void updateLogItem(MythUIButtonListItem *item);

  private:
    void Init(void) override; // MythScreenType
    static QString getSetting(const QString &key);

    bool                m_autoUpdate   {false};
    int                 m_updateTime   {DEFAULT_UPDATE_TIME};
    QTimer             *m_updateTimer  {nullptr};

    QString             m_currentLog;
    QString             m_progressLog;
    QString             m_fullLog;

    MythUIButtonList   *m_logList      {nullptr};
    MythUIText         *m_logText      {nullptr};

    MythUIButton       *m_exitButton   {nullptr};
    MythUIButton       *m_cancelButton {nullptr};
    MythUIButton       *m_updateButton {nullptr};
};

#endif
