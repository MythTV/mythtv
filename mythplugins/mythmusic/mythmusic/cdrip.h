#ifndef CDRIP_H_
#define CDRIP_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qsizepolicy.h>

#include "metadata.h"

#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

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
    Ripper(MythMainWindow *parent, const char *name = 0);
   ~Ripper(void);
  
    QSizePolicy sizePolicy(void);
    bool somethingWasRipped();

  protected slots:
    void ripthedisc();
    void artistChanged(const QString &newartist);
    void albumChanged(const QString &newalbum);
    void genreChanged(const QString &newgenre);
    void compilationChanged(bool state);
    void tableChanged(int row, int col);
    void switchTitlesAndArtists();
    void reject();

  private:
    void fillComboBox (MythComboBox &, const QString &);

    int ripTrack(QString &cddevice, Encoder *encoder, int tracknum);
    static QString fixFileToken(QString token);
    void handleFileTokens(QString &filename, Metadata *track);
    void ejectCD(QString &cddev);

    QVBoxLayout *bigvb;
    QFrame *firstdiag;

    MythComboBox *artistedit;
    MythLineEdit *albumedit;
    MythComboBox *genreedit;
    MythCheckBox *compilation;
    MythPushButton *switchtitleartist;

    MythTable *table;

    MythButtonGroup *qualitygroup;

    QLabel *statusline;
    QProgressBar *overall;
    QProgressBar *current;

    int totaltracks;
    long int totalSectors;
    long int totalSectorsDone;

    QString albumname, artistname, genrename;

    bool somethingwasripped;
};

#endif
