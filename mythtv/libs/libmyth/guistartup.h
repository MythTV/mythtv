#ifndef GUISTARTUP_H_
#define GUISTARTUP_H_

// QT headers
#include <QObject>
#include <QTranslator>
#include <QEventLoop>

// MythDB headers
#include "mythexp.h"

// MythUI headers
#include "mythscreentype.h"

class QEventLoop;
class MythUIButton;
class MythUIText;
class MythScreenStack;
class MythUIStateType;
class MythUIProgressBar;
class MythTimer;

class MPUBLIC GUIStartup : public MythScreenType
{
    Q_OBJECT

  public:

    bool m_Exit;
    bool m_Setup;
    bool m_Retry;

    GUIStartup(MythScreenStack *parent, QEventLoop *eventLoop);
   ~GUIStartup(void);
    bool Create(void);
    bool setStatusState(const QString &name);
    bool setMessageState(const QString &name);
    void showButtons(bool visible);
    void setTotal(int total);
    bool updateProgress(bool finished = false);


  private slots:
    void Retry(void);
    void Setup(void);
    void Close(void);
    void OnClosePromptReturn(bool submit);

  private:

    MythUIButton *m_dummyButton;
    MythUIButton *m_retryButton;
    MythUIButton *m_setupButton;
    MythUIButton *m_exitButton;
    MythUIStateType *m_statusState;
    MythUIStateType *m_messageState;
    MythUIProgressBar *m_progressBar;
    MythTimer *m_progressTimer;
    QEventLoop *m_loop;
    QEventLoop m_dlgLoop;
    int m_total;

};

#endif
