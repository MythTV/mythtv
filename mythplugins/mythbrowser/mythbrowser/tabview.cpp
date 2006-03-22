/*
 * tabview.cpp
 *
 * Copyright (C) 2003
 *
 * Author:
 * - Philippe Cattin <cattin@vision.ee.ethz.ch>
 *
 * Bugfixes from:
 *
 * Translations by:
 *
 */
#include "tabview.h"

#include <stdlib.h>
#include <qtabwidget.h>
#include <qstringlist.h>
#include <qlistbox.h>
#include <qcursor.h>
#include <qvgroupbox.h>
#include <qlayout.h>

#include <kglobal.h>
#include <klocale.h>
#include <kaccel.h>
#include <kio/netaccess.h>
#include <kconfig.h>
#include <kurl.h>
#include <kurlrequesterdlg.h>
#include <kstdaccel.h>
#include <kaction.h>
#include <kstdaction.h>
#include <khtml_part.h>
#include <khtmlview.h>

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include "mythtv/mythwidgets.h"

using namespace std;

TabView::TabView(MythMainWindow *parent, const char *name, QStringList urls, 
                 int zoom, int width, int height, WFlags flags)
    :MythDialog(parent, name)
{
    setNoErase();
    menuIsOpen = false;
    menu = NULL;

    mytab = new QTabWidget(this);
    mytab->setGeometry(0,0,width,height);
    QRect rect = mytab->childrenRect();

    z=zoom;
    w=width;
    h=height-rect.height();
    f=flags;

    // Qt doesn't give you mousebutton states from QWheelEvent->state()
    // so we have to remember the last mousebutton press/release state
    lastButtonState = (ButtonState)0;

    inputToggled = false;
    lastPosX = -1;
    lastPosY = -1;
    scrollSpeed = gContext->GetNumSetting("WebBrowserScrollSpeed", 4);
    scrollPage = gContext->GetNumSetting("WebBrowserScrollMode", 1);
    hideScrollbars = gContext->GetNumSetting("WebBrowserHideScrollbars", 0);
    if (scrollPage == 1) {
       scrollSpeed *= -1;  // scroll page vs background
    }

    widgetHistory.setAutoDelete(true);
    
    for (QStringList::Iterator it = urls.begin(); it != urls.end(); ++it) {
        WebPage *page = new WebPage(*it,z,f);
        
        connect(page,SIGNAL( newUrlRequested(const KURL &,const KParts::URLArgs &)),
           this, SLOT( newUrlRequested(const KURL &,const KParts::URLArgs &)));
        
        connect(page, SIGNAL( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                              const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ), 
                this, SLOT( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ) );

        
        mytab->addTab(page,*it);

/* moved this into show event processing
        // Hide Scrollbars - Why doesn't this work??? TSH 1/20/04
        if (hideScrollbars) {
            KHTMLView* view = page->browser->view();
            view->setVScrollBarMode(QScrollView::AlwaysOff);
            view->setHScrollBarMode(QScrollView::AlwaysOff);
        }
*/

        QPtrStack<QWidget> *currWidgetHistory = new QPtrStack<QWidget>;
        currWidgetHistory->setAutoDelete(true);
        widgetHistory.append(currWidgetHistory);
        
        QValueStack<QString> *currUrlHistory = new QValueStack<QString>;
        urlHistory.append(currUrlHistory);
    }

    // Connect the context-menu
    connect(this, SIGNAL(menuPressed()), this, SLOT(openMenu()));
    connect(this, SIGNAL(closeMenu()), this, SLOT(cancelMenu()));

    qApp->restoreOverrideCursor();
    qApp->installEventFilter(this);
}

TabView::~TabView()
{
}

void TabView::openMenu()
{
    QButton *temp;

    hadFocus = qApp->focusWidget();

    menu = new MythPopupBox(GetMythMainWindow(),"popupMenu");
    menu->addLabel(tr("MythBrowser Menu"));

    if(mytab->count()==1) 
    {
        temp = menu->addButton(tr("Back"), this, SLOT(actionBack()));
    } 
    else 
    {
        temp = menu->addButton(tr("Next Tab"), this, SLOT(actionNextTab()));
        menu->addButton(tr("Prev Tab"), this, SLOT(actionPrevTab()));
        menu->addButton(tr("Remove Tab"), this, SLOT(actionRemoveTab()));
        menu->addButton(tr("Back"), this, SLOT(actionBack()));
    }
    menu->addButton(tr("Save Link in Bookmarks"), this, SLOT(actionAddBookmark()));
    menu->addButton(tr("Zoom Out"), this, SLOT(actionZoomOut()));
    menu->addButton(tr("Zoom In"), this, SLOT(actionZoomIn()));
    temp->setFocus();

    menu->ShowPopup(this, SLOT(cancelMenu()));
    menuIsOpen = true;
}

void TabView::cancelMenu()
{
    if (menuIsOpen) 
    {
        menu->hide();
        delete menu;
        menu=NULL;
        menuIsOpen=false;
        hadFocus->setFocus();
    }
}

void TabView::handleMouseAction(QString action)
{
    int step = 5;

    // speed up mouse movement if the same key is held down
    if (action == lastMouseAction && 
           lastMouseActionTime.msecsTo(QTime::currentTime()) < 500) {
        lastMouseActionTime = QTime::currentTime();
        mouseKeyCount++;
        if (mouseKeyCount > 5)
            step = 25;
    }
    else {    
        lastMouseAction = action;
        lastMouseActionTime = QTime::currentTime();
        mouseKeyCount = 1;
    }
    
    if (action == "MOUSEUP") {    
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x(), curPos.y() - step);
    }  
    else if (action == "MOUSELEFT") {    
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x() - step, curPos.y());
    }
    else if (action == "MOUSERIGHT") {    
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x() + step, curPos.y());
    }
    else if (action == "MOUSEDOWN") {    
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x(), curPos.y() + step);
    }
    else if (action == "MOUSELEFTBUTTON") {    
        QPoint curPos = mouse->pos();
        QWidget *widget = QApplication::widgetAt(curPos, TRUE);
        
        if (widget) {
            curPos = widget->mapFromGlobal(curPos);
        
            QMouseEvent *me = new QMouseEvent(QEvent::MouseButtonPress, curPos, 
                                Qt::LeftButton, Qt::LeftButton);
            QApplication::postEvent(widget, me);                        
                        
            me = new QMouseEvent(QEvent::MouseButtonRelease, curPos, 
                                Qt::LeftButton, Qt::NoButton);
            QApplication::postEvent(widget, me); 
        }
    }
}

void TabView::actionNextTab()
{
    cancelMenu();

    int nextpage =  mytab->currentPageIndex() + 1;
    if(nextpage >= mytab->count())
        nextpage = 0;
    mytab->setCurrentPage(nextpage);
}

void TabView::actionPrevTab()
{
    cancelMenu();

    int nextpage =  mytab->currentPageIndex() - 1;
    if(nextpage < 0)
        nextpage = mytab->count() - 1;
    mytab->setCurrentPage(nextpage);
}

void TabView::actionRemoveTab()
{
    cancelMenu();
    
    // don't remove the last tab
    if (mytab->count() <= 1)
        return;
        
    int index = mytab->currentPageIndex();
    
    // delete web pages stored in history 
    widgetHistory.remove(index);
    urlHistory.remove(index);
    
    // delete current web page
    QWidget *curr = mytab->currentPage();
    mytab->removePage(curr);
    delete curr;
    
    // move to next/last tab
    if (index >= mytab->count())
        index = mytab->count() - 1;
    mytab->setCurrentPage(index);
}

void TabView::actionZoomOut()
{
    cancelMenu();

    ((WebPage*)mytab->currentPage())->zoomOut();
}

void TabView::actionZoomIn()
{
    cancelMenu();

    ((WebPage*)mytab->currentPage())->zoomIn();
}

void TabView::actionAddBookmark()
{
    cancelMenu();
    hadFocus = qApp->focusWidget();

    MythPopupBox *popup = new MythPopupBox(GetMythMainWindow(),
                                           "addbookmark_popup");

    QLabel *caption = popup->addLabel(tr("Add New Bookmark"), MythPopupBox::Medium);
    caption->setAlignment(Qt::AlignCenter);

    popup->addLabel(tr("Group:"), MythPopupBox::Small);
    MythRemoteLineEdit *group = new MythRemoteLineEdit(popup); 
    popup->addWidget(group);

    popup->addLabel(tr("Description:"), MythPopupBox::Small);
    MythRemoteLineEdit *desc = new MythRemoteLineEdit(popup); 
    popup->addWidget(desc);

    popup->addLabel(tr("URL:"), MythPopupBox::Small);
    MythRemoteLineEdit *url = new MythRemoteLineEdit(popup); 
    url->setText(((WebPage*)mytab->currentPage())->browser->baseURL().htmlURL());
    popup->addWidget(url);

    popup->addButton(tr("OK"));
    popup->addButton(tr("Cancel"));

    qApp->removeEventFilter(this);
    int res = popup->ExecPopup();
    qApp->installEventFilter(this);

    if (res == 0)
    {
        QString sGroup = group->text();
        QString sDesc = desc->text();
        QString sUrl = url->text();

        finishAddBookmark(sGroup, sDesc, sUrl);
    }

    delete popup;
    
    hadFocus->setFocus();
}

void TabView::finishAddBookmark(const char* group, const char* desc, const char* url)
{
    QString groupStr = QString(group);
    QString descStr = QString(desc);
    QString urlStr = QString(url);
    urlStr.stripWhiteSpace();
    if( urlStr.find("http://") == -1 && urlStr.find("file:/") == -1)
        urlStr.prepend("http://");

    if(groupStr.isEmpty() || urlStr.isEmpty())
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO websites (grp, dsc, url) VALUES(:GROUP, :DESC, :URL);");
    query.bindValue(":GROUP",groupStr.utf8());
    query.bindValue(":DESC",descStr.utf8());
    query.bindValue(":URL",urlStr.utf8());
    if (!query.exec()) {
        cerr << "MythBookmarksConfig: Error in inserting in DB" << endl;
    }
}

void TabView::actionBack()
{
    cancelMenu();

    int index = mytab->currentPageIndex();
    QPtrStack<QWidget> *curWidgetHistory = widgetHistory.at(index);
    if (!curWidgetHistory->isEmpty()) {
        QValueStack<QString> *curUrlHistory = urlHistory.at(index);

        QWidget *curr = mytab->currentPage();
        mytab->insertTab(curWidgetHistory->pop(),curUrlHistory->pop(),index);
        mytab->removePage(curr);
        mytab->setCurrentPage(index);

        // disconnect(curr,...);
        delete curr;
    }
}

void TabView::showEnterURLDialog()
{
    hadFocus = qApp->focusWidget();

    MythPopupBox *popup = new MythPopupBox(GetMythMainWindow(), "enterURL");
    popup->addLabel(tr("Enter URL"));

    MythRemoteLineEdit *editor = new MythRemoteLineEdit(popup);
    popup->addWidget(editor);
    editor->setFocus(); 

    popup->addButton(tr("OK"));
    popup->addButton(tr("Cancel"));

    qApp->removeEventFilter(this);
    int res = popup->ExecPopup();
    qApp->installEventFilter(this);

    if (res == 0)
    {
        QString sURL = editor->text();
        if (!sURL.startsWith("http://") && !sURL.startsWith("file:/"))
            sURL = "http://" + sURL;

        newPage(sURL);
    }

    delete popup;

    hadFocus->setFocus();
}

void TabView::newPage(QString sURL)
{
    int index = mytab->currentPageIndex();
    
    WebPage *page = new WebPage(sURL,z,f);
    
    connect(page,SIGNAL( newUrlRequested(const KURL &,const KParts::URLArgs&)),
        this, SLOT( newUrlRequested(const KURL &,const KParts::URLArgs &)));

    connect(page, SIGNAL( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ), 
            this, SLOT( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ) );

    mytab->insertTab(page, sURL, index);
    mytab->setCurrentPage(index);

    QPtrStack<QWidget> *currWidgetHistory = new QPtrStack<QWidget>;
    currWidgetHistory->setAutoDelete(true);
    widgetHistory.append(currWidgetHistory);
    
    QValueStack<QString> *currUrlHistory = new QValueStack<QString>;
    urlHistory.append(currUrlHistory);
    
    hadFocus = page;
}

void TabView::newUrlRequested(const KURL &url, const KParts::URLArgs &args)
{
    int index = mytab->currentPageIndex();
    
    if (mytab->tabLabel(mytab->currentPage()) == "")
    {
        // if the tab title is blank then a new blank web page has already been 
        // created for a popup window just need to open the URL 
        ((WebPage*)mytab->currentPage())->openURL(url.url());
        mytab->setTabLabel(mytab->currentPage(), url.url());
    }
    else
    {    
        QPtrStack<QWidget> *curWidgetHistory = widgetHistory.at(index);
        QValueStack<QString> *curUrlHistory = urlHistory.at(index);
    
        QWidget *curr = mytab->currentPage();
        curWidgetHistory->push(curr);
        curUrlHistory->push(mytab->label(index));
    
        WebPage *page = new WebPage(url.url(),args,((WebPage*)curr)->zoomFactor,f);

        mytab->insertTab(page,url.url(),index);
        mytab->removePage(curr);
        mytab->setCurrentPage(index);
        
        connect(page,SIGNAL( newUrlRequested(const KURL &,const KParts::URLArgs &)),
                this, SLOT( newUrlRequested(const KURL &,const KParts::URLArgs &)));
        
        connect(page, SIGNAL( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                              const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ), 
                this, SLOT( newWindowRequested( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ) );
    }
}

void TabView::newWindowRequested(const KURL &, const KParts::URLArgs &, 
                                 const KParts::WindowArgs &, 
                                 KParts::ReadOnlyPart *&part)
{
    newPage("");
    part = ((WebPage*)mytab->currentPage())->browser->currentFrame();
}

bool TabView::eventFilter(QObject* object, QEvent* event)
{
    object=object;

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = (QMouseEvent*)event;

        lastButtonState = me->stateAfter(); 
        if (me->button() == Qt::RightButton) {
            if (menuIsOpen)
                emit closeMenu();
            else
                emit menuPressed();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* me = (QMouseEvent*)event;
        lastButtonState = me->stateAfter(); 
    }

    QScrollView *view=((WebPage*)mytab->currentPage())->browser->view();
    if (event->type() == QEvent::Show) {
        // hide scrollbars - kind of a hack to do it on a show event but 
        // for some reason I can't get it to work upon creation of the page 
        //   - TSH
        if (hideScrollbars) {
            QScrollView *view=((WebPage*)mytab->currentPage())->browser->view();
            view->setVScrollBarMode(QScrollView::AlwaysOff);
            view->setHScrollBarMode(QScrollView::AlwaysOff);
        }
    }

    // MouseMove while middlebutton pressed pans page
    int deltax = 0, deltay = 0;
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* me = (QMouseEvent*)event;
        lastButtonState = me->stateAfter(); 
        deltax = me->globalX() - lastPosX;
        deltay = me->globalY() - lastPosY;
        deltax *= scrollSpeed;
        deltay *= scrollSpeed;
        if (lastPosX != -1 
//            && (lastButtonState & (LeftButton|AltButton)) 
            && (lastButtonState & (MidButton|AltButton)) 
            && !view->isHorizontalSliderPressed()
            && !view->isVerticalSliderPressed() ) 
        {
            view->scrollBy(deltax, deltay);
        }
        lastPosX = me->globalX();
        lastPosY = me->globalY();
    }

    // MouseWheel scrolling while middlebutton/ctrlkey pressed to zoom in/out
    if (event->type() == QEvent::Wheel) {
        QWheelEvent* we = (QWheelEvent*)event;
        if (lastButtonState & MidButton || we->state() & AltButton) {
            if (we->delta() > 0) {
                we->accept();
                ((WebPage*)mytab->currentPage())->zoomIn();
                return true;
            } else if (we->delta() < 0)  {
                we->accept();
                ((WebPage*)mytab->currentPage())->zoomOut();
                return true;
            }
        }
        else if(we->orientation()==Qt::Horizontal)
        {
            // add back button
            if(we->delta()>0)
            {
                actionBack();
                return(true);
            }
        }
    }

    if (menuIsOpen)
        return false;
    
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = (QKeyEvent*)event;

        QKeyEvent tabKey(ke->type(), Key_Tab, '\t', ke->state(),QString::null, ke->isAutoRepeat(), ke->count());
        QKeyEvent shiftTabKey(ke->type(), Key_Tab, '\t', ke->state()|Qt::ShiftButton,QString::null, ke->isAutoRepeat(), ke->count());
        QKeyEvent returnKey(ke->type(), Key_Return, '\r', ke->state(),QString::null, ke->isAutoRepeat(), ke->count());

        QStringList actions;
        GetMythMainWindow()->TranslateKeyPress("Browser", ke, actions);
   
        bool handled = false;
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "TOGGLEINPUT") {
                inputToggled = !inputToggled;
                return true;
            }    
            
            // if input is toggled all input goes to the web page
            if (inputToggled)
                return false;
            
            if (action == "NEXTTAB") {
                actionNextTab();
                return true;
            }    
            else if (action == "DELETETAB") {
                actionRemoveTab();
                return true;
            }

            else if (action == "BACK") {
                actionBack();
                return true;
            }
            else if (action == "FOLLOWLINK") {
                *ke = returnKey;
            }
            else if (action == "ZOOMIN") {
                actionZoomIn();
                return true;
            }    
            else if (action == "ZOOMOUT") {
                actionZoomOut();
                return true;
            }    
            else if (action == "MOUSEUP" || action == "MOUSEDOWN" || 
                     action == "MOUSELEFT" || action == "MOUSERIGHT" ||
                     action == "MOUSELEFTBUTTON") {    
                handleMouseAction(action);         
                return true;
            }
            else if (action == "PREVIOUSLINK")
                *ke = shiftTabKey;
            else if (action == "NEXTLINK")
                *ke = tabKey;
            else if (action == "ESCAPE")
                exit(0);
            else if (action == "INFO") {
                showEnterURLDialog();
                return true;
            }    
            else if (action == "MENU") {
                emit menuPressed();
                return true;
            }
        }
    }
    return false; // continue processing the event with QObject::event()
}
