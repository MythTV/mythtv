#ifndef SEARCH_H_
#define SEARCH_H_

#include <qregexp.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythwidgets.h>

class SearchListBoxItem: public QListBoxText
{
  public:
    SearchListBoxItem(const QString &text, unsigned int id)
       : QListBoxText(text), id(id) {};

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
    void okPressed(void);
    void cancelPressed(void);

  private:

    void runQuery(QString searchText);

    QLabel              *caption;
    MythListBox         *listbox;  
    MythLineEdit        *searchText;
    QButton             *cancelButton;
    QButton             *okButton;

    QString              whereClause;
};

#endif
