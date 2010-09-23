#ifndef LIRC_H_
#define LIRC_H_

#include <QByteArray>
#include <QString>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QList>

#include <sys/types.h>   // for uint

class LIRCPriv;

/**
 * \class LIRC
 *
 * \brief Listens for input from the Linux Infrared Remote Control daemon and
 *        translates it into internal input events.
 *
 * \ingroup MythUI_Input
 */
class LIRC : public QThread
{
    Q_OBJECT
  public:
    LIRC(QObject *main_window,
         const QString &lircd_device,
         const QString &our_program,
         const QString &config_file,
         const QString &external_app);
    bool Init(void);

    virtual void start(void);
    virtual void deleteLater(void);

  private:
    virtual ~LIRC();
    void TeardownAll();

    bool IsDoRunSet(void) const;
    virtual void run(void);
    void SpawnApp(void);
    QList<QByteArray> GetCodes(void);
    void Process(const QByteArray &data);

    mutable QMutex  lock;
    static  QMutex  lirclib_lock;
    QObject        *m_mainWindow;  ///< window to send key events to
    QString         lircdDevice;   ///< device on which to receive lircd data
    QString         program;       ///< program to extract from config file
    QString         configFile;    ///< file containing LIRC->key mappings
    QString         m_externalApp; ///< external application for keys
    bool            doRun;
    uint            buf_offset;
    QByteArray      buf;
    uint            eofCount;
    uint            retryCount;
    LIRCPriv       *d;
};

#endif
