#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

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
