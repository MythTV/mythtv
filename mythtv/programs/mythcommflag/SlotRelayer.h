#ifndef SLOTRELAYER_H
#define SLOTRELAYER_H

#include <QObject>
class QString;

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
    explicit SlotRelayer(void (*fp_in)(const QString&)) : m_fpQstring(fp_in) {}
    explicit SlotRelayer(void (*fp_in)()) : m_fpVoid(fp_in) {};

  public slots:
    void relay(const QString& arg) {if (m_fpQstring) m_fpQstring(arg);}
    void relay() {if (m_fpVoid) m_fpVoid();}

  private:
    ~SlotRelayer() override = default;
    void (*m_fpQstring)(const QString&) { nullptr };
    void (*m_fpVoid)()                  { nullptr };
};

#endif // SLOTRELAYER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
