#ifndef CUSTOMEVENTRELAYER_H
#define CUSTOMEVENTRELAYER_H

#include <QObject>

#include "libmythbase/mythcorecontext.h"

/* This is a simple class that relays a QT custom event to an ordinary function
 * pointer.  Useful when you have a relativly small app like mythcommflag that
 * you don't want to wrap inside a class. 
 */

class QEvent;
class CustomEventRelayer : public QObject
{
    Q_OBJECT

  public:
    explicit CustomEventRelayer(void (*fp_in)(QEvent*)) : m_fp(fp_in)
    {
        gCoreContext->addListener(this);
    }

    CustomEventRelayer()
    {
        gCoreContext->addListener(this);
    }

    virtual void deleteLater(void)
    {
        gCoreContext->removeListener(this);
        QObject::deleteLater();
    }

    void customEvent(QEvent *e) override //QObject
        { m_fp(e); }

  protected:
    ~CustomEventRelayer() override = default;

  private:
    void (*m_fp)(QEvent*) {nullptr};
};

#endif //  CUSTOMEVENTRELAYER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
