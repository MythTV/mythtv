#ifndef CDRIP_H_
#define CDRIP_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qsizepolicy.h>

#include "metadata.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

class QSqlDatabase;
class QFrame;
class QVBoxLayout;
class QPushButton;
class QLabel;
class QProgressBar;

class Ripper : public MythDialog
{
    Q_OBJECT
  public:
    Ripper(MythContext *context, QSqlDatabase *ldb, QWidget *parent = 0, 
           const char *name = 0);
   ~Ripper(void);
  
    QSizePolicy sizePolicy(void);

  protected slots:
    void ripthedisc();
    void artistChanged(const QString &newartist);
    void albumChanged(const QString &newalbum);
    void tableChanged(int row, int col);

  private:
    int ripTrack(char *cddevice, char *outputfilename, int tracknum);
    void fixFilename(char *filename, const char *addition);

    QSqlDatabase *db;
    QVBoxLayout *bigvb;
    QFrame *firstdiag;

    MythLineEdit *artistedit;
    MythLineEdit *albumedit;

    MythTable *table;

    MythButtonGroup *qualitygroup;

    QLabel *statusline;
    QProgressBar *overall;
    QProgressBar *current;

    int totaltracks;

    QString albumname, artistname;
};

#endif
