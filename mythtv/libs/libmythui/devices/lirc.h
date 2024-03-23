#ifndef LIRC_H_
#define LIRC_H_

#include <QByteArray>
#include <QString>
#include <QObject>
#include <QMutex>
#include <QRecursiveMutex>
#include <QList>

#include <sys/types.h>   // for uint

#include "libmythbase/mthread.h"

class LIRCPriv;

/**
 * \class LIRC
 *
 * \brief Listens for input from the Linux Infrared Remote Control daemon and
 *        translates it into internal input events.
 *
 * \ingroup MythUI_Input
 */
class LIRC : public QObject, public MThread
{
    Q_OBJECT
  public:
    LIRC(QObject *main_window,
         QString lircd_device,
         QString our_program,
         QString config_file);
    bool Init(void);

    virtual void start(void);
    virtual void deleteLater(void);

  private:
    ~LIRC() override;
    void TeardownAll();

    bool IsDoRunSet(void) const;
    void run(void) override; // MThread
    QList<QByteArray> GetCodes(void);
    void Process(const QByteArray &data);

    mutable QRecursiveMutex  m_lock;
    static  QMutex  s_lirclibLock;
    QObject        *m_mainWindow  {nullptr}; ///< window to send key events to
    QString         m_lircdDevice;           ///< device on which to receive lircd data
    QString         m_program;               ///< program to extract from config file
    QString         m_configFile;            ///< file containing LIRC->key mappings
    bool            m_doRun       {false};
    uint            m_bufOffset   {0};
    QByteArray      m_buf;
    uint            m_eofCount    {0};
    uint            m_retryCount  {0};
    LIRCPriv       *d             {nullptr}; // NOLINT(readability-identifier-naming)
};

#endif
