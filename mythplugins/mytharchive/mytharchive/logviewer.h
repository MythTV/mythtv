#ifndef LOGVIEWER_H_
#define LOGVIEWER_H_

// qt
#include <qlayout.h>
#include <qvariant.h>

// myth
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>


class LogViewer : public MythDialog
{
    Q_OBJECT
  public:

    LogViewer(MythMainWindow *parent, const char *name = 0);
   ~LogViewer(void);
    void setFilenames(const QString &progressLog, const QString &fullLog);

  protected slots:
    void cancelClicked(void);
    void updateClicked(void);
    void updateTimerTimeout(void);
    void updateTimeChanged(int value);
    void toggleAutoUpdate(bool checked);  
    bool loadFile(QString filename, QStringList &list, int startline);
    void keyPressEvent(QKeyEvent *e);
    void increaseFontSize(void);
    void decreaseFontSize(void);
    void showProgressLog(void);
    void showFullLog(void);
    void showMenu(void);
    void closePopupMenu(void);

  private:
    QString getSetting(const QString &key);

    int                 m_updateTime;
    QTimer             *m_updateTimer;

    QString             m_currentLog;
    QString             m_progressLog;
    QString             m_fullLog;

    MythPushButton     *m_exitButton;
    MythPushButton     *m_cancelButton;
    MythPushButton     *m_updateButton;

    MythSpinBox        *m_updateTimeSpin;
    MythCheckBox       *m_autoupdateCheck;
    MythListBox        *m_listbox;

    MythPopupBox       *m_popupMenu;
};

#endif
