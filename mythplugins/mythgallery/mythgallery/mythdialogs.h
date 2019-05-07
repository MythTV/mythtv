#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <QDialog>
#include <QObject>
#include <QFrame>

#include "mythexp.h"
#include "mythmainwindow.h"

class MythMediaDevice;
class MythLineEdit;
class MythRemoteLineEdit;
class MythListBox;
class MythLabel;
struct fontProp;
class QVBoxLayout;
class QProgressBar;
class QAbstractButton;

typedef enum DialogCode
{
    kDialogCodeRejected  = QDialog::Rejected,
    kDialogCodeAccepted  = QDialog::Accepted,
    kDialogCodeListStart = 0x10,
} DialogCode;

inline bool operator==(const DialogCode &a, const QDialog::DialogCode &b)
{ return ((int)a) == ((int)b); }
inline bool operator==(const QDialog::DialogCode &a, const DialogCode &b)
{ return ((int)a) == ((int)b); }
inline bool operator!=(const DialogCode &a, const QDialog::DialogCode &b)
{ return ((int)a) == ((int)b); }
inline bool operator!=(const QDialog::DialogCode &a, const DialogCode &b)
{ return ((int)a) == ((int)b); }

/*!
 * \deprecated Due for removal, use libmythui's MythScreenType instead
 */
class MPUBLIC MythDialog : public QFrame
{
    Q_OBJECT

  public:
    MythDialog(MythMainWindow *parent, const char *name = "MythDialog",
               bool setsize = true);

    // these are for backward compatibility..
    static const DialogCode Rejected  = kDialogCodeRejected;
    static const DialogCode Accepted  = kDialogCodeAccepted;
    static const DialogCode ListStart = kDialogCodeListStart;

    DialogCode result(void) const { return m_rescode; }

     // QFrame::Show is not virtual, so this isn't an override.
    virtual void Show(void);

    void hide(void);

    void setNoErase(void);

    virtual bool onMediaEvent(MythMediaDevice * mediadevice);

    void setResult(DialogCode r);

    // Not an override because the underlying QObject::deleteLater
    // function isn't virtual.  This is a new virtual function, which
    // calls QFrame::deleteLater.
    virtual void deleteLater(void);

    static int CalcItemIndex(DialogCode code);

 signals:
    void menuButtonPressed();
    void leaveModality();

  public slots:
    DialogCode exec(void);
    virtual void done(int); // Must be given a valid DialogCode
    virtual void AcceptItem(int);
    virtual void accept();
    virtual void reject();

  protected:
    ~MythDialog();
    void TeardownAll(void);

    void keyPressEvent(QKeyEvent *e) override; // QWidget

    float          m_dlgwmult     {0.0F};
    float          m_dlghmult     {0.0F};
    int            m_screenwidth  {0};
    int            m_screenheight {0};
    int            m_xbase        {0};
    int            m_ybase        {0};

    MythMainWindow *m_parent      {nullptr};

    DialogCode      m_rescode     {kDialogCodeAccepted};

    bool            m_in_loop     {false};

    QFont m_defaultBigFont, m_defaultMediumFont, m_defaultSmallFont;
};

#endif
