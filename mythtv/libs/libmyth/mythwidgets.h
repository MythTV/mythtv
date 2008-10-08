#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <q3textedit.h>
#include <QPushButton>
#include <QToolButton>
#include <QDialog>
#include <q3listview.h>
#include <q3header.h>
#include <q3buttongroup.h>
#include <q3listbox.h>
#include <QCheckBox>
#include <QRadioButton>
#include <QImage>
#include <QLabel>
#include <QTimer>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QEvent>

#include <vector>

using namespace std;

#include "virtualkeyboard.h"

#include "mythexp.h"

// These widgets follow these general navigation rules:
//
// - Up and Down shift focus to the previous/next widget in the tab
// order
// - Left and Right adjust the current setting
// - Space selects


class MPUBLIC MythComboBox: public QComboBox
{
    Q_OBJECT

  public:
    MythComboBox(bool rw, QWidget* parent=0, const char* name="MythComboBox");

    void setHelpText(const QString &help);

    void setAcceptOnSelect(bool Accept)      { AcceptOnSelect = Accept; }
    void setStep(int _step = 1)              { step = _step; }
    void setAllowVirtualKeyboard(bool allowKbd = true)
    { allowVirtualKeyboard = allowKbd; }
    void setPopupPosition(PopupPosition pos) { popupPosition = pos; }
    PopupPosition getPopupPosition(void)     { return popupPosition; }

  signals:
    void changeHelpText(QString);
    void accepted(int);
    void gotFocus();

  public slots:
    virtual void deleteLater(void);
    void insertItem(const QString &item)
    {
        QComboBox::insertItem(count()+1, item);
    }

  protected:
    void Teardown(void);
    virtual ~MythComboBox(); // use deleteLater for thread safety
    virtual void keyPressEvent (QKeyEvent *e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);
    void Init(void);
    virtual void popupVirtualKeyboard(void);

  private:
    VirtualKeyboard *popup;
    QString helptext;
    bool AcceptOnSelect;
    bool useVirtualKeyboard;
    bool allowVirtualKeyboard;
    PopupPosition popupPosition;
    int step;
};

class MPUBLIC MythSpinBox: public QSpinBox
{
    Q_OBJECT

  public:
    MythSpinBox(QWidget* parent = NULL, const char* name = "MythSpinBox",
                bool allow_single_step = false)
        : QSpinBox(parent), singlestep(allow_single_step)
    {
        setObjectName(name);
        if (singlestep)
            setLineStep(10);
    }

    void setHelpText(const QString&);

    bool singleStep(void)               { return singlestep; }
    void setSingleStep(bool arg = true) { singlestep = arg; }

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

  private:
    QString helptext;
    bool singlestep;
};

class MPUBLIC MythSlider: public QSlider
{
    Q_OBJECT

  public:
    MythSlider(QWidget* parent=0, const char* name="MythSlider")
        : QSlider(parent) { setObjectName(name); };

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent (QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

  private:
    QString helptext;
};

class MPUBLIC MythLineEdit : public QLineEdit
{
    Q_OBJECT

  public:
    MythLineEdit(QWidget *parent=NULL, const char *name="MythLineEdit");
    MythLineEdit(const QString &text,
                 QWidget *parent=NULL, const char *name="MythLineEdit");

    void setHelpText(const QString&);;
    void setRW(bool readwrite = true) { rw = readwrite; };
    void setRO() { rw = false; };
    void setAllowVirtualKeyboard(bool allowKbd = true)
	       { allowVirtualKeyboard = allowKbd; }
    // muthui's MythUITextEdit m_Filter & FilterNumeric
    // may be a better way to do it
    //void setSmartVirtualKeyboard(bool allowKbd = true)
	//       { allowSmartKeyboard   = allowKbd; }
    void setPopupPosition(PopupPosition pos) { popupPosition = pos; }
    PopupPosition getPopupPosition(void) { return popupPosition; }

    virtual QString text();

  public slots:
    virtual void deleteLater(void);
    virtual void setText(const QString &text);

  signals:
    void changeHelpText(QString);

  protected:
    void Teardown(void);
    virtual ~MythLineEdit(); // use deleteLater for thread safety

    virtual void keyPressEvent(QKeyEvent *e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);
    virtual void hideEvent(QHideEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void popupVirtualKeyboard(void);

  private:
    VirtualKeyboard *popup;
    QString helptext;
    bool rw;
    bool useVirtualKeyboard;
    bool allowVirtualKeyboard;
    PopupPosition popupPosition;
};

/**
 * A LineEdit that does special things when you press number keys
 * (enter letters with multiple presses, just like a phone keypad)
 */
class MPUBLIC MythRemoteLineEdit : public Q3TextEdit
{
    Q_OBJECT

  public:
    MythRemoteLineEdit(QWidget *parent,
                       const char *name = "MythRemoteLineEdit");
    MythRemoteLineEdit(const QString &contents, QWidget *parent,
                       const char *name = "MythRemoteLineEdit");
    MythRemoteLineEdit(QFont *a_font, QWidget *parent,
                       const char *name = "MythRemoteLineEdit");
    MythRemoteLineEdit(int lines, QWidget *parent,
                       const char *name = "MythRemoteLineEdit");

    void setHelpText(const QString&);
    void setCycleTime(float desired_interval); // in seconds
    void setCharacterColors(QColor unselected, QColor selected, QColor special);
    void insert(QString text);
    void backspace();
    void del();
    void setPopupPosition(PopupPosition pos) { popupPosition = pos; };
    PopupPosition getPopupPosition(void)     { return popupPosition; };

    virtual QString text();

  signals:
    void    shiftState(bool);
    void    cycleState(QString current_choice, QString set);
    void    changeHelpText(QString);
    void    gotFocus();
    void    lostFocus();
    void    tryingToLooseFocus(bool up_or_down);
    void    textChanged(QString);

  public slots:
    virtual void deleteLater(void);
    virtual void setText(const QString& text);

  protected:
    void Teardown(void);
    virtual ~MythRemoteLineEdit(); // use deleteLater for thread safety
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void popupVirtualKeyboard(void);

  private slots:
    void    startCycle(QString current_choice, QString set);
    void    updateCycle(QString current_choice, QString set);
    void    endCycle();

  private:
    QFont   *my_font;
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

    int m_lines;

    VirtualKeyboard *popup;
    bool             useVirtualKeyboard;
    PopupPosition    popupPosition;
};

class MPUBLIC MythPushButton : public QPushButton
{
    Q_OBJECT

  public:
    MythPushButton(QWidget *parent, const char *name = "MythPushButton",
                   bool aa = false)
        : QPushButton(parent), arrowAccel(aa)
    {
        setObjectName(name);
        //setBackgroundOrigin(WindowOrigin);
        setToggleButton(false);
    }

    MythPushButton(const QString &text, QWidget *parent, bool aa = false)
        : QPushButton(text, parent), arrowAccel(aa)
    {
        setObjectName("MythPushButton");
        //setBackgroundOrigin(WindowOrigin);
        setToggleButton(false);
    }

    MythPushButton(const QString &ontext, const QString &offtext,
                   QWidget *parent, bool isOn = true, bool aa = false);

    void setHelpText(const QString &help);

    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);

    void toggleText(void);

  signals:
    void changeHelpText(QString);

  protected:
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);

  private:
    QColor origcolor;
    QString helptext;
    QString onText;
    QString offText;
    bool arrowAccel;

    QStringList keyPressActions;
};

class MPUBLIC MythCheckBox: public QCheckBox
{
    Q_OBJECT

  public:
    MythCheckBox(QWidget *parent = 0, const char *name = "MythCheckBox")
        : QCheckBox(parent)       { setObjectName(name); };
    MythCheckBox(const QString &text,
                 QWidget *parent = 0, const char *name = "MythCheckBox")
        : QCheckBox(text, parent) { setObjectName(name); };

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

  private:
    QString helptext;
};

class MPUBLIC MythRadioButton: public QRadioButton
{
    Q_OBJECT

  public:
    MythRadioButton(QWidget* parent = 0, const char* name = "MythRadioButton")
        : QRadioButton(parent) { setObjectName(name); };

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

  private:
    QString helptext;
};

class MPUBLIC MythListView : public Q3ListView
{
    Q_OBJECT

  public:
    MythListView(QWidget *parent);

    void ensureItemVCentered (const Q3ListViewItem *i);

  protected:
    void keyPressEvent(QKeyEvent *e);
    void focusInEvent(QFocusEvent *e);
};

class MPUBLIC MythListBox: public Q3ListBox {
    Q_OBJECT

  public:
    MythListBox(QWidget* parent);

    virtual void keyPressEvent(QKeyEvent* e);

    void setHelpText(const QString&);

    int currentItem() { return Q3ListBox::currentItem(); }

  protected:
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);
    virtual void polish(void);

  public slots:
    void setCurrentItem(const QString& matchText, bool caseSensitive = true,
                        bool partialMatch = false);
    void setCurrentItem(int index) { Q3ListBox::setCurrentItem(index); };

  signals:
    void changeHelpText(QString);
    void accepted(int);
    void menuButtonPressed(int);
    void editButtonPressed(int);
    void deleteButtonPressed(int);

  private:
    QString helptext;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
