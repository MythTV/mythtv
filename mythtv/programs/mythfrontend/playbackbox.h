#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>

class QVBoxLayout;
class QButtonGroup;
class QSqlDatabase;
class QListViewItem;
class TV;

class PlaybackBox : public QDialog
{
    Q_OBJECT
  public:
    PlaybackBox(TV *ltv, QSqlDatabase *ldb, QWidget *parent = 0, 
                const char *name = 0);

    void Show();
  
  protected slots:
    void selected(QListViewItem *);

  private:
    QSqlDatabase *db;
    TV *tv;
};

#endif
