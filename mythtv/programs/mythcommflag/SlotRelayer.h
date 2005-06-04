#ifndef _SLOTRELAYER_H_
#define _SLOTRELAYER_H_

#include "qobject.h"
#include "qstring.h"

/* This is a simple class that relays a QT slot to a function pointer.
 * Useful when you have a relativly small app like mythcommflag that you
 * don't want to wrap inside a QObject enheriting class. 
 *
 * Unfortunattely QT does not allow this class to be templatized for all
 * possible parameter types, so manual labor is in order if you need types other
 * than the ones I made here
 */

class SlotRelayer : public QObject
{
    Q_OBJECT
public:
    SlotRelayer(void (*fp_in)(const QString&)) : fp_qstring(fp_in), fp_void(0) {};
    SlotRelayer(void (*fp_in)()) : fp_qstring(0), fp_void(fp_in) {};
    ~SlotRelayer() {}

public slots:
    void relay(const QString& arg) {if (fp_qstring) fp_qstring(arg);}
    void relay() {if (fp_void) fp_void();}

private:
    void (*fp_qstring)(const QString&);
    void (*fp_void)();
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
