#ifndef SEARCH_H_
#define SEARCH_H_

#include <qdatetime.h>
#include <qhbox.h>
#include "libmyth/mythwidgets.h"
#include "proglist.h"

#include <pthread.h>

class QListViewItem;
class QLabel;
class ProgramInfo;

class Search : public MythDialog
{
    Q_OBJECT
  public:

    Search(MythMainWindow *parent, const char *name = 0);
   ~Search(void);
   
  signals:
    void dismissWindow();

  protected slots:
    void textChanged(void);
    void runSearch(void);
    void runDescSearch(void);
    void runNewList(void);
    void cancelClicked(void);

  private:
    MythRemoteLineEdit *m_title;
    MythPushButton *m_searchButton;
    MythPushButton *m_descButton;
    MythPushButton *m_newButton;
    MythPushButton *m_cancelButton;
};

#endif
