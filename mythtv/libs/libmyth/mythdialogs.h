#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qprogressbar.h>
#include <qdom.h>
#include <qptrlist.h>
#include <qpixmap.h>

class XMLParse;
class UIType;
class UIManagedTreeListType;
class UITextType;
class UIPushButtonType;
class UITextButtonType;
class UIRepeatedImageType;
class UIBlackHoleType;
class UIImageType;
class LayerSet;

class MythMainWindow : public QDialog
{
  public:
    MythMainWindow(QWidget *parent = 0, const char *name = 0, 
                   bool modal = FALSE);

    virtual void Show();

   protected:
    float wmult, hmult;
    int screenwidth, screenheight;
};

class MythDialog : public QDialog
{
  public:
    MythDialog(QWidget *parent = 0, const char *name = 0, bool modal = FALSE);

    virtual void Show();

   protected:
    float wmult, hmult;
    int screenwidth, screenheight;
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
    MythThemedDialog(QString window_name, QString theme_filename = "",
                     QWidget *parent = 0, const char *name = 0);

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

