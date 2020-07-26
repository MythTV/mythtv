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
