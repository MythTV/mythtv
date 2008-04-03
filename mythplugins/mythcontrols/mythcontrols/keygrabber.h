// -*- Mode: c++ -*-
#ifndef KEYGRABBER_H_
#define KEYGRABBER_H_

// MythTV headers
#include <mythtv/libmythui/mythdialogbox.h>

/** \class KeyGrabPopupBox
 *  \brief Captures a key.
 *
 */
class KeyGrabPopupBox : public MythDialogBox
{
    Q_OBJECT

  public:
    KeyGrabPopupBox(MythScreenStack *parent);
    ~KeyGrabPopupBox();

    bool keyPressEvent(QKeyEvent *);
    bool Create(void);

  private:
    bool     m_waitingForKeyRelease;
    bool     m_keyReleaseSeen;
    QString  m_capturedKey;
};

#endif // KEYGRABBER_H_
