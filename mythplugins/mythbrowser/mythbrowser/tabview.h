#ifndef TABVIEW_H
#define TABVIEW_H

#include "webpage.h"

#include <kapplication.h>
#include <kmainwindow.h>
#include <qtabwidget.h>
#include <qptrstack.h>
#include <qptrlist.h>
#include <qvaluestack.h>
#include <qdatetime.h>

#include "mythtv/mythdialogs.h"

using namespace std;

typedef QPtrStack<QWidget> WIDGET_HISTORY;
typedef QValueStack<QString> URL_HISTORY;

class TabView : public MythDialog
{
    Q_OBJECT

protected:
    virtual bool eventFilter(QObject* object, QEvent* event);

public:
    TabView(MythMainWindow *parent, const char *name, QStringList urls, 
            int zoom, int width, int height, WFlags flags);
    ~TabView();

signals:
    void menuPressed();
    void closeMenu();

public slots:
    void newUrlRequested(const KURL &url, const KParts::URLArgs &args);
    void newPage(QString URL);
    void newWindowRequested(const KURL &url, const KParts::URLArgs &args, 
                                  const KParts::WindowArgs &windowArgs, 
                                  KParts::ReadOnlyPart *&part);

private slots:
    void openMenu();
    void actionBack();
    void actionNextTab();
    void actionPrevTab();
    void actionRemoveTab();
    void actionZoomOut();
    void actionZoomIn();
    void cancelMenu();
    void actionAddBookmark();
    void finishAddBookmark(const char* group, const char* desc, const char* url);
    void handleMouseAction(QString action);
    
    // new URL dialog
    void showEnterURLDialog();

private:
    int z,w,h;
    WFlags f;

    int mouseEmulation, saveMouseEmulation;

    QStringList urls;
    QTabWidget *mytab;

    QCursor *mouse;

    bool menuIsOpen;
    MythPopupBox *menu;

    QWidget *hadFocus;

    QPtrList<WIDGET_HISTORY> widgetHistory;
    QPtrList<URL_HISTORY> urlHistory;

    ButtonState lastButtonState;
    int lastPosX; 
    int lastPosY; 
    int scrollSpeed;
    int scrollPage;
    int hideScrollbars;
    bool inputToggled;
    
    QString lastMouseAction;
    int     mouseKeyCount;
    QTime   lastMouseActionTime;
};

#endif // TABVIEW_H
