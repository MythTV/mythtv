#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include <QEvent>

#include "mythscreentype.h"
#include "mythlistbutton.h"
#include "mythuitextedit.h"
#include "mythuibutton.h"

class MythListButtonItem;
class MythListButton;

const int kMythDialogBoxCompletionEventType = 34111;

class DialogCompletionEvent : public QEvent
{
  public:
    DialogCompletionEvent(const QString &id, int result, QString text, void *data)
        : QEvent((QEvent::Type)kMythDialogBoxCompletionEventType),
          m_id(id), m_result(result), m_resultText(text), m_resultData(data) { }

    QString GetId() { return m_id; }
    int GetResult() { return m_result; }
    QString GetResultText() { return m_resultText; }
    void *GetResultData() { return m_resultData; }

  private:
    QString m_id;
    int m_result;
    QString m_resultText;
    void *m_resultData;
};

// Sends out an event with 'resultid' as the id when done.
class MythDialogBox : public MythScreenType
{
    Q_OBJECT
  public:
    MythDialogBox(const QString &text,
                  MythScreenStack *parent, const char *name);

    virtual bool Create(void);

    void SetReturnEvent(MythScreenType *retscreen, const QString &resultid);

    void AddButton(const QString &title, void *data = 0);
    void AddButton(const QString &title, const char *slot);

    virtual bool keyPressEvent(QKeyEvent *event);

  public slots:
    void Select(MythListButtonItem* item);

  signals:
    void Selected();

  protected:
    void SendEvent(int res, QString text = "", void *data = 0);

    MythUIText     *m_textarea;
    MythListButton *m_buttonList;
    MythScreenType *m_retScreen;
    QString m_id;
    bool    m_useSlots;

    QString m_text;
};

class MythConfirmationDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythConfirmationDialog(MythScreenStack *parent, const QString &message,
                           bool showCancel = true);

    bool Create(void);
    void SetReturnEvent(MythScreenType *retscreen, const QString &resultid);

 signals:
     void haveResult(bool);

  private:
    void sendResult(bool);
    QString m_message;
    bool m_showCancel;
    MythScreenType *m_retScreen;
    QString m_id;

  private slots:
    void Confirm(void);
    void Cancel();
};

class MythTextInputDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythTextInputDialog(MythScreenStack *parent, const QString &message,
                        InputFilter filter = FilterNone,
                        bool isPassword = false);

    bool Create(void);
    void SetReturnEvent(MythScreenType *retscreen, const QString &resultid);

 signals:
     void haveResult(QString);

  private:
    MythUITextEdit *m_textEdit;
    QString m_message;
    InputFilter m_filter;
    bool m_isPassword;
    MythScreenType *m_retScreen;
    QString m_id;

  private slots:
    void sendResult();
};

#endif
