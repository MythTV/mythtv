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

#include "mythtv/mythcontext.h"

using namespace std;

TabView::TabView(QSqlDatabase *db, QStringList urls,
         int zoom, int width, int height, WFlags flags)
{
    menuIsOpen = false;

    myDb = db;
    mytab = new QTabWidget(this);
    mytab->setGeometry(0,0,width,height);
    QRect rect = mytab->childrenRect();

    z=zoom;
    w=width;
    h=height-rect.height();
    f=flags;

    for(QStringList::Iterator it = urls.begin(); it != urls.end(); ++it) {
        // is rect.height() really the height of the tab in pixels???
        WebPage *page = new WebPage(*it,z,w,h,f);
        mytab->addTab(page,*it);
        connect(page,SIGNAL(changeTitle(QString)),this,SLOT(changeTitle(QString)));
    }

    // Connect the context-menu
    connect(this,SIGNAL(menuPressed()),this,SLOT(openMenu()));
    connect(this,SIGNAL(closeMenu()),this,SLOT(cancelMenu()));

    setCursor(QCursor(Qt::ArrowCursor));
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
    menu->addLabel("MythBrowser Menu");

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

void TabView::actionBack()
{
    cancelMenu();

    ((WebPage*)mytab->currentPage())->back();
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

    printf("finish bookmark menu\n");

    if(groupStr.isEmpty() || urlStr.isEmpty())
        return;

    // Check if already in DB
    QSqlQuery query( "SELECT url FROM websites WHERE url='" + urlStr + "'", myDb);
    if (!query.isActive()) {
        cerr << "MythBookmarksConfig: Error in finding in DB" << endl;
        return;
    } else if( query.numRowsAffected() == 0 ) { // Insert if not yet in DB
        QSqlQuery query( "INSERT INTO websites (grp,dsc,url) VALUES( '" + groupStr + "', '" +
             descStr + "', '" + urlStr + "' );",myDb);
        if (!query.isActive()) {
            cerr << "MythBookmarksConfig: Error in inserting in DB" << endl;
        }
    }
}

void TabView::changeTitle(QString title)
{
    mytab->setTabLabel(mytab->currentPage(),title);
}

bool TabView::eventFilter(QObject* object, QEvent* event)
{
    object=object;

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = (QMouseEvent*)event;

        if(me->button() == Qt::RightButton) {
            if(menuIsOpen)
                emit closeMenu();
            else
                emit menuPressed();
            return true;
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

    QVGroupBox  *vbox = new QVGroupBox("Add New Website",this);
    lay->addWidget(vbox);

    QLabel *groupLabel = new QLabel("Group:", vbox);
    groupLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    group = new QLineEdit(vbox);

    QLabel *descLabel = new QLabel("Description:", vbox);
    descLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    desc = new QLineEdit(vbox);

    QLabel *urlLabel =new QLabel("URL:", vbox);
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
