#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qprogressbar.h>
#include <qdom.h>
#include <qptrlist.h>
#include <qpixmap.h>
#include <qpushbutton.h>

#include <vector>
using namespace std;

#include "uitypes.h"

class XMLParse;
class UIType;
class UIManagedTreeListType;
class UITextType;
class UIPushButtonType;
class UITextButtonType;
class UIRepeatedImageType;
class UIBlackHoleType;
class UIImageType;
class UIStatusBarType;
class LayerSet;

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

    float wmult, hmult;
    int screenwidth, screenheight;

    vector<QWidget *> widgetList;
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

  protected slots:
    virtual void done( int );
    virtual void accept();
    virtual void reject();

  protected:
    void setResult(int r) { rescode = r; }
    void keyPressEvent(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;
 
    MythMainWindow *m_parent;

    int rescode;

    bool in_loop;
};

class MythPopupBox : public MythDialog
{
  public:
    MythPopupBox(MythMainWindow *parent, const char *name = 0);

    void addWidget(QWidget *widget, bool setAppearance = true);

  private:
    QVBoxLayout *vbox;
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
};

class MythThemedDialog : public MythDialog
{
    Q_OBJECT
  public:
    MythThemedDialog(MythMainWindow *parent, QString window_name,
                     QString theme_filename = "", const char *name = 0);

    virtual void loadWindow(QDomElement &);
    virtual void parseContainer(QDomElement &);
    virtual void parseFont(QDomElement &);
    virtual void parsePopup(QDomElement &);

    UIType *getUIObject(const QString &name);
    UIManagedTreeListType *getUIManagedTreeListType(const QString &name);
    UITextType *getUITextType(const QString &name);
    UIPushButtonType *getUIPushButtonType(const QString &name);
    UITextButtonType *getUITextButtonType(const QString &name);
    UIRepeatedImageType *getUIRepeatedImageType(const QString &name);
    UIBlackHoleType *getUIBlackHoleType(const QString &name);
    UIImageType *getUIImageType(const QString &name);
    UIStatusBarType*        getUIStatusBarType(const QString &name);

    void setContext(int a_context) { context = a_context; }

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

  private:

    XMLParse *theme;
    QDomElement xmldata;
    QPixmap my_background;
    QPixmap my_foreground;
    int context;

    QPtrList<LayerSet>  my_containers;
    UIType              *widget_with_current_focus;
    QPtrList<UIType>    focus_taking_widgets;
};

#endif

