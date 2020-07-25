#ifndef MYTHUI_BUTTON_H_
#define MYTHUI_BUTTON_H_

#include <QString>

#include "mythuitype.h"

class MythUIStateType;
class MythUIText;
class MythGestureEvent;

/** \class MythUIButton
 *
 *  \brief A single button widget
 *
 *  Has multiple states with backgrounds and fonts, text and optional
 *  checkbox (dual/tri state)
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIButton : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButton(MythUIType *parent, const QString &name);
   ~MythUIButton() override;

    void Reset(void) override; // MythUIType

    bool gestureEvent(MythGestureEvent *event) override; // MythUIType
    bool keyPressEvent(QKeyEvent *event) override; // MythUIType

    void SetText(const QString &msg);
    QString GetText(void) const;
    QString GetDefaultText(void) const;

    void Push(bool lock=false);

    void SetLockable(bool lockable) { m_lockable = lockable; };
    void SetLocked(bool locked);

  protected slots:
    void Select();
    void Deselect();
    void Enable();
    void Disable();
    void UnPush();

  signals:
    void Clicked();

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void SetInitialStates(void);
    void SetState(const QString& state);

    QString m_message;
    QString m_valueText;

    MythUIStateType *m_backgroundState {nullptr};
    MythUIText *m_text                 {nullptr};

    QString m_state;

    bool m_pushed                      {false};
    bool m_lockable                    {false};
    class QTimer *m_clickTimer         {nullptr};
};

#endif
