#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <qcombobox.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qdialog.h>
#include <qlistview.h>
#include <qheader.h>
#include <qtable.h>
#include <qbuttongroup.h>

class MythContext;

// These widgets follow these general navigation rules:
//
// - Up and Down shift focus to the previous/next widget in the tab
// order
// - Left and Right adjust the current setting
// - Space selects


class MythComboBox: public QComboBox {
public:
    MythComboBox(bool rw, QWidget* parent=0, const char* name=0):
        QComboBox(rw, parent, name) {};
protected:
    virtual void keyPressEvent (QKeyEvent* e);
};


class MythSpinBox: public QSpinBox {
public:
    MythSpinBox(QWidget* parent = NULL, const char* widgetName = 0):
        QSpinBox(parent, widgetName) {};
protected:
    virtual bool eventFilter (QObject* o, QEvent* e);
};

class MythSlider: public QSlider {
public:
    MythSlider(QWidget* parent=0, const char* name=0):
        QSlider(parent, name) {};
protected:
    virtual void keyPressEvent (QKeyEvent* e);
};

class MythLineEdit : public QLineEdit {
  public:
    MythLineEdit(QWidget *parent=NULL, const char* widgetName=0) :
      QLineEdit(parent, widgetName) { };

    MythLineEdit(const QString& contents, QWidget *parent=NULL, const char* widgetName=0) :
      QLineEdit(contents, parent, widgetName) { };

protected:
  virtual void keyPressEvent(QKeyEvent *e);
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
    MythPushButton(const QString &text, QWidget *parent)
                 : QPushButton(text, parent)
                  { setBackgroundOrigin(WindowOrigin); }

    void drawButton(QPainter *p);

  private:
    QColor origcolor;
};

class MythDialog : public QDialog
{
  public:
    MythDialog(MythContext *context, QWidget *parent = 0, const char *name = 0,
               bool modal = FALSE);
    
    void Show();

    float getHFactor(void) { return hmult; }
    float getWFactor(void) { return wmult; }

  protected:
    MythContext *m_context;
    float wmult, hmult;
    int screenwidth, screenheight;
};

class MythListView : public QListView
{
    Q_OBJECT
  public:
    MythListView(QWidget *parent);

    void SetAllowKeypress(bool allow) { allowkeypress = allow; }
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

#endif
