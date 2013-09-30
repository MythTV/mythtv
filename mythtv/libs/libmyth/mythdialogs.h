#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <QDomElement>
#include <QDialog>
#include <QLabel>
#include <QAbstractButton>
#include <QObject>
#include <QFrame>
#include <QVector>
#include <QList>

#include "mythexp.h"
#include "mythmainwindow.h"
#include <vector>
using namespace std;

class MythMediaDevice;
class MythLineEdit;
class MythRemoteLineEdit;
class MythListBox;
struct fontProp;
class QVBoxLayout;
class QProgressBar;

typedef enum DialogCode
{
    kDialogCodeRejected  = QDialog::Rejected,
    kDialogCodeAccepted  = QDialog::Accepted,
    kDialogCodeListStart = 0x10,
    kDialogCodeButton0   = 0x10,
    kDialogCodeButton1   = 0x11,
    kDialogCodeButton2   = 0x12,
    kDialogCodeButton3   = 0x13,
    kDialogCodeButton4   = 0x14,
    kDialogCodeButton5   = 0x15,
    kDialogCodeButton6   = 0x16,
    kDialogCodeButton7   = 0x17,
    kDialogCodeButton8   = 0x18,
    kDialogCodeButton9   = 0x19,
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

    DialogCode result(void) const { return rescode; }

    virtual void Show(void);

    void hide(void);

    void setNoErase(void);

    virtual bool onMediaEvent(MythMediaDevice * mediadevice);

    void setResult(DialogCode r);

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

    void keyPressEvent(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;
    int xbase, ybase;

    MythMainWindow *m_parent;

    DialogCode rescode;

    bool in_loop;

    QFont defaultBigFont, defaultMediumFont, defaultSmallFont;
};

/*!
 * \deprecated Due for removal, use libmythui's MythScreenType, MythDialogBox
 *             or MythConfirmationDialog instead
 */
class MPUBLIC MythPopupBox : public MythDialog
{
    Q_OBJECT

  public:
    MythPopupBox(MythMainWindow *parent, const char *name = "MythPopupBox") MDEPRECATED;
    MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                 QColor popupForeground, QColor popupBackground,
                 QColor popupHighlight, const char *name = "MythPopupBox") MDEPRECATED;

    void addWidget(QWidget *widget, bool setAppearance = true);
    void addLayout(QLayout *layout, int stretch = 0);

    typedef enum { Large, Medium, Small } LabelSize;

    QLabel *addLabel(QString caption, LabelSize size = Medium,
                     bool wrap = false);

    QAbstractButton *addButton(QString caption, QObject *target = NULL,
                               const char *slot = NULL);

    void ShowPopup(QObject *target = NULL, const char *slot = NULL);
    void ShowPopupAtXY(int destx, int desty,
                       QObject *target = NULL, const char *slot = NULL);

    DialogCode ExecPopup(QObject *target = NULL, const char *slot = NULL);
    DialogCode ExecPopupAtXY(int destx, int desty,
                             QObject *target = NULL, const char *slot = NULL);

    static bool showOkPopup(MythMainWindow *parent,
                            const QString  &title,
                            const QString  &message,
                            QString         button_msg = QString());

    static DialogCode Show2ButtonPopup(
        MythMainWindow *parent,
        const QString &title, const QString &message,
        const QString &button1msg, const QString &button2msg,
        DialogCode default_button);

    static DialogCode ShowButtonPopup(
        MythMainWindow *parent,
        const QString &title, const QString &message,
        const QStringList &buttonmsgs,
        DialogCode default_button);

    static bool showGetTextPopup(MythMainWindow *parent, QString title,
                                 QString message, QString& text);

  public slots:
    virtual void AcceptItem(int);
    virtual void accept(void);
    virtual void reject(void);

  signals:
    void popupDone(int);

  protected:
    ~MythPopupBox() {} // use deleteLater() instead for thread safety
    bool focusNextPrevChild(bool next);
    void keyPressEvent(QKeyEvent *e);

  protected slots:
    void defaultButtonPressedHandler(void);

  private:
    QVBoxLayout *vbox;
    QColor       popupForegroundColor;
    int          hpadding, wpadding;
};

/*!
 *  This dialog is responsible for displaying a progress bar box on
 *  the screen. This is used when you have a known set of steps to
 *  perform and the possibility of calling the \p setProgress call at
 *  the end of each step.
 *
 *  The dialog widget also updates the LCD display if present.
 *
 * \deprecated Due for removal, use libmythui's MythUIProgressDialog instead
 */
class MPUBLIC MythProgressDialog: public MythDialog
{
    Q_OBJECT

  public:
    /** Create a progress bar dialog.

        \param message the title string to appear in the progress dialog.
        \param totalSteps the total number of steps
        \param cancelButton display cancel button
        \param target target for cancel signal
        \param slot slot for cancel signal
        \deprecated Due for removal, use libmythui instead
      */
    MythProgressDialog(const QString& message, int totalSteps = 0,
                       bool cancelButton = false,
                       const QObject * target = NULL,
                       const char * slot = NULL) MDEPRECATED;

    /* \brief Close the dialog.

        This will close the dialog and return the LCD to the Time screen
    */
    void Close(void);
    /* \brief Update the progress bar.

       This will move the progress bar the percentage-completed as
       determined by \p curprogress and the totalsteps as set by the
       call to the constructor.

       The LCD is updated as well.
    */
    void setProgress(int curprogress);

    void setLabel(QString newlabel);

    void keyPressEvent(QKeyEvent *);

    virtual void deleteLater(void);

  signals:
    void pressed();

  protected:
    ~MythProgressDialog(); // use deleteLater() instead for thread safety
    QProgressBar *progress;
    QLabel *msglabel;

  private:
    void setTotalSteps(int totalSteps);
    int steps;
    int m_totalSteps;
};


#endif
