#ifndef _CUSTOMEVENTRELAYER_H_
#define _CUSTOMEVENTRELAYER_H_

#include "qobject.h"
#include "qstring.h"

#include "mythcontext.h"

/* This is a simple class that relays a QT custom event to an ordinary function
 * pointer.  Useful when you have a relativly small app like mythcommflag that
 * you don't want to wrap inside a class. 
 */

class CustomEventRelayer : public QObject
{
    Q_OBJECT
public:
    CustomEventRelayer(void (*fp_in)(QCustomEvent*)) : fp(fp_in)
    {
        gContext->addListener(this);
    };

    CustomEventRelayer()
    {
        gContext->addListener(this);
    };

    ~CustomEventRelayer()
    {
        gContext->removeListener(this);
    };

    void customEvent(QCustomEvent *e) { fp(e);}
       
private:
    void (*fp)(QCustomEvent*);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
