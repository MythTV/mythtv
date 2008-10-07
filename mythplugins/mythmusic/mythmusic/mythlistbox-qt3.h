#ifndef _MYTHLISTBOX_H_
#define _MYTHLISTBOX_H_

#include <q3listbox.h>

class Q3MythListBox: public Q3ListBox
{
    Q_OBJECT
  public:
    Q3MythListBox(QWidget* parent);

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

#endif // _MYTHLISTBOX_H_
