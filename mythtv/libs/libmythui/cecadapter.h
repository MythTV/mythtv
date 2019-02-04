#ifndef CECADAPTER_H_
#define CECADAPTER_H_

#include <QMutex>
#include <QWaitCondition>
#include "mthread.h"

#define LIBCEC_ENABLED     QString("libCECEnabled")
#define LIBCEC_DEVICE      QString("libCECDevice")
#define LIBCEC_BASE        QString("libCECBase")
#define LIBCEC_PORT        QString("libCECPort")
#define POWEROFFTV_ALLOWED QString("PowerOffTVAllowed")
#define POWEROFFTV_ONEXIT  QString("PowerOffTVOnExit")
#define POWERONTV_ALLOWED  QString("PowerOnTVAllowed")
#define POWERONTV_ONSTART  QString("PowerOnTVOnStart")

class CECAdapterPriv;

class CECAdapter : public QObject, public MThread
{
  Q_OBJECT

  public:
    CECAdapter();
    virtual ~CECAdapter();
    bool IsValid();
    void Action(const QString &action);

  protected:
    void run() override; // MThread

  private:
    CECAdapterPriv        *m_priv {nullptr};
    static QMutex         *gLock;
    static QMutex         *gHandleActionsLock;
    static QWaitCondition *gActionsReady;
};

#endif

