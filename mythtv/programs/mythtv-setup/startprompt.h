#ifndef SETUPDIALOG_H_
#define SETUPDIALOG_H_

#include <QObject>

class StartPrompter : public QObject
{
    Q_OBJECT

  public:
    StartPrompter();
    StartPrompter(const StartPrompter &) = delete;
    ~StartPrompter() override;

  public slots:
    void handleStart();
    void backendRunningPrompt();
    static void leaveBackendRunning();
    static void stopBackend();
    static void quit();

  private:
    struct StartPrompterPrivate *m_d {nullptr};
};

#endif
