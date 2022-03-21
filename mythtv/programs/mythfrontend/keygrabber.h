// -*- Mode: c++ -*-
#ifndef KEYGRABBER_H_
#define KEYGRABBER_H_

// MythUI
#include "libmythui/mythscreentype.h"

class MythUIText;
class MythUIButton;

/** \class KeyGrabPopupBox
 *  \brief Captures a key.
 *
 */
class KeyGrabPopupBox : public MythScreenType
{
    Q_OBJECT

  public:
    explicit KeyGrabPopupBox(MythScreenStack *parent)
        : MythScreenType (parent, "keygrabberdialog") {}
    ~KeyGrabPopupBox() override = default;

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    bool Create(void) override; // MythScreenType

  signals:
    void HaveResult(QString);

  private slots:
    void SendResult();

  private:
    bool     m_waitingForKeyRelease {false};
    bool     m_keyReleaseSeen       {false};
    QString  m_capturedKey;

    MythUIText   *m_messageText     {nullptr};
    MythUIButton *m_okButton        {nullptr};
    MythUIButton *m_cancelButton    {nullptr};
};

#endif // KEYGRABBER_H_
