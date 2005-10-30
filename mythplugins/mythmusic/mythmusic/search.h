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

  signals:

    void done();

  private:

    void runQuery(QString searchText);
    
    QVBoxLayout         *vbox;
    QLabel              *caption;
    MythListBox         *listbox;  
    MythLineEdit        *searchText;
    
    QString              whereClause;
};

// For some reason, pressing "OK" on my Hauppauge remote does *NOT* cause the
// returnPressed() signal to be emitted. Hence the dirty hack below :(
class MyLineEdit : public MythLineEdit
{
  public:

    MyLineEdit(QWidget* parent) : MythLineEdit(parent) {};
  
  protected:

    virtual void keyPressEvent( QKeyEvent *e )
    {
        MythLineEdit::keyPressEvent(e);
        if (e->key() == Qt::Key_Return)
            emit returnPressed();
    }
};

#endif
