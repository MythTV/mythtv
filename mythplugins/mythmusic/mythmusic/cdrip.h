#ifndef CDRIP_H_
#define CDRIP_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qsizepolicy.h>

#include "metadata.h"

#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

class QSqlDatabase;
class QFrame;
class QVBoxLayout;
class QPushButton;
class QLabel;
class QProgressBar;
class Encoder;

class Ripper : public MythDialog
{
    Q_OBJECT
  public:
    Ripper(QSqlDatabase *ldb, MythMainWindow *parent, const char *name = 0);
   ~Ripper(void);
  
    QSizePolicy sizePolicy(void);

  protected slots:
    void ripthedisc();
    void artistChanged(const QString &newartist);
    void albumChanged(const QString &newalbum);
    void genreChanged(const QString &newgenre);
    void tableChanged(int row, int col);
    void reject();

  private:
    void fillComboBox (MythComboBox &, const QString &);

    int ripTrack(QString &cddevice, Encoder *encoder, int tracknum);
    QString fixFileToken(QString token);
    void handleFileTokens(QString &filename, Metadata *track);
    void ejectCD(QString &cddev);

    QSqlDatabase *db;
    QVBoxLayout *bigvb;
    QFrame *firstdiag;

    MythComboBox *artistedit;
    MythLineEdit *albumedit;
    MythComboBox *genreedit;

    MythTable *table;

    MythButtonGroup *qualitygroup;

    QLabel *statusline;
    QProgressBar *overall;
    QProgressBar *current;

    int totaltracks;

    QString albumname, artistname, genrename;
};

#endif
