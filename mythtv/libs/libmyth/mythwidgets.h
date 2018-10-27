#ifndef MYTHWIDGETS_H_
#define MYTHWIDGETS_H_

#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QDialog>
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
#include <QTextEdit>
#include <QListWidget>
#include <QGroupBox>

#include <vector>

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
    MythComboBox(bool rw, QWidget* parent=nullptr, const char* name="MythComboBox");

    void setHelpText(const QString &help);

    void setAcceptOnSelect(bool Accept)      { AcceptOnSelect = Accept; }
    void setStep(int _step = 1)              { step = _step; }

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
    void keyPressEvent (QKeyEvent *e) override;  // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget
    void Init(void);

  private:
    QString helptext;
    bool AcceptOnSelect;
    int step;
};

class MPUBLIC MythSpinBox: public QSpinBox
{
    Q_OBJECT

  public:
    MythSpinBox(QWidget* parent = nullptr, const char* name = "MythSpinBox",
                bool allow_single_step = false);

    void setHelpText(const QString&);

    bool allowSingleStep(void)               { return allowsinglestep; }
    void setAllowSingleStep(bool arg = true) { allowsinglestep = arg; }

  signals:
    void changeHelpText(QString);

  protected:
    void keyPressEvent(QKeyEvent* e) override;   // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget

  private:
    QString helptext;
    bool allowsinglestep;
};

class MPUBLIC MythSlider: public QSlider
{
    Q_OBJECT

  public:
    MythSlider(QWidget* parent=nullptr, const char* name="MythSlider");

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    void keyPressEvent (QKeyEvent* e) override;  // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget

  private:
    QString helptext;
};

class MPUBLIC MythLineEdit : public QLineEdit
{
    Q_OBJECT

  public:
    MythLineEdit(QWidget *parent=nullptr, const char *name="MythLineEdit");
    MythLineEdit(const QString &text,
                 QWidget *parent=nullptr, const char *name="MythLineEdit");

    void setHelpText(const QString&);;
    void setRW(bool readwrite = true) { rw = readwrite; };
    void setRO() { rw = false; };

    virtual QString text();

  public slots:
    virtual void deleteLater(void);
    virtual void setText(const QString &text);

  signals:
    void changeHelpText(QString);

  protected:
    void Teardown(void);
    virtual ~MythLineEdit(); // use deleteLater for thread safety

    void keyPressEvent(QKeyEvent *e) override;   // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget
    void hideEvent(QHideEvent *e) override;      // QWidget
    void mouseDoubleClickEvent(QMouseEvent *e) override; // QWidget

  private:
    QString helptext;
    bool rw;
};

/**
 * A LineEdit that does special things when you press number keys
 * (enter letters with multiple presses, just like a phone keypad)
 */
class MPUBLIC MythRemoteLineEdit : public QTextEdit
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
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget
    void keyPressEvent(QKeyEvent *e) override;   // QWidget

  private slots:
    void    startCycle(QString current_choice, QString set);
    void    updateCycle(QString current_choice, QString set);
    void    endCycle(bool select);
    void    endCycle(void) { endCycle(true); }

  private:
    QFont   *my_font;
    void    Init(void);
    void    cycleKeys(QString cycleList);
    void    toggleShift(void);

    bool    shift;
    QTimer  *cycle_timer;
    bool    active_cycle;
    QString current_choice;
    QString current_set;
    int     cycle_time;
    QString helptext;

    int pre_cycle_pos;
    QString pre_cycle_text_before_cursor;
    QString pre_cycle_text_after_cursor;

    QColor  col_unselected;
    QColor  col_selected;
    QColor  col_special;

    QString  hex_unselected;
    QString  hex_selected;
    QString  hex_special;

    int m_lines;
};

class MPUBLIC MythPushButton : public QPushButton
{
    Q_OBJECT

  public:
    MythPushButton(QWidget *parent, const char *name = "MythPushButton");

    MythPushButton(const QString &text, QWidget *parent);

    MythPushButton(const QString &ontext, const QString &offtext,
                   QWidget *parent, bool isOn = true);

    void setHelpText(const QString &help);

    void keyPressEvent(QKeyEvent *e) override; // QWidget
    void keyReleaseEvent(QKeyEvent *e) override; // QWidget

    void toggleText(void);

  signals:
    void changeHelpText(QString);

  protected:
    void focusInEvent(QFocusEvent *e) override; // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget

  private:
    QColor origcolor;
    QString helptext;
    QString onText;
    QString offText;

    QStringList keyPressActions;
};

class MPUBLIC MythCheckBox: public QCheckBox
{
    Q_OBJECT

  public:
    MythCheckBox(QWidget *parent = nullptr, const char *name = "MythCheckBox");
    MythCheckBox(const QString &text,
                 QWidget *parent = nullptr, const char *name = "MythCheckBox");

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    void keyPressEvent(QKeyEvent* e) override;   // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget

  private:
    QString helptext;
};

class MPUBLIC MythRadioButton: public QRadioButton
{
    Q_OBJECT

  public:
    MythRadioButton(QWidget* parent = nullptr, const char* name = "MythRadioButton");

    void setHelpText(const QString&);

  signals:
    void changeHelpText(QString);

  protected:
    void keyPressEvent(QKeyEvent* e) override;   // QWidget
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget

  private:
    QString helptext;
};

class MPUBLIC MythListBox: public QListWidget
{
    Q_OBJECT

  public:
    MythListBox(QWidget       *parent,
                const QString &name = QString("MythListBox"));

    void keyPressEvent(QKeyEvent* e) override; // QWidget

    QString currentText(void) const { return text(currentRow()); }

    void setTopRow(uint row);
    void insertItem(const QString&);
    void insertStringList(const QStringList&);
    void removeRow(uint row);
    void changeItem(const QString&, uint row);
    int  getIndex(const QList<QListWidgetItem*>&);
    QList<QListWidgetItem*> findItems(
        const QString &text, Qt::MatchFlags flags = Qt::MatchStartsWith) const
    {
        return QListWidget::findItems(text, flags);
    }


    void setHelpText(const QString&);

  protected:
    void focusInEvent(QFocusEvent *e) override;  // QWidget
    void focusOutEvent(QFocusEvent *e) override; // QWidget
    // Not an override because the underlying QWidget::ensurePolished
    // function isn't virtual.  This is a new virtual function which
    // calls QListWidget::ensurePolished.
    virtual void ensurePolished(void) const;

    bool itemVisible(uint row) const;
    QString text(uint row) const;

  public slots:
    void setCurrentItem(const QString& matchText, bool caseSensitive = true,
                        bool partialMatch = false);

  signals:
    void changeHelpText(QString);
    void accepted(int);
    void menuButtonPressed(int);
    void editButtonPressed(int);
    void deleteButtonPressed(int);
    void highlighted(int);

  private slots:
    void HandleItemSelectionChanged(void);

  private:
    QString helptext;
};

class MPUBLIC MythLabel: public QLabel
{
    Q_OBJECT

public:
    MythLabel(QWidget* parent = nullptr, const char* name = "MythLabel");
    MythLabel(const QString& text, QWidget* parent = nullptr,
              const char* name = "MythLabel");
};

class MPUBLIC MythGroupBox: public QGroupBox
{
    Q_OBJECT

public:
    MythGroupBox(QWidget* parent = nullptr, const char* name = "MythGroupBox");
    MythGroupBox(const QString& text, QWidget* parent = nullptr,
                 const char* name = "MythGroupBox");
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
