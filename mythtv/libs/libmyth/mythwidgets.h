#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <qcombobox.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>

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
    virtual void keyPressEvent (QKeyEvent* e) {
        switch (e->key()) {
        case Key_Up:
            focusNextPrevChild(false);
            break;
        case Key_Down:
            focusNextPrevChild(true);
            break;
        case Key_Left:
            if (currentItem() == 0)
                setCurrentItem(count()-1);
            else
                setCurrentItem((currentItem() - 1) % count());
            break;
        case Key_Right:
            setCurrentItem((currentItem() + 1) % count());
            break;
        case Key_Space:
            popup();
            break;
        default:
            e->ignore();
            return;
        }
    }
};


class MythSpinBox: public QSpinBox {
public:
    MythSpinBox(QWidget* parent = NULL, const char* widgetName = 0):
        QSpinBox(parent, widgetName) {};
protected:
    virtual bool eventFilter (QObject* o, QEvent* e) {
        (void)o;

        if (e->type() != QEvent::KeyPress)
            return FALSE;

        switch (((QKeyEvent*)e)->key()) {
        case Key_Up:
            focusNextPrevChild(false);
            break;
        case Key_Down:
            focusNextPrevChild(true);
            break;
        case Key_Left:
            stepDown();
            break;
        case Key_Right:
            stepUp();
            break;
        default:
            return FALSE;
        }
        return TRUE;
    };
};

class MythSlider: public QSlider {
public:
    MythSlider(QWidget* parent=0, const char* name=0):
        QSlider(parent, name) {};
protected:
    virtual void keyPressEvent (QKeyEvent* e) {
        switch (e->key()) {
        case Key_Up:
            focusNextPrevChild(false);
            break;
        case Key_Down:
            focusNextPrevChild(true);
            break;
        case Key_Left:
            setValue(value() - lineStep());
            break;
        case Key_Right:
            setValue(value() + lineStep());
            break;
        default:
            QSlider::keyPressEvent(e);
        }
    }
};

class MythLineEdit : public QLineEdit {
  public:
    MythLineEdit(QWidget *parent=NULL, const char* widgetName=0) :
      QLineEdit(parent, widgetName) { };

    MythLineEdit(const QString& contents, QWidget *parent=NULL, const char* widgetName=0) :
      QLineEdit(contents, parent, widgetName) { };

protected:
  virtual void keyPressEvent(QKeyEvent *e) {
      switch (e->key())
          {
          case Key_Up:
              focusNextPrevChild(FALSE);
              break;
          case Key_Down:
              focusNextPrevChild(TRUE);
              break;
          default:
              QLineEdit::keyPressEvent(e);
          }
  };
};

class MyToolButton : public QToolButton
{
  public:
    MyToolButton(QWidget *parent) : QToolButton(parent)
                    { setFocusPolicy(StrongFocus);
                      setBackgroundOrigin(WindowOrigin); }

    void drawButton(QPainter *p);

  private:
    QColor origcolor;
};

class MyPushButton : public QPushButton
{
  public:
    MyPushButton(const QString &text, QWidget *parent)
               : QPushButton(text, parent)
                { setBackgroundOrigin(WindowOrigin); }

    void drawButton(QPainter *p);

  private:
    QColor origcolor;
};

#endif
