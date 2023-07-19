#ifndef MYTHPROGRESSBOX_H_
#define MYTHPROGRESSBOX_H_

#include <utility>

// Qt headers
#include <QEvent>
#include <QMutex>

// MythTV headers
#include "mythscreentype.h"

class MythUIText;
class MythUIProgressBar;

class MUI_PUBLIC ProgressUpdateEvent : public QEvent
{
  public:
    explicit ProgressUpdateEvent(uint count, uint total=0, QString message="") :
        QEvent(kEventType), m_total(total), m_count(count),
        m_message(std::move(message)) { }
    ~ProgressUpdateEvent() override;

    QString GetMessage() { return m_message; }
    uint GetTotal() const { return m_total; }
    uint GetCount() const { return m_count; }

    static const Type kEventType;

  private:
    uint m_total {0};
    uint m_count {0};
    QString m_message;
};

class MUI_PUBLIC MythUIBusyDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIBusyDialog(const QString &message,
                  MythScreenStack *parent, const char *name);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void SetMessage(const QString &message);
    void Reset(void) override; // MythUIType
    void Pulse(void) override; // MythUIType

  protected:
    QString m_origMessage;
    QString m_message;
    bool    m_haveNewMessage  {false};
    QString m_newMessage;
    QMutex  m_newMessageLock;

    MythUIText *m_messageText {nullptr};
};

class MUI_PUBLIC MythUIProgressDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIProgressDialog(QString message,
                  MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name, false), m_message(std::move(message)) {}

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType
    void SetTotal(uint total);
    void SetProgress(uint count);
    void SetMessage(const QString &message);

  protected:
    void UpdateProgress(void);

    QString            m_message;
    uint               m_total        {0};
    uint               m_count        {0};

    MythUIText        *m_messageText  {nullptr};
    MythUIText        *m_progressText {nullptr};
    MythUIProgressBar *m_progressBar  {nullptr};
};

MUI_PUBLIC MythUIBusyDialog  *ShowBusyPopup(const QString &message);

#endif
