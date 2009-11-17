#ifndef SEARCH_H_
#define SEARCH_H_

#include <QString>
#include <mythdialogs.h>
#include <mythwidgets.h>

#include "mythlistbox-qt3.h"

class QLabel;
class SearchListBoxItem: public Q3ListBoxText
{
  public:
    SearchListBoxItem(const QString &text, unsigned int id)
       : Q3ListBoxText(text), id(id) {};

    unsigned int getId() { return id; }

  private:
    virtual void paint(QPainter *p);
    unsigned int id;
};

class SearchDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    SearchDialog(MythMainWindow *parent, const char *name = 0); 
    ~SearchDialog();

    void getWhereClause(QString &whereClause);

  protected slots:

    void searchTextChanged(const QString &searchText);
    void itemSelected(int i);

  private:

    void runQuery(QString searchText);

    QLabel              *caption;
    Q3MythListBox       *listbox;  
    MythLineEdit        *searchText;
    QAbstractButton     *cancelButton;
    QAbstractButton     *okButton;

    QString              whereClause;
};

#endif
