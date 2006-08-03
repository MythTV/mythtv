#ifndef LOGVIEWER_H_
#define LOGVIEWER_H_

// qt
#include <qlayout.h>
#include <qhbox.h>
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
    void setFilename(QString filename) { m_filename = filename; }

  protected slots:

    void exitClicked(void);
    void cancelClicked(void);
    void updateClicked(void);
    void updateTimerTimeout(void);
    void updateTimeChanged(int value);
    void toggleAutoUpdate(bool checked);  
    bool loadFile(QString filename, QStringList &list, int startline);

  private:
    QTimer             *updateTimer;
    QString            m_filename;
    MythPushButton     *exitButton;
    MythPushButton     *cancelButton;
    MythPushButton     *updateButton;

    MythSpinBox        *updateTimeSpin;
    MythCheckBox       *autoupdateCheck;
    MythListBox        *listbox;
};

#endif
