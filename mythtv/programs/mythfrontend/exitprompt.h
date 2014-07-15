#include <QObject>

#define NO_EXIT  0
#define QUIT     1
#define HALT     2
#define REBOOT   3

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter() {};
   ~ExitPrompter() {};

  public slots:
    void quit(void);
    void halt(void);
    void reboot(void);
    void standby(void);
    void handleExit(void);
};
