#ifndef LIRC_H_
#define LIRC_H_

#include <QThread>
#include <QString>

#include <lirc/lirc_client.h>

#include "mythexp.h"

/** \class LircThread
 *  \brief Interface between mythtv and lircd
 *
 *   Create connection to the lircd daemon and translate remote keypresses
 *   into custom events which are posted to the mainwindow.
 */
class MPUBLIC LircThread : public QThread
{
    Q_OBJECT
  public:
    LircThread(QObject *main_window);
    ~LircThread();
    int Init(const QString &config_file, const QString &program,
                bool ignoreExtApp=false);
    void Stop(void) { m_bStop = true; }

  private:
    void run(void);
    void SpawnApp(void);

    struct lirc_config *m_lircConfig;
    QObject *m_mainWindow;
    volatile bool m_bStop;
    int m_fd;

    QString m_externalApp;
};

#endif
