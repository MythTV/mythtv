#ifndef TABVIEW_H
#define TABVIEW_H

#include "webpage.h"

#include <kapplication.h>
#include <kmainwindow.h>
#include <qtabwidget.h>
#include <qptrstack.h>
#include <qptrlist.h>
#include <qvaluestack.h>

#include "mythtv/mythdialogs.h"

typedef QPtrStack<QWidget> WIDGET_HISTORY;
typedef QValueStack<QString> URL_HISTORY;

class MyMythPopupBox : public MythPopupBox
{
    Q_OBJECT

public:
    MyMythPopupBox(MythMainWindow *parent, const char *name = 0);
//    ~MyMythPopupBox();
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
//  void openURLRequested(KURL url,KParts::URLArgs args);
    void newUrlRequested(const KURL &url, const KParts::URLArgs &args);

private slots:
    void openMenu();
    void actionBack();
    void actionNextTab();
    void actionPrevTab();
    void actionMouseEmulation();
//  void actionDoLeftClick();
    void actionSTOP();
    void actionZoomOut();
    void actionZoomIn();
    void cancelMenu();
    void actionAddBookmark();
    void finishAddBookmark(const char* group, const char* desc, const char* url);

private:
    int z,w,h;
    WFlags f;

    int mouseEmulation, saveMouseEmulation;

    QStringList urls;
    QTabWidget *mytab;

    QCursor *mouse;

    bool menuIsOpen;
    MyMythPopupBox *menu;

    QWidget *hadFocus;

    QPtrList<WIDGET_HISTORY> widgetHistory;
    QPtrList<URL_HISTORY> urlHistory;

    ButtonState lastButtonState;
    int lastPosX; 
    int lastPosY; 
    int scrollSpeed;
    int scrollPage;
    int hideScrollbars;
};

class PopupBox : public QDialog
{
    Q_OBJECT

protected:
//  virtual bool eventFilter(QObject* object, QEvent* event);

public:
    PopupBox(QWidget *parent, QString defltUrl);
    ~PopupBox();

signals:
    void finished(const char* group, const char* desc,const char* url);
//    void cancelMenu();

private slots:
    void slotOkClicked();

private:
    QLineEdit* group;
    QLineEdit* desc;
    QLineEdit* url;
};

#endif // TABVIEW_H
