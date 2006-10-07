#ifndef LIRC_H_
#define LIRC_H_

#include <lirc/lirc_client.h>
#include <qobject.h>
#include <qsocket.h>
#include <qstring.h>

#include "mythdialogs.h"

class MPUBLIC LircClient : public QObject
{
    Q_OBJECT
  public:
    LircClient(QObject *main_window);
    ~LircClient();
    int Init(const QString &config_file, const QString &program);
    void Process(void);

  private:
    void SpawnApp(void);

    struct lirc_config *lircConfig;
    QObject *mainWindow;
    pthread_t pth;
    QString external_app;
};

#endif
