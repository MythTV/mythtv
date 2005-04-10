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

class MyMythPopupBox : public MythPopupBox
{
    Q_OBJECT

public:
    MyMythPopupBox(MythMainWindow *parent, const char *name = 0);
};

class TabView : public MythMainWindow
{
    Q_OBJECT

protected:
    virtual bool eventFilter(QObject* object, QEvent* event);

public:
    TabView(QStringList urls, int zoom, int width, int height, WFlags flags);
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
    void closeEnterURLDialog();
    void enterURLOkPressed();

private:
    int z,w,h;
    WFlags f;

    int mouseEmulation, saveMouseEmulation;

    QStringList urls;
    QTabWidget *mytab;

    QCursor *mouse;

    bool menuIsOpen;
    MyMythPopupBox *menu;
    
    MyMythPopupBox *enterURL;
    MythRemoteLineEdit *URLeditor;
    
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

class PopupBox : public QDialog
{
    Q_OBJECT

public:
    PopupBox(QWidget *parent, QString defltUrl);
    ~PopupBox();

signals:
    void finished(const char* group, const char* desc,const char* url);

private slots:
    void slotOkClicked();

private:
    MythRemoteLineEdit* group;
    MythRemoteLineEdit* desc;
    MythRemoteLineEdit* url;
};

#endif // TABVIEW_H
