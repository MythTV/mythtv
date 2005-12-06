#ifndef STATUSBOX_H_
#define STATUSBOX_H_

#include <qstringlist.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"

typedef QMap<QString, unsigned int> recprof2bps_t;

class LayerSet;

class StatusBox : public MythDialog
{
    Q_OBJECT
  public:
    StatusBox(MythMainWindow *parent, const char *name = 0);
   ~StatusBox(void);

   bool IsErrored() const { return errored; }

  protected slots:

  protected:
    void keyPressEvent(QKeyEvent *e);
    void paintEvent(QPaintEvent *e);

  private:
    void updateTopBar();
    void updateSelector();
    void updateContent();
    void LoadTheme();
    void doListingsStatus();
    void doTunerStatus();
    void doLogEntries();
    void doJobQueueStatus();
    void doMachineStatus();
    void clicked();
    void setHelpText();
    void getActualRecordedBPS(QString hostnames);

    XMLParse *theme;
    QDomElement xmldata;
    QRect TopRect, SelectRect, ContentRect;
    UITextType *heading, *helptext;
    UIListType *icon_list, *list_area;
    LayerSet *selector, *topbar, *content;

    int max_icons;

    bool inContent, doScroll;
    int contentTotalLines;
    int contentSize;
    int contentPos;
    int contentMid;
    int min_level;
    QString dateFormat, timeFormat, timeDateFormat;

    QMap<int, QString> contentLines;
    QMap<int, QString> contentDetail;
    QMap<int, QString> contentFont;
    QMap<int, QString> contentData;
    recprof2bps_t      recordingProfilesBPS;

    MythMainWindow *my_parent;

    bool isBackend;
    bool errored;
};

#endif
