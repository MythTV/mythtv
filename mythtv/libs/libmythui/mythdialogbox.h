#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include <QEvent>
#include <QKeyEvent>

#include "mythscreentype.h"
#include "mythlistbutton.h"

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

    virtual bool keyPressEvent(QKeyEvent *event);

  public slots:
    void Select(MythListButtonItem* item);

  signals:
    void Selected(int selection);

  protected:
    void SendEvent(int res, QString text = "", void *data = 0);

    MythUIText     *m_textarea;
    MythListButton *m_buttonList;
    MythScreenType *m_retScreen;
    QString m_id;

    QString m_text;
};

#endif
