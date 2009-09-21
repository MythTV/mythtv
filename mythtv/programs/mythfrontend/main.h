#include <QThread>

class QTimer;
class ConnectToBackend : public QThread
{
    Q_OBJECT

  public:
    ConnectToBackend();
    ~ConnectToBackend();

  public slots:
    void Connect(void);

  private:
    QTimer *timer; // audited ref #5318
};
