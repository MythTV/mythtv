#ifndef DIALOGBOX_H_
#define DIALOGBOX_H_

#include "mythwidgets.h"
#include "xmlparse.h"

class QVBoxLayout;
class QButtonGroup;
class QString;
class QCheckBox;

class DialogBox : public MythDialog
{
    Q_OBJECT
  public:
    DialogBox(const QString &text, const char *checkboxtext = 0,
              QWidget *parent = 0, const char *name = 0);

    void AddButton(const QString &title);

    bool getCheckBoxState(void) {  if (checkbox) return checkbox->isChecked();
                                   return false; }
  protected slots:
    void buttonPressed(int which);

  private:
    QVBoxLayout *box;
    QButtonGroup *buttongroup;

    QCheckBox *checkbox;
};

class MythThemedDialog : public MythDialog
{
    Q_OBJECT

    //
    //  A potential base class for Myth GUI screens that are built
    //  from ui.xml files
    //
    
  public:
  
    //
    //  Constructor needs the name of "this" window (the one to be 
    //  themed) and possibly a filename where the theme is
    //
      
    MythThemedDialog(QString window_name, QString theme_filename = "",
                     QWidget *parent = 0, const char *name = 0);


    virtual void loadWindow(QDomElement &);
    virtual void parseContainer(QDomElement &);
    virtual void parseFont(QDomElement &);
    virtual void parsePopup(QDomElement &);

    
    //
    //  Methods to get pointers to dynamically created
    //  objects
    //

    UIType*                 getUIObject(QString name);
    UIManagedTreeListType*  getUIManagedTreeListType(QString name);
    UITextType*             getUITextType(QString name);
    UIPushButtonType*       getUIPushButtonType(QString name);
    UITextButtonType*       getUITextButtonType(QString name);
    UIRepeatedImageType*    getUIRepeatedImageType(QString name);
    
    
    void setContext(int a_context){context = a_context;}
    
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
  
    XMLParse    *theme;
    QDomElement xmldata;                     
    QPixmap     my_background;
    QPixmap     my_foreground;
    int         context;
    
    QPtrList<LayerSet>  my_containers;
    UIType              *widget_with_current_focus;
    QPtrList<UIType>    focus_taking_widgets;
};

#endif
