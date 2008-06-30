// -*- Mode: c++ -*-

#ifndef MYTH_TERMINAL_H
#define MYTH_TERMINAL_H

// Qt headers
#include <QObject>
#include <QString>
#include <QProcess>
#include <QMutex>

// MythTV headers
#include "mythexp.h"
#include "settings.h"

class MythTerminalKeyFilter;
class MPUBLIC MythTerminal : public TransListBoxSetting
{
    Q_OBJECT

  public:
    MythTerminal(QString program, QStringList arguments);
    virtual void deleteLater(void)
        { TeardownAll(); TransListBoxSetting::deleteLater(); }

  public slots:
    void Start(void);
    void Kill(void);
    bool IsDone(void) const;
    void AddText(const QString&);

  protected slots:
    void ProcessHasText(void); // connected to from process' readyRead signal
    void ProcessSendKeyPress(QKeyEvent *e);
    void ProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

  protected:
    virtual ~MythTerminal() { TeardownAll(); }
    void TeardownAll(void);

    mutable QMutex         lock;
    bool                   running;
    QProcess              *process;
    QString                program;
    QStringList            arguments;
    QString                curLabel;
    uint                   curValue;
    MythTerminalKeyFilter *filter;
};

class MPUBLIC MythTerminalKeyFilter : public QObject
{
    Q_OBJECT

  signals:
    void KeyPressd(QKeyEvent *e);

  protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // MYTH_TERMINAL_H
