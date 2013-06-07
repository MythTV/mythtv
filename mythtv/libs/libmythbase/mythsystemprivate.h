/// -*- Mode: c++ -*-

#ifndef MYTHSYSTEMPRIVATE_H_
#define MYTHSYSTEMPRIVATE_H_

// C header
#include <stdint.h> // for uint

// Qt header
#include <QPointer> // FIXME: QPointer is deprecated
#include <QObject>

// MythTV header
#include "referencecounter.h"
#include "mythbaseexp.h"
#include "mythsystemlegacy.h"

class QStringList;
class QString;
class QBuffer;

// FIXME: do we really need reference counting?
// it shouldn't be difficult to track the lifetime of a private object.
// FIXME: This should not live in the same header as MythSystemLegacy
class MythSystemLegacyPrivate : public QObject, public ReferenceCounter
{
    Q_OBJECT

  public:
    MythSystemLegacyPrivate(const QString &debugName);

    virtual void Fork(time_t timeout) = 0;
    virtual void Manage(void) = 0;

    virtual void Term(bool force=false) = 0;
    virtual void Signal(int sig) = 0;
    virtual void JumpAbort(void) = 0;

    virtual bool ParseShell(const QString &cmd, QString &abscmd,
                            QStringList &args) = 0;

  protected:
    // FIXME: QPointer uses global hash & is deprecated for good reason
    QPointer<MythSystemLegacy> m_parent;

    uint GetStatus(void)             { return m_parent->GetStatus(); }
    void SetStatus(uint status)      { m_parent->SetStatus(status); }

    // FIXME: We should not return a reference here
    QString& GetLogCmd(void)         { return m_parent->GetLogCmd(); }
    // FIXME: We should not return a reference here
    QString& GetDirectory(void)      { return m_parent->GetDirectory(); }

    bool GetSetting(const char *setting)
        { return m_parent->GetSetting(setting); }

    // FIXME: We should not return a reference here
    QString& GetCommand(void)        { return m_parent->GetCommand(); }
    void SetCommand(QString &cmd)    { m_parent->SetCommand(cmd); }

    // FIXME: We should not return a reference here
    // FIXME: Rename "GetArguments"
    QStringList &GetArgs(void)       { return m_parent->GetArgs(); }
    // FIXME: Rename "SetArguments"
    void SetArgs(QStringList &args)  { m_parent->SetArgs(args); }

    // FIXME: This is likely a bad idea, but possibly manageable
    //        since this is a private class.
    QBuffer *GetBuffer(int index)    { return m_parent->GetBuffer(index); }
    // FIXME: This is likely a bad idea, but possibly manageable
    //        since this is a private class.
    void Unlock(void)                { m_parent->Unlock(); }

  signals:
    void started(void);
    void finished(void);
    void error(uint status);
    void readDataReady(int fd);
};

#endif // MYTHSYSTEMPRIVATE_H_
