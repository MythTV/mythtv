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
#include "mythscreentype.h"

class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUITextEdit;

class MPUBLIC MythTerminal : public MythScreenType
{
    Q_OBJECT

  public:
    MythTerminal(MythScreenStack *stack, QString program,
                 QStringList arguments);
    virtual void deleteLater(void)
        { TeardownAll(); MythScreenType::deleteLater(); }
    void Init(void) override; // MythScreenType
    bool Create(void) override; // MythScreenType

  public slots:
    void Start(void);
    void Kill(void);
    bool IsDone(void) const;
    void AddText(const QString&);

  protected slots:
    void ProcessHasText(void); // connected to from process' readyRead signal
    void ProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

  protected:
    virtual ~MythTerminal() { TeardownAll(); }
    void TeardownAll(void);

    mutable QMutex         m_lock;
    bool                   m_running;
    QProcess              *m_process;
    QString                m_program;
    QStringList            m_arguments;
    MythUIButtonListItem  *m_currentLine;
    MythUIButtonList      *m_output;
    MythUITextEdit        *m_textEdit;
    MythUIButton          *m_enterButton;
};

#endif // MYTH_TERMINAL_H
