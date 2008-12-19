#include <QObject>

#define NO_EXIT  0
#define QUIT     1
#define HALT     2
#define REBOOT   3

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter();
   ~ExitPrompter() {};

  public slots:
    void fillPrompt(void);
    void handleExit(void);
    void quit(void);
};
