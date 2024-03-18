#include <QObject>

enum exit_actions : std::uint8_t {
    NO_EXIT  = 0,
    QUIT     = 1,
    HALT     = 2,
    REBOOT   = 3
};

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter(void);
    ~ExitPrompter(void) override;

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
