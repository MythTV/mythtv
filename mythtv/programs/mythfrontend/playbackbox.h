#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class QLabel;
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
    void changed(QListViewItem *);

  private:
    QSqlDatabase *db;
    TV *tv;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
};

#endif
