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
    ~ExitPrompter();

  public slots:
    void masterPromptExit();
    void handleExit();
    void quit();

  private:
    ExitPrompter(const ExitPrompter &);

  private:
    struct ExitPrompterPrivate *m_d;
};
