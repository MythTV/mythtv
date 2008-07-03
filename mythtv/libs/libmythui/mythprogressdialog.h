#ifndef MYTHPROGRESSBOX_H_
#define MYTHPROGRESSBOX_H_

#include "mythscreentype.h"
#include "mythmainwindow.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"

class MythUIBusyDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIBusyDialog(const QString &message,
                  MythScreenStack *parent, const char *name);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

    void Close();

  protected:
    QString m_message;

    MythUIText *m_messageText;
};

class MythUIProgressDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythUIProgressDialog(const QString &message,
                  MythScreenStack *parent, const char *name);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

    void SetTotal(uint total);
    void SetCount(uint count);
    void Close();

  protected:
    void UpdateProgress(void);

    QString m_message;
    uint m_total;
    uint m_count;

    MythUIText *m_messageText;
    MythUIText *m_progressText;
    MythUIProgressBar *m_progressBar;
};

#endif
