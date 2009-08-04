#ifndef _CUSTOMEVENTRELAYER_H_
#define _CUSTOMEVENTRELAYER_H_

#include "qobject.h"
#include "qstring.h"
#include <QEvent>

#include "mythcontext.h"

/* This is a simple class that relays a QT custom event to an ordinary function
 * pointer.  Useful when you have a relativly small app like mythcommflag that
 * you don't want to wrap inside a class. 
 */

class CustomEventRelayer : public QObject
{
    Q_OBJECT

  public:
    CustomEventRelayer(void (*fp_in)(QEvent*)) : fp(fp_in)
    {
        gContext->addListener(this);
    }

    CustomEventRelayer()
    {
        gContext->addListener(this);
    }

    virtual void deleteLater(void)
    {
        gContext->removeListener(this);
        QObject::deleteLater();
    }

    void customEvent(QEvent *e) { fp(e); }

  protected:
    virtual ~CustomEventRelayer() {}

  private:
    void (*fp)(QEvent*);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
