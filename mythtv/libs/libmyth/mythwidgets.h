#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <qbutton.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qdialog.h>
#include <qlistview.h>
#include <qheader.h>
#include <qtable.h>
#include <qprogressbar.h>
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
    void gotFocus();

  public slots:
    void insertItem(const QString& item) {
        QComboBox::insertItem(item);
    };

  protected:
    virtual void keyPressEvent (QKeyEvent *e);
    virtual void focusInEvent(QFocusEvent *e) { emit changeHelpText(helptext);
                                                emit gotFocus();
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



//  thor feb 18 (serious redo)
//  Challenge: Use only keys 0-9 (that's it)
class MythRemoteLineEdit : public QTextEdit
{
    Q_OBJECT

    //
    //  A Line edit that does special
    //  things when you press number keys
    //
    
    public:
    
        MythRemoteLineEdit( QWidget * parent, const char * name = 0 );
        MythRemoteLineEdit( const QString & contents, QWidget * parent, const char * name = 0 );    
        ~MythRemoteLineEdit();
        void setHelpText(QString help) { helptext = help; }
        void setCycleTime(float desired_interval); // in seconds
        void setCharacterColors(QColor unselected, QColor selected, QColor special);

    signals:
    
        void    shiftState(bool);
        void    cycleState(QString current_choice, QString set);
        void    changeHelpText(QString);
        void    gotFocus();
        void    lostFocus();

    public slots:
    
        virtual void setText(const QString& text);
    
    protected:
    
        virtual void focusInEvent(QFocusEvent *e);
        virtual void focusOutEvent(QFocusEvent *e);
        virtual void keyPressEvent(QKeyEvent *e);

    private slots:

        void    startCycle(QString current_choice, QString set);
        void    updateCycle(QString current_choice, QString set);
        void    endCycle();

    private:
    
        void    Init(void);
        void    cycleKeys(QString cycleList);
        void    toggleShift(void);
        void    assignHexColors();

        bool    shift;
        QTimer  *cycle_timer;
        bool    active_cycle;
        QString current_choice;
        QString current_set;
        int     cycle_time;
        QString helptext;

        int     pre_cycle_para;
        int     pre_cycle_pos;
        QString pre_cycle_text_upto;
        QString pre_cycle_text_from;

        
        QColor  col_unselected;
        QColor  col_selected;
        QColor  col_special;
        
        QString  hex_unselected;
        QString  hex_selected;
        QString  hex_special;
        
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
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

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
    void focusInEvent(QFocusEvent *e);

  signals:
    void playPressed(QListViewItem *);
    void deletePressed(QListViewItem *);
    void infoPressed(QListViewItem *);
    void numberPressed(QListViewItem *, int);

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

class MythPopupBox : public QFrame
{
  public:
    MythPopupBox(QWidget *parent);

    void addWidget(QWidget *widget, bool setAppearance = true);

  private:
    QVBoxLayout *vbox;
};

#endif
