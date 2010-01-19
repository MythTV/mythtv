#ifndef MYTHPROGRESSBOX_H_
#define MYTHPROGRESSBOX_H_

#include <QEvent>

#include "mythscreentype.h"
#include "mythmainwindow.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"

class MPUBLIC ProgressUpdateEvent : public QEvent
{
  public:
    ProgressUpdateEvent(uint count, uint total=0, QString message="") :
        QEvent(kEventType), m_total(total), m_count(count),
        m_message(message) { }

    QString GetMessage() { return m_message; }
    uint GetTotal() { return m_total; }
    uint GetCount() { return m_count; }

    static Type kEventType;

  private:
    uint m_total;
    uint m_count;
    QString m_message;
};

class MPUBLIC MythUIBusyDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIBusyDialog(const QString &message,
                  MythScreenStack *parent, const char *name);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  protected:
    QString m_message;

    MythUIText *m_messageText;
};

class MPUBLIC MythUIProgressDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIProgressDialog(const QString &message,
                  MythScreenStack *parent, const char *name);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);
    void SetTotal(uint total);
    void SetProgress(uint count);
    void SetMessage(const QString &message);

  protected:
    void UpdateProgress(void);

    QString m_message;
    uint m_total;
    uint m_count;

    MythUIText *m_messageText;
    MythUIText *m_progressText;
    MythUIProgressBar *m_progressBar;
};

MPUBLIC MythUIBusyDialog  *ShowBusyPopup(const QString &message);

#endif
