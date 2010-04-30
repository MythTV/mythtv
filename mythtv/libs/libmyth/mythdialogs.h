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

#include <vector>
using namespace std;

class XMLParse;
class UIType;
class UIManagedTreeListType;
class UITextType;
class UIPushButtonType;
class UITextButtonType;
class UIRemoteEditType;
class UIRepeatedImageType;
class UICheckBoxType;
class UISelectorType;
class UIBlackHoleType;
class UIImageType;
class UIImageGridType;
class UIStatusBarType;
class UIListBtnType;
class UIListTreeType;
class UIKeyboardType;
class LayerSet;
class GenericTree;
class MythMediaDevice;
class MythLineEdit;
class MythRemoteLineEdit;
class MythListBox;
struct fontProp;
class MythMainWindow;
class QVBoxLayout;
class QProgressBar;

#include "mythmainwindow.h"

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

class MPUBLIC MythPopupBox : public MythDialog
{
    Q_OBJECT

  public:
    MythPopupBox(MythMainWindow *parent, const char *name = "MythPopupBox");
    MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                 QColor popupForeground, QColor popupBackground,
                 QColor popupHighlight, const char *name = "MythPopupBox");

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
                            QString         button_msg = QString::null);

    static bool showOkCancelPopup(MythMainWindow *parent, QString title,
                                  QString message, bool focusOk);

    static DialogCode Show2ButtonPopup(
        MythMainWindow *parent,
        const QString &title, const QString &message,
        const QString &button1msg, const QString &button2msg,
        DialogCode default_button)
    {
        QStringList buttonmsgs;
        buttonmsgs += (button1msg.isEmpty()) ?
            QString("Button 1") : button1msg;
        buttonmsgs += (button2msg.isEmpty()) ?
            QString("Button 2") : button2msg;
        return ShowButtonPopup(
            parent, title, message, buttonmsgs, default_button);
    }

    static DialogCode ShowButtonPopup(
        MythMainWindow *parent,
        const QString &title, const QString &message,
        const QStringList &buttonmsgs,
        DialogCode default_button);

    static bool showGetTextPopup(MythMainWindow *parent, QString title,
                                 QString message, QString& text);
    static QString showPasswordPopup(MythMainWindow *parent,
                                     QString title, QString message);

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
    bool         arrowAccel;
};

/** The MythTV progress bar dialog.

    This dialog is responsible for displaying a progress bar box on
    the screen. This is used when you have a known set of steps to
    perform and the possibility of calling the \p setProgress call at
    the end of each step.

	If you do not know the number of steps, use \p MythBusyDialog
	instead.

    The dialog widget also updates the LCD display if present.

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
      */
    MythProgressDialog(const QString& message, int totalSteps = 0,
                       bool cancelButton = false,
                       const QObject * target = NULL,
                       const char * slot = NULL);

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

/** MythDialog box that displays a busy spinner-style dialog box to
    indicate the program is busy, but that the number of steps needed
    is unknown.

    Ie. used by MythMusic when scanning the filesystem for musicfiles.
 */
class MPUBLIC MythBusyDialog : public MythProgressDialog
{
    Q_OBJECT

  public:
    /** \brief Create the busy indicator.

        Creates the dialog widget and sets up the timer that causes
        the widget to indicate progress every 100msec;

        \param title the title to appear in the progress bar dialog
        \param cancelButton display cancel button
        \param target target for cancel signal
        \param slot slot for cancel signal
     */
    MythBusyDialog(const QString &title,
                   bool cancelButton = false,
                   const QObject * target = NULL,
                   const char * slot = NULL);

    /** \brief Setup a timer to 'move' the spinner

        This will create a \p QTimer object that will update the
        spinner ever \p interval msecs.

        \param interval msecs between movement, default is 100
    */
    void start(int interval = 100);

    /** \brief Close the dialog.

        This will close the dialog and stop the timer.
    */
    void Close();

  public slots:
    virtual void deleteLater(void);

  protected slots:
    void setProgress();
    void timeout();

  protected:
    void Teardown(void);
    ~MythBusyDialog();

  private:
    QTimer *timer;
};

class MPUBLIC MythThemedDialog : public MythDialog
{
    Q_OBJECT

  public:
    MythThemedDialog(MythMainWindow *parent,
                     const QString  &window_name,
                     const QString  &theme_filename = QString(),
                     const char     *name = "MythThemedDialog",
                     bool            setsize = true);
    MythThemedDialog(MythMainWindow *parent,
                     const char     *name = "MythThemedDialog",
                     bool            setsize = true);

    virtual bool loadThemedWindow(QString window_name, QString theme_filename);
    virtual void loadWindow(QDomElement &);
    virtual void parseContainer(QDomElement &);
    virtual void parseFont(QDomElement &);
    virtual void parsePopup(QDomElement &);
    bool buildFocusList();

    UIType *getUIObject(const QString &name);

    UIType *getCurrentFocusWidget();
    void setCurrentFocusWidget(UIType *widget);

    UIManagedTreeListType *getUIManagedTreeListType(const QString &name);
    UITextType *getUITextType(const QString &name);
    UIPushButtonType *getUIPushButtonType(const QString &name);
    UITextButtonType *getUITextButtonType(const QString &name);
    UIRemoteEditType *getUIRemoteEditType(const QString &name);
    UIRepeatedImageType *getUIRepeatedImageType(const QString &name);
    UICheckBoxType *getUICheckBoxType(const QString &name);
    UISelectorType *getUISelectorType(const QString &name);
    UIBlackHoleType *getUIBlackHoleType(const QString &name);
    UIImageGridType *getUIImageGridType(const QString &name);
    UIImageType *getUIImageType(const QString &name);
    UIStatusBarType *getUIStatusBarType(const QString &name);
    UIListBtnType *getUIListBtnType(const QString &name);
    UIListTreeType *getUIListTreeType(const QString &name);
    UIKeyboardType *getUIKeyboardType(const QString &name);

    LayerSet* getContainer(const QString &name);
    fontProp* getFont(const QString &name);

    void setContext(int a_context) { context = a_context; }
    int  getContext(){return context;}

  public slots:
    virtual void deleteLater(void);
    virtual void updateBackground();
    virtual void initForeground();
    virtual void updateForeground();
    /// draws anything that intersects
    virtual void updateForeground(const QRect &);
    /// only draws the region
    virtual void updateForegroundRegion(const QRect &);
    virtual bool assignFirstFocus();
    virtual bool nextPrevWidgetFocus(bool up_or_down);
    virtual void activateCurrent();

  protected:
    ~MythThemedDialog(); // use deleteLater() instead for thread safety

    void paintEvent(QPaintEvent* e);
    UIType *widget_with_current_focus;

    // These need to be just "protected" so that subclasses can mess with them
    XMLParse *getTheme() {return theme;}
    QDomElement& getXmlData() {return xmldata;}

    QPixmap my_background;
    QPixmap my_foreground;

  private:

    void ReallyUpdateForeground(const QRect &);

    void UpdateForegroundRect(const QRect &inv_rect);

    XMLParse *theme;
    QDomElement xmldata;
    int context;

    QList<LayerSet*>  my_containers;
    vector<UIType*>   focus_taking_widgets;

    QRect redrawRect;
};

class MPUBLIC MythPasswordDialog: public MythDialog
{
  Q_OBJECT

    //
    //  Very simple class, not themeable
    //

  public:

    MythPasswordDialog(QString         message,
                       bool           *success,
                       QString         target,
                       MythMainWindow *parent,
                       const char     *name = "MythPasswordDialog",
                       bool            setsize = true);
  public slots:

    void checkPassword(const QString &);

  protected:
    ~MythPasswordDialog(); // use deleteLater() instead for thread safety
    void keyPressEvent(QKeyEvent *e);

  private:

    MythLineEdit        *password_editor;
    QString              target_text;
    bool                *success_flag;
};

class MPUBLIC MythSearchDialog: public MythPopupBox
{
    Q_OBJECT

  public:

    MythSearchDialog(MythMainWindow *parent,
                     const char     *name = "MythSearchDialog");

  public:
    void setCaption(QString text);
    void setSearchText(QString text);
    void setItems(QStringList items);
    QString getResult(void);

  public slots:
    virtual void deleteLater(void);

  protected slots:
    void searchTextChanged(void);

  protected:
    void Teardown(void);
    ~MythSearchDialog(); // use deleteLater() instead for thread safety
    void keyPressEvent(QKeyEvent *e);

  private:

    QLabel              *caption;
    MythRemoteLineEdit  *editor;
    MythListBox         *listbox;
    QAbstractButton     *ok_button;
    QAbstractButton     *cancel_button;
};

#endif
