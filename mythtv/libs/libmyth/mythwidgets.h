#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <qbutton.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qdialog.h>
#include <qprogressdialog.h>
#include <qlistview.h>
#include <qheader.h>
#include <qtable.h>
#include <qbuttongroup.h>
#include <qlistbox.h>
#include <qcheckbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qtimer.h>

#include <vector>

// These widgets follow these general navigation rules:
//
// - Up and Down shift focus to the previous/next widget in the tab
// order
// - Left and Right adjust the current setting
// - Space selects


class MythComboBox: public QComboBox 
{
    Q_OBJECT
  public:
    MythComboBox(bool rw, QWidget* parent=0, const char* name=0):
        QComboBox(rw, parent, name) {};

    void setHelpText(QString help) { helptext = help; }

  signals:
    void changeHelpText(QString);

  public slots:
    void insertItem(const QString& item) {
        QComboBox::insertItem(item);
    };

  protected:
    virtual void keyPressEvent (QKeyEvent *e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext);
                                                QComboBox::focusInEvent(e); }

  private:
    QString helptext;
};

class MythSpinBox: public QSpinBox 
{
    Q_OBJECT
  public:
    MythSpinBox(QWidget* parent = NULL, const char* widgetName = 0):
        QSpinBox(parent, widgetName) {};

    void setHelpText(QString help) { helptext = help; }

  signals:
    void changeHelpText(QString);

  protected:
    virtual bool eventFilter (QObject* o, QEvent* e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext);
                                                QSpinBox::focusInEvent(e); }

  private:
    QString helptext;
};

class MythSlider: public QSlider 
{
    Q_OBJECT
  public:
    MythSlider(QWidget* parent=0, const char* name=0):
        QSlider(parent, name) {};

    void setHelpText(QString help) { helptext = help; }

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent (QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext); 
                                                QSlider::focusInEvent(e); }

  private:
    QString helptext;
};


class MythLineEdit : public QLineEdit 
{
    Q_OBJECT
  public:
    MythLineEdit(QWidget *parent=NULL, const char* widgetName=0) :
      QLineEdit(parent, widgetName) { };

    MythLineEdit(const QString& contents, QWidget *parent=NULL, 
                 const char* widgetName=0) :
      QLineEdit(contents, parent, widgetName) { };

    void setHelpText(QString help) { helptext = help; }

 public slots:
     virtual void setText(const QString& text);

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext); 
                                                QLineEdit::focusInEvent(e); }

  private:
    QString helptext;
};



//    Challenge: Use only keys 0-9 (that's it)
    
class LineEditHintBox : public QLabel
{
    Q_OBJECT

    //
    //  A little hint box that goes beside
    //  the actual text editing box. There
    //  are two of these. One shows the
    //  state of shift'ing, and the other
    //  the current characters that the 
    //  current keypress will cycle through
    //

    public:
        LineEditHintBox(QWidget * parent, const char * name = 0 );
        ~LineEditHintBox();
        
    public slots:

        void    showShift(bool);
        void    showCycle(QString current_choice, QString set);

};

class KeypadLineEdit : public QLineEdit
{
    Q_OBJECT

    //
    //  The actual editor part of
    //  the compound widget
    //
    
    public:
    
        KeypadLineEdit( QWidget * parent, const char * name = 0 );
        KeypadLineEdit( const QString & contents, QWidget * parent, const char * name = 0 );    
        ~KeypadLineEdit();
        void setCycleTime(float desired_interval); // in seconds

    signals:
    
        void    shiftState(bool);
        void    cycleState(QString current_choice, QString set);
        void    askParentToChangeHelpText(void);

    protected:
    
        virtual void focusInEvent(QFocusEvent *e);
        virtual void keyPressEvent(QKeyEvent *e);

    private slots:

        void    endCycle();

    private:
    
        void    Init(void);
        void    cycleKeys(QString cycleList);
        void    toggleShift(void);

        bool    shift;
        QTimer  *cycle_timer;
        bool    active_cycle;
        QString current_choice;
        QString current_set;
        int     cycle_time;
};

//  thor feb 17 2003
//
//  Alternative to MythLineEdit that lets you enter
//  text using only (!) the keys 0-9 on a keypad/remote control
//
class MythRemoteLineEdit : public QWidget
{
    Q_OBJECT

    //
    //  A container that holds the
    //  hint boxes and the editor and
    //  co-ordinates their actions
    //
            
    public:
    
        MythRemoteLineEdit( QWidget * parent, const char * name = 0 );
        MythRemoteLineEdit( const QString & contents, QWidget * parent, const char * name = 0 );    
        ~MythRemoteLineEdit();
        void setCycleTime(float desired_interval); //seconds
        void setHelpText(QString help) { helptext = help; }

        //
        //  These are three widgets inside
        //  the container. They should really
        //  be private, but are public for
        //  the time being (so you can hack
        //  settings on them like
        //  MythRemoteLineEdit->editor->setPalette()
        //  and so on)
        //

        KeypadLineEdit      *editor;
        LineEditHintBox     *character_hint;
        LineEditHintBox     *shift_hint;

        
    public slots:
    
        void setGeometry(int x, int y, int w, int h);
        virtual void setText(const QString& text);
        void textHasChanged(const QString &);
        void honorRequestToChangeHelpText(void);

    signals:
        void changeHelpText(QString);
        void textChanged(const QString &);

    private:
    
        void    Init(void);
        QString helptext;
    
};


class MythTable : public QTable
{
  public:
    MythTable(QWidget *parent) : QTable(parent) { }

    void keyPressEvent(QKeyEvent *e);
};

class MythButtonGroup : public QButtonGroup
{
  public:
    MythButtonGroup(QWidget *parent = 0) : QButtonGroup(parent) { }

    void moveFocus(int key);
};

class MythToolButton : public QToolButton
{
  public:
    MythToolButton(QWidget *parent) : QToolButton(parent)
                    { setFocusPolicy(StrongFocus);
                      setBackgroundOrigin(WindowOrigin); }

    void drawButton(QPainter *p);

  private:
    QColor origcolor;
};

class MythPushButton : public QPushButton
{
  public:
    MythPushButton(QWidget *parent, const char *name = 0)
                 : QPushButton(parent, name)
                  { setBackgroundOrigin(WindowOrigin); }

    MythPushButton(const QString &text, QWidget *parent)
                 : QPushButton(text, parent)
                  { setBackgroundOrigin(WindowOrigin); }

    void drawButton(QPainter *p);

  private:
    QColor origcolor;
};

class MythCheckBox: public QCheckBox
{
    Q_OBJECT
  public:
    MythCheckBox(QWidget* parent = 0, const char* name = 0):
        QCheckBox(parent, name) {};
    void setHelpText(QString help) { helptext = help; }

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext); 
                                                QCheckBox::focusInEvent(e); }

  private:
    QString helptext;
};

class MythDialog : public QDialog
{
  public:
    MythDialog(QWidget *parent = 0, const char *name = 0, bool modal = FALSE);
    
    virtual void Show();

    float getHFactor(void) { return hmult; }
    float getWFactor(void) { return wmult; }

  protected:
    float wmult, hmult;
    int screenwidth, screenheight;
};

class MythProgressDialog: public QProgressDialog {
public:
    MythProgressDialog(const QString& labelText, int totalSteps,
                       QWidget* parent=NULL, const char* name=0,
                       bool modal=FALSE);
        
};

class MythListView : public QListView
{
    Q_OBJECT
  public:
    MythListView(QWidget *parent);

    void SetAllowKeypress(bool allow) { allowkeypress = allow; }
    bool GetAllowKeypress(void) { return allowkeypress; }
    void DontFixSpaceBar(void) { fixspace = false; }

  protected:
    void keyPressEvent( QKeyEvent *e );

  signals:
    void playPressed(QListViewItem *);
    void deletePressed(QListViewItem *);

  private:
    bool allowkeypress;
    bool fixspace;
};

class MythListBox: public QListBox {
    Q_OBJECT
public:
    MythListBox(QWidget* parent): QListBox(parent) {};

    virtual void keyPressEvent(QKeyEvent* e);

public slots:
    void setCurrentItem(const QString& text);
    void setCurrentItem(int index) { QListBox::setCurrentItem(index); };
};

#endif
