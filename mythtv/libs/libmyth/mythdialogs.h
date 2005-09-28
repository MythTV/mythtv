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
#include <qvaluevector.h>
#include <qscrollview.h>

#include <vector>
using namespace std;

#include "uitypes.h"

class XMLParse;
class UIType;
class UIManagedTreeListType;
class UITextType;
class UIMultiTextType;
class UIPushButtonType;
class UITextButtonType;
class UIRemoteEditType;
class UIRepeatedImageType;
class UICheckBoxType;
class UISelectorType;
class UIBlackHoleType;
class UIImageType;
class UIStatusBarType;
class UIListBtnType;
class UIListTreeType;
class LayerSet;
class GenericTree;
class MythMediaDevice;

const int kExternalKeycodeEventType = 33213;
const int kExitToMainMenuEventType = 33214;

class ExternalKeycodeEvent : public QCustomEvent
{
  public:
    ExternalKeycodeEvent(const int key) 
           : QCustomEvent(kExternalKeycodeEventType), keycode(key) {}

    int getKeycode() { return keycode; }

  private:
    int keycode;
};

class ExitToMainMenuEvent : public QCustomEvent
{
  public:
    ExitToMainMenuEvent(void) : QCustomEvent(kExitToMainMenuEventType) {}
};

#define REG_KEY(a, b, c, d) gContext->GetMainWindow()->RegisterKey(a, b, c, d)
#define GET_KEY(a, b) gContext->GetMainWindow()->GetKey(a, b)
#define REG_JUMP(a, b, c, d) gContext->GetMainWindow()->RegisterJump(a, b, c, d)
#define REG_MEDIA_HANDLER(a, b, c, d, e) gContext->GetMainWindow()->RegisterMediaHandler(a, b, c, d, e)
#define REG_MEDIAPLAYER(a,b,c) gContext->GetMainWindow()->RegisterMediaPlugin(a, b, c)

typedef  int (*MediaPlayCallback)(const char*, const char*, const char*, const char*, int, const char*);

class MythMainWindowPrivate;

class MythMainWindow : public QDialog
{
  public:
    MythMainWindow(QWidget *parent = 0, const char *name = 0, 
                   bool modal = FALSE, WFlags flags = 0);
    virtual ~MythMainWindow();

    void Init(void);
    void Show(void);

    void attach(QWidget *child);
    void detach(QWidget *child);

    QWidget *currentWidget(void);

    bool TranslateKeyPress(const QString &context, QKeyEvent *e, 
                           QStringList &actions);

    void ClearKey(const QString &context, const QString &action);
    void BindKey(const QString &context, const QString &action,
		 const QString &key);
    void RegisterKey(const QString &context, const QString &action,
                     const QString &description, const QString &key);
    QString GetKey(const QString &context, const QString &action) const;

    void ClearJump(const QString &destination);
    void BindJump(const QString &destination, const QString &key);
    void RegisterJump(const QString &destination, const QString &description,
                      const QString &key, void (*callback)(void));
    void RegisterMediaHandler(const QString &destination, 
                              const QString &description, const QString &key, 
                              void (*callback)(MythMediaDevice* mediadevice), 
                              int mediaType);

    void RegisterMediaPlugin(const QString &name, const QString &desc,
                             MediaPlayCallback fn);
    bool HandleMedia(QString& handler, const QString& mrl, const QString& plot="",
                     const QString& title="", const QString& director="", int lenMins=120, 
                     const QString& year="1895");

    void JumpTo(const QString &destination);
    bool DestinationExists(const QString &destination) const;


  protected:
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *ce);
    void closeEvent(QCloseEvent *e);

    void ExitToMainMenu();

    QObject *getTarget(QKeyEvent &key);

    MythMainWindowPrivate *d;
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

    virtual void Show(void);

    void hide(void);

    void setNoErase(void);
   
    virtual bool onMediaEvent(MythMediaDevice * mediadevice); 
    
  signals:
    void menuButtonPressed();

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
    void addLayout(QLayout *layout, int stretch = 0);

    typedef enum { Large, Medium, Small } LabelSize;

    QLabel *addLabel(QString caption, LabelSize size = Medium, 
                     bool wrap = false);

    QButton *addButton(QString caption, QObject *target = NULL, 
                       const char *slot = NULL);

    void ShowPopup(QObject *target = NULL, const char *slot = NULL);
    void ShowPopupAtXY(int destx, int desty, 
                       QObject *target = NULL, const char *slot = NULL);

    int ExecPopup(QObject *target = NULL, const char *slot = NULL);

    static void showOkPopup(MythMainWindow *parent, QString title,
                            QString message);
    static bool showOkCancelPopup(MythMainWindow *parent, QString title,
                                  QString message, bool focusOk);
    static int show2ButtonPopup(MythMainWindow *parent, QString title,
                                QString message, QString button1msg,
                                QString button2msg, int defvalue);
    static int showButtonPopup(MythMainWindow *parent, QString title,
                               QString message, QStringList buttonmsgs,
                               int defvalue);

    static bool showGetTextPopup(MythMainWindow *parent, QString title,
                                 QString message, QString& text);
  signals:
    void popupDone();

  protected:
    bool focusNextPrevChild(bool next);
    void keyPressEvent(QKeyEvent *e);

  protected slots:
    void defaultButtonPressedHandler(void);
    void defaultExitHandler(void);

  private:
    QVBoxLayout *vbox;
    QColor popupForegroundColor;
    int hpadding, wpadding;
    bool arrowAccel;
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

    QPtrList<class LCDTextItem> * textItems;

    int m_totalSteps;
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
    UIMultiTextType *getUIMultiTextType(const QString &name);
    UIPushButtonType *getUIPushButtonType(const QString &name);
    UITextButtonType *getUITextButtonType(const QString &name);
    UIRemoteEditType *getUIRemoteEditType(const QString &name);
    UIRepeatedImageType *getUIRepeatedImageType(const QString &name);
    UICheckBoxType *getUICheckBoxType(const QString &name);
    UISelectorType *getUISelectorType(const QString &name);
    UIBlackHoleType *getUIBlackHoleType(const QString &name);
    UIImageType *getUIImageType(const QString &name);
    UIStatusBarType *getUIStatusBarType(const QString &name);
    UIListBtnType *getUIListBtnType(const QString &name);
    UIListTreeType *getUIListTreeType(const QString &name);
    LayerSet* getContainer(const QString &name);

    void setContext(int a_context) { context = a_context; }
    int  getContext(){return context;}

  public slots:

    virtual void updateBackground();
    virtual void initForeground();
    virtual void updateForeground();
    virtual void updateForeground(const QRect &); // draws anything that intersects
    virtual void updateForegroundRegion(const QRect &); // only draws the region
    virtual bool assignFirstFocus();
    virtual bool nextPrevWidgetFocus(bool up_or_down);
    virtual void activateCurrent();

  protected:

    void paintEvent(QPaintEvent* e);
    UIType *widget_with_current_focus;

    // These need to be just "protected" so that subclasses can mess with them
    XMLParse *getTheme() {return theme;}
    QDomElement& getXmlData() {return xmldata;}
    
    QPixmap my_background;
    QPixmap my_foreground;

  private:

    void ReallyUpdateForeground(const QRect &);

    XMLParse *theme;
    QDomElement xmldata;
    int context;

    QPtrList<LayerSet>  my_containers;
    QPtrList<UIType>    focus_taking_widgets;

    QRect redrawRect;
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

class MythSearchDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    MythSearchDialog(MythMainWindow *parent, const char *name = 0); 
    ~MythSearchDialog();

  public: 
    void setCaption(QString text);
    void setSearchText(QString text);
    void setItems(QStringList items); 
    QString getResult(void);
    
 protected slots:
    void okPressed(void);
    void cancelPressed(void);   
    void searchTextChanged(void);
    void itemSelected(int index);
     
 protected:
    void keyPressEvent(QKeyEvent *e);

  private:
  
    QLabel              *caption;
    MythRemoteLineEdit  *editor;
    MythListBox         *listbox;  
    QButton             *ok_button;
    QButton             *cancel_button;
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
    QString               initialDir;
};

// ---------------------------------------------------------------------------

class MythScrollDialog : public QScrollView
{
    Q_OBJECT
    
  public:

    enum DialogCode {
        Rejected,
        Accepted
    };

    enum ScrollMode {
        HScroll=0,
        VScroll
    };

    MythScrollDialog(MythMainWindow *parent, ScrollMode mode=HScroll,
                     const char *name = 0);
    ~MythScrollDialog();

    void setArea(int w, int h);
    void setAreaMultiplied(int areaWTimes, int areaHTimes);

    int  result() const;

  public slots:

    int  exec();
    virtual void done(int);
    virtual void show();
    virtual void hide();
    virtual void setContentsPos(int x, int y);
    
  protected slots:

    virtual void accept();
    virtual void reject();

  protected:

    void         keyPressEvent(QKeyEvent *e);
    virtual void paintEvent(QRegion& region, int x, int y, int w, int h);

    void setResult(int r);
    void viewportPaintEvent(QPaintEvent *pe);

    MythMainWindow *m_parent;
    int             m_screenWidth, m_screenHeight;
    int             m_xbase, m_ybase;
    float           m_wmult, m_hmult;

    ScrollMode      m_scrollMode;

    QFont           m_defaultBigFont;
    QFont           m_defaultMediumFont;
    QFont           m_defaultSmallFont;

    int             m_resCode;
    bool            m_inLoop;
    
    QPixmap        *m_bgPixmap;
    
    QPixmap        *m_upArrowPix;
    QPixmap        *m_dnArrowPix;
    QPixmap        *m_rtArrowPix;
    QPixmap        *m_ltArrowPix;

    bool            m_showUpArrow;
    bool            m_showDnArrow;
    bool            m_showLtArrow;
    bool            m_showRtArrow;

    QRect           m_upArrowRect;
    QRect           m_dnArrowRect;
    QRect           m_rtArrowRect;
    QRect           m_ltArrowRect;
};

#endif

