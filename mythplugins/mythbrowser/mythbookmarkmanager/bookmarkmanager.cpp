/*
 * bookmarkmanager.cpp
 *
 * Copyright (C) 2003
 *
 * This code is originally based on MythNews code from Renchi Raju
 *
 * Author:
 * - Philippe Cattin <cattin@vision.ee.ethz.ch>
 *
 * Bugfixes from:
 * - Jim Radford
 *
 * Translations by:
 *
 */
#include <iostream>

#include <qptrlist.h>
#include <qstring.h>
#include <qfile.h>
#include <qdom.h>
#include <qlayout.h>
#include <qcursor.h>
#include <qlabel.h>
#include <qvgroupbox.h>
#include <qtimer.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "bookmarkmanager.h"

using namespace std;

// ---------------------------------------------------

class BookmarkItem
{
public:
    typedef QPtrList<BookmarkItem> List;

    QString group;
    QString desc;
    QString url;
};

// ---------------------------------------------------

class BookmarkGroup
{
public:

    typedef QPtrList<BookmarkGroup> List;

    QString name;
    BookmarkItem::List  siteList;

    BookmarkGroup() {
        siteList.setAutoDelete(true);
    }

    ~BookmarkGroup() {
        siteList.clear();
    }

    void clear() {
        siteList.clear();
    };
};

// ---------------------------------------------------

class BookmarkViewItem : public QListViewItem
{
public:

    BookmarkViewItem(QListViewItem* parent, BookmarkItem* siteItem)
        : QListViewItem(parent, siteItem->desc, siteItem->url)
        {
        myBookmarkSite = siteItem;
    }

    ~BookmarkViewItem()
        {}

    BookmarkItem* myBookmarkSite;
};

// ---------------------------------------------------

PopupBox::PopupBox(QWidget *parent)
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

    QHBoxLayout *hbox = new QHBoxLayout(lay);

    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    MythPushButton *okButton = new MythPushButton(tr("&Ok"), this);
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

// ---------------------------------------------------

 class BookmarkConfigPriv
{
public:

    BookmarkGroup::List groupList;
    QStringList selectedSitesList;

    BookmarkConfigPriv() {
        groupList.setAutoDelete(true);
    }

    ~BookmarkConfigPriv() {
        groupList.clear();
    }
};

// ---------------------------------------------------

BookmarksConfig::BookmarksConfig(MythMainWindow *parent,
                                 const char *name)
    : MythDialog(parent, name)
{
    setPalette(parent->palette());

//    myTree = new BookmarkConfigPriv;

    // Create the database if not exists
    QString queryString( "CREATE TABLE IF NOT EXISTS websites "
                         "( grp VARCHAR(255) NOT NULL, dsc VARCHAR(255),"
             " url VARCHAR(255) NOT NULL PRIMARY KEY,"
                         "  updated INT UNSIGNED );");
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(queryString)) {
        cerr << "MythBookmarksConfig: Error in creating sql table" << endl;
    }

    // List View of Tabs and their bookmaks
    myBookmarksView = new MythListView(this);
    myBookmarksView->header()->hide();
    myBookmarksView->addColumn("Sites");
    myBookmarksView->setRootIsDecorated(true);
    myBookmarksView->addColumn("URL",-1);

    populateListView();
    setupView();

    setCursor(QCursor(Qt::ArrowCursor));
}


BookmarksConfig::~BookmarksConfig()
{
//    delete myTree;
    gContext->SaveSetting("WebBrowserZoomLevel", zoom->value());
    gContext->SaveSetting("WebBrowserCommand", browser->text());
    gContext->SaveSetting("WebBrowserScrollMode", 
                                               scrollmode->isChecked()?1:0);
    gContext->SaveSetting("WebBrowserScrollSpeed", scrollspeed->value());
    gContext->SaveSetting("WebBrowserHideScrollbars", 
                                               hidescrollbars->isChecked()?1:0);
}


void BookmarksConfig::populateListView()
{
    BookmarkConfigPriv* myTree = new BookmarkConfigPriv;

    myTree->groupList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT grp, dsc, url FROM websites ORDER BY grp");

    if (!query.isActive()) {
        cerr << "MythBrowserConfig: Error in loading from DB" << endl;
    } else {
        BookmarkGroup *group = new BookmarkGroup();
        group->name="Empty";
        while( query.next() ) {
            if( QString::compare(group->name,query.value(0).toString())!=0 ) {
                group = new BookmarkGroup();
                group->name = query.value(0).toString();
                myTree->groupList.append(group);
            }
            BookmarkItem *site = new BookmarkItem();
            site->group = query.value(0).toString();
            site->desc = query.value(1).toString();
            site->url = query.value(2).toString();
            group->siteList.append(site);
        }
    }

    // Build the ListView
    myBookmarksView->clear();
    for (BookmarkGroup* cat = myTree->groupList.first(); cat; cat = myTree->groupList.next() ) {
    QListViewItem *catItem = new QListViewItem(myBookmarksView, cat->name,"");
    catItem->setOpen(true);
    for(BookmarkItem* site = cat->siteList.first(); site; site = cat->siteList.next() ) {
        new BookmarkViewItem(catItem, site);
    }
    }
}


void BookmarksConfig::setupView()
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(hmult*10));

    // Top fancy stuff
    QLabel *topLabel = new QLabel(this);
    topLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    topLabel->setText(tr("MythBrowser Bookmarks Settings"));

    vbox->addWidget(topLabel);

    QLabel *helpLabel = new QLabel(this);
    helpLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    helpLabel->setFrameStyle(QFrame::Box + QFrame::Sunken);
    helpLabel->setMargin(int(hmult*4));
    helpLabel->setText(tr("Press the 'New Bookmark' button to "
               "add a new site/group.\n"
               "Pressing SPACE/Enter on a selected entry "
               "removes it from the listview."));
    vbox->addWidget(helpLabel);

    // Add List View of Tabs and their bookmaks
    vbox->addWidget(myBookmarksView);

    connect(myBookmarksView, SIGNAL(doubleClicked(QListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(QListViewItem*)));
    connect(myBookmarksView, SIGNAL(spacePressed(QListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(QListViewItem*)));

    QFrame *hbar2 = new QFrame( this, "<hr>", 0 );
    hbar2->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar2->setFrameStyle( QFrame::Sunken + QFrame::HLine );
    hbar2->setFixedHeight( 12 );
    vbox->addWidget(hbar2);

    // Zoom Level & Browser Command
    QHBoxLayout *hbox2 = new QHBoxLayout(vbox);

    QLabel *zoomLabel = new QLabel(this);
    zoomLabel->setText(tr("Zoom [%]:"));
    zoomLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    zoomLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    hbox2->addWidget(zoomLabel);

    zoom = new MythSpinBox(this);
    zoom->setMinValue(20);
    zoom->setMaxValue(300);
    zoom->setLineStep(5);
    hbox2->addWidget(zoom);
    zoom->setValue(gContext->GetNumSetting("WebBrowserZoomLevel", 20));

    QLabel *browserLabel = new QLabel(this);
    browserLabel->setText(tr("Browser:"));
    browserLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    browserLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    hbox2->addWidget(browserLabel);
    browser = new MythLineEdit(this);
    browser->setRW(true);
    browser->setHelpText("this is the help line");
    hbox2->addWidget(browser);
    browser->setText(gContext->GetSetting("WebBrowserCommand", PREFIX "/bin/mythbrowser"));

    // Scroll Settings 
    QHBoxLayout *hbox3 = new QHBoxLayout(vbox);

    hidescrollbars = new MythCheckBox(this);
    hidescrollbars->setText(tr("Hide Scrollbars"));
    hidescrollbars->setChecked(gContext->GetNumSetting(
                                                "WebBrowserHideScrollbars", 0) 
             == 1);
    hbox3->addWidget(hidescrollbars);

    scrollmode = new MythCheckBox(this);
    scrollmode->setText(tr("Scroll Page"));
    scrollmode->setChecked(gContext->GetNumSetting("WebBrowserScrollMode", 1) 
             == 1);
    hbox3->addWidget(scrollmode);

    QLabel *label = new QLabel(this);
    label->setText(tr("Scroll Speed:"));
    label->setBackgroundOrigin(QWidget::WindowOrigin);
    label->setBackgroundOrigin(QWidget::WindowOrigin);
    hbox3->addWidget(label);

    scrollspeed = new MythSpinBox(this);
    scrollspeed->setMinValue(1);
    scrollspeed->setMaxValue(8);
    scrollspeed->setLineStep(1);
    hbox3->addWidget(scrollspeed);
    scrollspeed->setValue(gContext->GetNumSetting("WebBrowserScrollSpeed", 4));

    // Add new bookmark ------------------------------------
    QHBoxLayout *hbox = new QHBoxLayout(vbox);

    MythPushButton *newBookmark = new MythPushButton(tr("&New Bookmark"), this);
    hbox->addWidget(newBookmark);

    connect(newBookmark, SIGNAL(clicked()), this, SLOT(slotAddBookmark()));

    QLabel *customSiteLabel = new QLabel(this);
    customSiteLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    customSiteLabel->setText(tr("Add a new Website"));
    hbox->addWidget(customSiteLabel);

    // Finish -----------------------------------
    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    MythPushButton *finish = new MythPushButton(tr("&Finish"), this);
    hbox->addWidget(finish);

    connect(finish, SIGNAL(clicked()), this, SLOT(slotFinish()));
}


void BookmarksConfig::slotFinish()
{
    close();
}


void BookmarksConfig::slotBookmarksViewExecuted(QListViewItem *item)
{
    if(!item)
        return;

    BookmarkViewItem* viewItem = dynamic_cast<BookmarkViewItem*>(item);
    if (!viewItem) { // This is a upper level item, i.e. a "group name"
        // item->setOpen(!(item->isOpen()));
    } else {
    
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM websites WHERE url=:URL");
        query.bindValue(":URL",viewItem->myBookmarkSite->url);

        if (!query.exec()) {
           cerr << "MythBookmarksConfig: Error in deleting in DB" << endl;
           return;
        }
        populateListView();
    }
}

void BookmarksConfig::slotAddBookmark()
{
     PopupBox *popupBox = new PopupBox(this);
     connect(popupBox, SIGNAL(finished(const char*,const char*,const char*)),
             this, SLOT(slotWebSiteAdded(const char*,const char*,const char*)));
     popupBox->show();
}

void BookmarksConfig::slotWebSiteAdded(const char* group, const char* desc, const char* url)
{
    QString *groupStr = new QString(group);
    QString *descStr = new QString(desc);
    QString *urlStr = new QString(url);
    urlStr->stripWhiteSpace();
    if( urlStr->find("http://") == -1 && urlStr->find("file:/") == -1 )
        urlStr->prepend("http://");

    if(groupStr->isEmpty() || urlStr->isEmpty())
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO websites (grp, dsc, url) VALUES(:GROUP, :DESC, :URL);");
    query.bindValue(":GROUP",groupStr->utf8());
    query.bindValue(":DESC",descStr->utf8());
    query.bindValue(":URL",urlStr->utf8());
    if (!query.exec()) {
        cerr << "MythBookmarksConfig: Error in inserting in DB" << endl;
    }
    populateListView();
}

// ---------------------------------------------------

Bookmarks::Bookmarks(MythMainWindow *parent, const char *name)
    : MythDialog(parent, name)
{
    setPalette(parent->palette());

//    myTree = new BookmarkConfigPriv;

    // Create the database if not exists
    QString queryString( "CREATE TABLE IF NOT EXISTS websites "
                         "( grp VARCHAR(255) NOT NULL, dsc VARCHAR(255),"
             " url VARCHAR(255) NOT NULL PRIMARY KEY,"
                         "  updated INT UNSIGNED );");
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(queryString)) {
        cerr << "MythBookmarksConfig: Error in creating sql table" << endl;
    }

    // List View of Tabs and their bookmaks
    myBookmarksView = new MythListView(this);
    myBookmarksView->header()->hide();
    myBookmarksView->addColumn("Sites");
    myBookmarksView->setRootIsDecorated(true);
    myBookmarksView->addColumn("URL",-1);

    populateListView();
    setupView();

    setCursor(QCursor(Qt::ArrowCursor));
}


Bookmarks::~Bookmarks()
{
//    delete myTree;
}


void Bookmarks::populateListView()
{
    BookmarkConfigPriv* myTree = new BookmarkConfigPriv;

    myTree->groupList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT grp, dsc, url FROM websites ORDER BY grp");

    if (!query.isActive()) {
        cerr << "MythBrowserConfig: Error in loading from DB" << endl;
    } else {
        BookmarkGroup *group = new BookmarkGroup();
        group->name="Empty";
        while( query.next() ) {
            if( QString::compare(group->name,query.value(0).toString())!=0 ) {
                group = new BookmarkGroup();
                group->name = query.value(0).toString();
                myTree->groupList.append(group);
            }
            BookmarkItem *site = new BookmarkItem();
            site->group = query.value(0).toString();
            site->desc = query.value(1).toString();
            site->url = query.value(2).toString();
            group->siteList.append(site);
        }
    }

    // Build the ListView
    myBookmarksView->clear();
    for (BookmarkGroup* cat = myTree->groupList.first(); cat; cat = myTree->groupList.next() ) {
        QListViewItem *catItem = new QListViewItem(myBookmarksView, cat->name,"");
        catItem->setOpen(false);
        for(BookmarkItem* site = cat->siteList.first(); site; site = cat->siteList.next() ) {
            new BookmarkViewItem(catItem, site);
        }
    }

    myBookmarksView->setFocus();
    myBookmarksView->setCurrentItem(myBookmarksView->firstChild());
    myBookmarksView->setSelected(myBookmarksView->firstChild(),true);
}


void Bookmarks::setupView()
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(hmult*10));

    // Top fancy stuff
    QLabel *topLabel = new QLabel(this);
    topLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    topLabel->setText(tr("MythBrowser: Select group or single site to view"));

    QFrame *hbar1 = new QFrame( this, "<hr>", 0 );
    hbar1->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar1->setFrameStyle( QFrame::Sunken + QFrame::HLine );
    hbar1->setFixedHeight( 12 );

    vbox->addWidget(topLabel);
    vbox->addWidget(hbar1);

    // Add List View of Tabs and their bookmaks
    vbox->addWidget(myBookmarksView);

    connect(myBookmarksView, SIGNAL(doubleClicked(QListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(QListViewItem*)));
    connect(myBookmarksView, SIGNAL(spacePressed(QListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(QListViewItem*)));
}


void Bookmarks::slotBookmarksViewExecuted(QListViewItem *item)
{
    QString cmd = gContext->GetSetting("WebBrowserCommand", PREFIX "/bin/mythbrowser");
    QString zoom = QString(" -z %1 ").arg(gContext->GetNumSetting("WebBrowserZoomLevel",200));

    if(!item)
        return;

    BookmarkViewItem* viewItem = dynamic_cast<BookmarkViewItem*>(item);
    if (!viewItem) { // This is a upper level item, i.e. a "group name"
        QListViewItemIterator it(item);
        ++it;
        while ( it.current() ) {
            BookmarkViewItem* leafItem = dynamic_cast<BookmarkViewItem*>(it.current());
            if(leafItem)
                cmd += zoom+leafItem->myBookmarkSite->url;
            else
                break;
            ++it;
        }
        myth_system(cmd);
    } else {
        cmd += zoom+viewItem->myBookmarkSite->url;
        myth_system(cmd);
    }
}
