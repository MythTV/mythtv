#include <QObject>

#define NO_EXIT  0
#define QUIT     1
#define HALT     2
#define REBOOT   3

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter(void);
    ~ExitPrompter(void);

    void customEvent(QEvent *event) override; // QObject
    
  public slots:
    void masterPromptExit(void);
    void handleExit(void);
    static void quit(void);
    
  private:
    ExitPrompter(const ExitPrompter &);

  private:
    struct ExitPrompterPrivate *m_d {nullptr};
};
