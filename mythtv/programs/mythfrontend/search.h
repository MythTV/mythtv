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
    void channelChanged(void);
    void categoryChanged(void);
    void runSearch(void);
    void runDescSearch(void);
    void runNewList(void);
    void runChannelList(void);
    void runCategoryList(void);
    void runMovieList(void);
    void cancelClicked(void);

  private:
    MythRemoteLineEdit *m_title;
    MythPushButton *m_searchButton;
    MythPushButton *m_descButton;
    MythComboBox *m_channel;
    MythPushButton *m_chanButton;
    MythComboBox *m_category;
    MythPushButton *m_catButton;
    MythPushButton *m_movieButton;
    MythPushButton *m_newButton;
    MythPushButton *m_cancelButton;
};

#endif
