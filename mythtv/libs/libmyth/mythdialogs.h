#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qprogressbar.h>
#include <qdom.h>
#include <qptrlist.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qevent.h>

#include <vector>
using namespace std;

#include "uitypes.h"
#include "lcddevice.h"

class XMLParse;
class UIType;
class UIManagedTreeListType;
class UITextType;
class UIPushButtonType;
class UITextButtonType;
class UIRepeatedImageType;
class UICheckBoxType;
class UISelectorType;
class UIBlackHoleType;
class UIImageType;
class UIStatusBarType;
class LayerSet;
class GenericTree;

const int kExternalKeycodeEventType = 33213;

class ExternalKeycodeEvent : public QCustomEvent
{
  public:
    ExternalKeycodeEvent(const int key) 
           : QCustomEvent(kExternalKeycodeEventType), keycode(key) {}

    int getKeycode() { return keycode; }

  private:
    int keycode;
};

class MythMainWindow : public QDialog
{
  public:
    MythMainWindow(QWidget *parent = 0, const char *name = 0, 
                   bool modal = FALSE);

    void Init(void);
    void Show(void);

    void attach(QWidget *child);
    void detach(QWidget *child);

    QWidget *currentWidget(void);

   protected:
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *ce);

    QObject *getTarget(QKeyEvent &key);

    float wmult, hmult;
    int screenwidth, screenheight;
    int xbase, ybase;

    vector<QWidget *> widgetList;

    bool ignore_lirc_keys;
};

class MythDialog : public QFrame
{
    Q_OBJECT
  public:
    MythDialog(MythMainWindow *parent, const char *name = 0, 
               bool setsize = true);
   ~MythDialog();

    enum DialogCode { Rejected, Accepted };

    int result(void) const { return rescode; }

    virtual void Show();

    void hide();

    void setNoErase(void);

  public slots:
    int exec();
    virtual void done( int );

  protected slots:
    virtual void accept();
    virtual void reject();

  protected:
    void setResult(int r) { rescode = r; }
    void keyPressEvent(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;
    int xbase, ybase;
 
    MythMainWindow *m_parent;

    int rescode;

    bool in_loop;

    QFont defaultBigFont, defaultMediumFont, defaultSmallFont;
};

class MythPopupBox : public MythDialog
{
    Q_OBJECT
  public:
    MythPopupBox(MythMainWindow *parent, const char *name = 0);
    MythPopupBox(MythMainWindow *parent, bool graphicPopup, 
                 QColor popupForeground, QColor popupBackground,
                 QColor popupHighlight, const char *name = 0);

    void addWidget(QWidget *widget, bool setAppearance = true);

    typedef enum { Large, Medium, Small } LabelSize;

    QLabel *addLabel(QString caption, LabelSize size = Medium, 
                     bool wrap = false);

    QButton *addButton(QString caption, QObject *target = NULL, 
                       const char *slot = NULL);

    void ShowPopup(QObject *target = NULL, const char *slot = NULL);
    int ExecPopup(QObject *target = NULL, const char *slot = NULL);

    static void showOkPopup(MythMainWindow *parent, QString title,
                            QString message);
    static bool showOkCancelPopup(MythMainWindow *parent, QString title,
                                  QString message, bool focusOk);

  protected slots:
    void defaultButtonPressedHandler(void);
    void defaultExitHandler(void);

  private:
    QVBoxLayout *vbox;
    QColor popupForegroundColor;
    int hpadding, wpadding;
};

class MythProgressDialog: public MythDialog
{
  public:
    MythProgressDialog(const QString& message, int totalSteps);

    void Close(void);
    void setProgress(int curprogress);
    void keyPressEvent(QKeyEvent *);

  private:
    QProgressBar *progress;
    int steps;

    QPtrList<LCDTextItem> *textItems;
};

class MythThemedDialog : public MythDialog
{
    Q_OBJECT
  public:
    MythThemedDialog(MythMainWindow *parent, QString window_name,
                     QString theme_filename = "", const char *name = 0,
                     bool setsize = true);
   ~MythThemedDialog();

    virtual void loadWindow(QDomElement &);
    virtual void parseContainer(QDomElement &);
    virtual void parseFont(QDomElement &);
    virtual void parsePopup(QDomElement &);
    bool buildFocusList();

    UIType *getUIObject(const QString &name);
    UIType *getCurrentFocusWidget();
    UIManagedTreeListType *getUIManagedTreeListType(const QString &name);
    UITextType *getUITextType(const QString &name);
    UIPushButtonType *getUIPushButtonType(const QString &name);
    UITextButtonType *getUITextButtonType(const QString &name);
    UIRepeatedImageType *getUIRepeatedImageType(const QString &name);
    UICheckBoxType *getUICheckBoxType(const QString &name);
    UISelectorType *getUISelectorType(const QString &name);
    UIBlackHoleType *getUIBlackHoleType(const QString &name);
    UIImageType *getUIImageType(const QString &name);
    UIStatusBarType*        getUIStatusBarType(const QString &name);

    void setContext(int a_context) { context = a_context; }
    int  getContext(){return context;}

  public slots:

    virtual void updateBackground();
    virtual void initForeground();
    virtual void updateForeground();
    virtual void updateForeground(const QRect &);
    virtual bool assignFirstFocus();
    virtual bool nextPrevWidgetFocus(bool up_or_down);
    virtual void activateCurrent();

  protected:

    void paintEvent(QPaintEvent* e);
    UIType              *widget_with_current_focus;

    //
    //  These need to be just "protected"
    //  so that subclasses can mess with them
    //

    QPixmap my_background;
    QPixmap my_foreground;

  private:

    XMLParse *theme;
    QDomElement xmldata;
    int context;

    QPtrList<LayerSet>  my_containers;
    QPtrList<UIType>    focus_taking_widgets;
};

class MythPasswordDialog: public MythDialog
{
  Q_OBJECT

    //
    //  Very simple class, not themeable
    //

  public:

    MythPasswordDialog( QString message,
                        bool *success,
                        QString target,
                        MythMainWindow *parent, 
                        const char *name = 0, 
                        bool setsize = true);
    ~MythPasswordDialog();

  public slots:
  
    void checkPassword(const QString &);

 protected:
 
    void keyPressEvent(QKeyEvent *e);

  private:
  
    MythLineEdit        *password_editor;
    QString             target_text;
    bool                *success_flag;
};

class Myth2ButtonDialog: public MythDialog
{
    Q_OBJECT
  public:
    Myth2ButtonDialog(MythMainWindow *parent, QString title, QString message,
                      QString button1, QString button2, int defvalue);
    ~Myth2ButtonDialog() { releaseKeyboard(); delete popup; };

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:
    MythPopupBox *popup;
    MythPushButton *but1, *but2;
};

class MythImageFileDialog: public MythThemedDialog
{
    //
    //  Simple little popup dialog (themeable)
    //  that lets a user find files/directories
    //

    Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;
    
    MythImageFileDialog(QString *result,
                        QString top_directory,
                        MythMainWindow *parent, 
                        QString window_name,
                        QString theme_filename = "", 
                        const char *name = 0,
                        bool setsize=true);
    ~MythImageFileDialog();

  public slots:
  
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntered(int, IntVector*);
    void buildTree(QString starting_where);
    void buildFileList(QString directory);

  protected:
  
    void keyPressEvent(QKeyEvent *e);

  private:

    QString               *selected_file;
    UIManagedTreeListType *file_browser;
    GenericTree           *root_parent;
    GenericTree           *file_root;
    GenericTree           *initial_node;
    UIImageType           *image_box;
    QStringList           image_files;
};


#endif

