#include <QObject>

#define NO_EXIT  0
#define QUIT     1
#define HALT     2
#define REBOOT   3

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter() = default;
   ~ExitPrompter() = default;

  public slots:
    static void quit(void);
    static void halt(bool Confirmed = true);
    static void reboot(bool Confirmed = true);
    static void standby(void);
    void handleExit(void);
    void confirmHalt(void);
    void confirmReboot(void);
    void confirm(int Action);
};
