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

using namespace std;

TabView::TabView(QStringList urls, int zoom, int width, int height, 
                 WFlags flags)
{
    menuIsOpen = false;

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

    lastPosX = -1;
    lastPosY = -1;
    scrollSpeed = gContext->GetNumSetting("WebBrowserScrollSpeed", 4);
    scrollPage = gContext->GetNumSetting("WebBrowserScrollMode", 1);
    hideScrollbars = gContext->GetNumSetting("WebBrowserHideScrollbars", 0);
    if (scrollPage == 1) {
       scrollSpeed *= -1;  // scroll page vs background
    }

    for(QStringList::Iterator it = urls.begin(); it != urls.end(); ++it) {
        // is rect.height() really the height of the tab in pixels???
        WebPage *page = new WebPage(*it,z,w,h,f);
	connect(page,SIGNAL( newUrlRequested(const KURL &,const KParts::URLArgs&)),
           this, SLOT( newUrlRequested(const KURL &,const KParts::URLArgs &)));
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
	widgetHistory.append(currWidgetHistory);
	QValueStack<QString> *currUrlHistory = new QValueStack<QString>;
	urlHistory.append(currUrlHistory);
    }

    // Connect the context-menu
    connect(this,SIGNAL(menuPressed()),this,SLOT(openMenu()));
    connect(this,SIGNAL(closeMenu()),this,SLOT(cancelMenu()));

    qApp->setOverrideCursor(QCursor(Qt::ArrowCursor));
    qApp->installEventFilter(this);
    mytab->show();
}

TabView::~TabView()
{
}

void TabView::openMenu()
{
    QButton *temp;

    hadFocus = qApp->focusWidget();

    menu = new MyMythPopupBox(this,"popupMenu");
    menu->addLabel(tr("MythBrowser Menu"));

    if(mytab->count()==1) {
        temp = menu->addButton(tr("         Back         "), this, SLOT(actionBack()));
    } else {
        temp = menu->addButton(tr("       Next Tab       "), this, SLOT(actionNextTab()));
        menu->addButton(tr("       Prev Tab       "), this, SLOT(actionPrevTab()));
        menu->addButton(tr("         Back         "), this, SLOT(actionBack()));
    }
    menu->addButton(tr("Save Link in Bookmarks"), this, SLOT(actionAddBookmark()));
//    menu->addButton(tr("       New Tab        "), this, SLOT(actionNewTab()));
//    menu->addButton(tr("         STOP         "), this, SLOT(actionSTOP()));
    menu->addButton(tr("       zoom Out       "), this, SLOT(actionZoomOut()));
    menu->addButton(tr("       zoom In        "), this, SLOT(actionZoomIn()));
    temp->setFocus();

    menu->ShowPopup(this,SLOT(cancelMenu()));
    menuIsOpen=true;
}

void TabView::cancelMenu()
{
    if(menuIsOpen) {
        menu->hide();
//        delete menu;
//        menu=NULL;
        menuIsOpen=false;
	hadFocus->setFocus();
    }
    
}

void TabView::actionMouseEmulation()
{
    cancelMenu();

    mouseEmulation=~mouseEmulation;
    if(mouseEmulation) {
        mouse = new QCursor(Qt::ArrowCursor);
        mouse->setPos(w/2,h/2);
    } else {
        mouse->cleanup();
    }
}

void TabView::actionNextTab()
{
    cancelMenu();

    int nextpage =  mytab->currentPageIndex()+1;
    if(nextpage >= mytab->count())
        nextpage = 0;
    mytab->setCurrentPage(nextpage);
}


void TabView::actionPrevTab()
{
    cancelMenu();

    int nextpage =  mytab->currentPageIndex()-1;
    if(nextpage < 0)
        nextpage = mytab->count()-1;
    mytab->setCurrentPage(nextpage);
}

void TabView::actionSTOP()
{
    cancelMenu();
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

//void TabView::actionDoLeftClick()
//{
//    QPoint p=mouse->pos(); // current position
//
//    ((WebPage*)mytab->currentPage())->click(p);
//}

void TabView::actionAddBookmark()
{
    cancelMenu();
    PopupBox *popupBox = new PopupBox(this,
	    ((WebPage*)mytab->currentPage())->browser->baseURL().htmlURL());
    connect(popupBox, SIGNAL(finished(const char*,const char*,const char*)),
            this, SLOT(finishAddBookmark(const char*,const char*,const char*)));
    qApp->removeEventFilter(this);
    popupBox->exec();
    qApp->installEventFilter(this);
}

void TabView::finishAddBookmark(const char* group, const char* desc, const char* url)
{
    QString groupStr = QString(group);
    QString descStr = QString(desc);
    QString urlStr = QString(url);
    urlStr.stripWhiteSpace();
    if( urlStr.find("http://")==-1 )
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

void TabView::newUrlRequested(const KURL &url, const KParts::URLArgs &args)
{
    int index = mytab->currentPageIndex();
    QPtrStack<QWidget> *curWidgetHistory = widgetHistory.at(index);
    QValueStack<QString> *curUrlHistory = urlHistory.at(index);

    QWidget *curr = mytab->currentPage();
    curWidgetHistory->push(curr);
    curUrlHistory->push(mytab->label(index));

    WebPage *page = new WebPage(url.url(),args,((WebPage*)curr)->zoomFactor,w,h,f);
    mytab->insertTab(page,url.url(),index);
    mytab->removePage(curr);
    mytab->setCurrentPage(index);

    connect(page,SIGNAL( newUrlRequested(const KURL &,const KParts::URLArgs&)),
	    this, SLOT( newUrlRequested(const KURL &,const KParts::URLArgs &)));
}

bool TabView::eventFilter(QObject* object, QEvent* event)
{
    object=object;

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = (QMouseEvent*)event;

        lastButtonState = me->stateAfter(); 
        if(me->button() == Qt::RightButton) {
            if(menuIsOpen)
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

    if(menuIsOpen)
        return false;

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = (QKeyEvent*)event;

        QKeyEvent tabKey(ke->type(), Key_Tab, '\t', ke->state(),QString::null, ke->isAutoRepeat(), ke->count());
        QKeyEvent shiftTabKey(ke->type(), Key_Tab, '\t', ke->state()|Qt::ShiftButton,QString::null, ke->isAutoRepeat(), ke->count());

        switch(ke->key()) {
            case Qt::Key_Print:
                emit menuPressed();
                return true;
                break;
            case Qt::Key_Escape:
                qApp->restoreOverrideCursor();
                exit(0);
                break;
            case Qt::Key_Pause:
                actionNextTab();
                return true;
                break;
            case Qt::Key_Left:
                *ke = shiftTabKey;
                break;
            case Qt::Key_Right:
                *ke = tabKey;
                break;
        }
    }
    return false; // continue processing the event with QObject::event()
}

// ---------------------------------------------------

MyMythPopupBox::MyMythPopupBox(MythMainWindow *parent, const char *name)
    : MythPopupBox(parent,name)
{
    setCursor(QCursor(Qt::ArrowCursor));
}

// ---------------------------------------------------

PopupBox::PopupBox(QWidget *parent, QString deflt)
        : QDialog(parent, 0, true, WType_Popup)
{
    setPalette(parent->palette());
    setFont(parent->font());

    QVBoxLayout *lay  = new QVBoxLayout(this, 5);

    QVGroupBox  *vbox = new QVGroupBox(tr("Add New Website"),this);
    lay->addWidget(vbox);

    QLabel *groupLabel = new QLabel(tr("Group:"), vbox);
    groupLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    group = new QLineEdit(vbox);

    QLabel *descLabel = new QLabel(tr("Description:"), vbox);
    descLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    desc = new QLineEdit(vbox);

    QLabel *urlLabel =new QLabel(tr("URL:"), vbox);
    urlLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    url = new QLineEdit(vbox);
    url->setText(deflt);

    QHBoxLayout *hbox = new QHBoxLayout(lay);

    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    MythPushButton *okButton = new MythPushButton("&Ok", this);
    okButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hbox->addWidget(okButton);

    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    connect(okButton, SIGNAL(clicked()), this, SLOT(slotOkClicked()));

    gContext->ThemeWidget(this);
}

PopupBox::~PopupBox()
{
}

void PopupBox::slotOkClicked()
{
    emit finished(group->text(),desc->text(),url->text());
    close();
}
