#ifndef STATUSBOX_H_
#define STATUSBOX_H_

#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"

class LayerSet;

class StatusBox : public MythDialog
{
    Q_OBJECT
  public:
    StatusBox(MythMainWindow *parent, const char *name = 0);
   ~StatusBox(void);

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

    MythMainWindow *my_parent;

    bool isBackend;
};

#endif
