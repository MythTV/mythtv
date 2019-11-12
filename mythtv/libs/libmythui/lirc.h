#ifndef LIRC_H_
#define LIRC_H_

#include <QByteArray>
#include <QString>
#include <QObject>
#include <QMutex>
#include <QList>

#include <sys/types.h>   // for uint

#include "mthread.h"

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
    virtual ~LIRC();
    void TeardownAll();

    bool IsDoRunSet(void) const;
    void run(void) override; // MThread
    QList<QByteArray> GetCodes(void);
    void Process(const QByteArray &data);

    mutable QMutex  lock;
    static  QMutex  lirclib_lock;
    QObject        *m_mainWindow {nullptr};  ///< window to send key events to
    QString         lircdDevice;   ///< device on which to receive lircd data
    QString         program;       ///< program to extract from config file
    QString         configFile;    ///< file containing LIRC->key mappings
    bool            doRun;
    uint            buf_offset;
    QByteArray      buf;
    uint            eofCount;
    uint            retryCount;
    LIRCPriv       *d {nullptr};
};

#endif
