#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include "qevent.h"

#include "mythscreentype.h"

class MythListButton;

const int kMythDialogBoxCompletionEventType = 34111;

class DialogCompletionEvent : public QCustomEvent
{
  public: 
    DialogCompletionEvent(const QString &id, int result)
        : QCustomEvent(kMythDialogBoxCompletionEventType), 
          m_id(id), m_result(result) { }

    QString GetId() { return m_id; }
    int GetResult() { return m_result; }

  private:
    QString m_id;
    int m_result;
};

// Sends out an event with 'resultid' as the id when done.
class MythDialogBox : public MythScreenType
{
    Q_OBJECT
  public:
    MythDialogBox(const QString &text,
                  MythScreenStack *parent, const char *name);

    void SetReturnEvent(MythScreenType *retscreen, const QString &resultid);

    void AddButton(const QString &title);

    virtual bool keyPressEvent(QKeyEvent *e);

  signals:
    void Selected(int selection);

  private:
    void SendEvent(int res);

    MythListButton *buttonList;
    MythScreenType *m_retScreen;
    QString m_id;
};

#endif
