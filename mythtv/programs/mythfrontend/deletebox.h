#ifndef DELETEBOX_H_
#define DELETEBOX_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class MyListView;
class QLabel;
class QProgressBar;
class TV;

class DeleteBox : public QDialog
{
    Q_OBJECT
  public:
    DeleteBox(QString prefix, TV *ltv, QSqlDatabase *ldb, QWidget *parent = 0, 
              const char *name = 0);

    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);

  private:
    void GetFreeSpaceStats(int &totalspace, int &usedspace); 
    void UpdateProgressBar(void);

    QSqlDatabase *db;
    TV *tv;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *pixlabel;

    MyListView *listview;

    QLabel *freespace;
    QProgressBar *progressbar;
 
    QString fileprefix;
};

#endif
