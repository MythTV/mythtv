#ifndef LIRC_H_
#define LIRC_H_

#include <lirc/lirc_client.h>
#include <QObject>
#include <QThread>
#include <QString>

#include "mythexp.h"

class MPUBLIC LircThread : public QThread
{
    Q_OBJECT
  public:
    LircThread(QObject *main_window);
    ~LircThread();
    int Init(const QString &config_file, const QString &program,
                bool ignoreExtApp=false);
    void Stop(void) { bStop = true; }

  private:
    void run(void);
    void SpawnApp(void);

    struct lirc_config *lircConfig;
    QObject *mainWindow;
    volatile bool bStop;
    int fd;

    QString external_app;
};

#endif
