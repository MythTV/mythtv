#ifndef MYTHFRONTEND_BACKENDCONNECTIONMANAGER_H
#define MYTHFRONTEND_BACKENDCONNECTIONMANAGER_H

#include <QObject>

class Reconnect;
class QTimer;
class QEvent;

class BackendConnectionManager : public QObject
{
    Q_OBJECT

  public:
    BackendConnectionManager();
    ~BackendConnectionManager() override;

    void customEvent(QEvent *event) override; // QObject

  protected slots:
    void ReconnectToBackend(void);

  protected:
    Reconnect *m_reconnecting    {nullptr};
    QTimer    *m_reconnectTimer  {nullptr};
    bool       m_reconnectAgain  {false};
};

#endif // MYTHFRONTEND_BACKENDCONNECTIONMANAGER_H
