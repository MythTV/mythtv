#include <iostream>

// qt
#include <q3ptrlist.h>
#include <qstring.h>
#include <qfile.h>
#include <qdom.h>
#include <qlayout.h>
#include <qcursor.h>
#include <qlabel.h>
#include <q3vgroupbox.h>
#include <qtimer.h>

//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3Frame>
#include <Q3VBoxLayout>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdirs.h>

// mythbrowser
#include "bookmarkmanager.h"

using namespace std;

// ---------------------------------------------------

class BookmarkItem
{
public:
    typedef Q3PtrList<BookmarkItem> List;

    QString group;
    QString desc;
    QString url;
};

// ---------------------------------------------------

class BookmarkGroup
{
public:

    typedef Q3PtrList<BookmarkGroup> List;

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

class BookmarkViewItem : public Q3ListViewItem
{
public:

    BookmarkViewItem(Q3ListViewItem* parent, BookmarkItem* siteItem)
        : Q3ListViewItem(parent, siteItem->desc, siteItem->url)
        {
        myBookmarkSite = siteItem;
    }

    ~BookmarkViewItem()
        {}

    BookmarkItem* myBookmarkSite;
};

// ---------------------------------------------------

PopupBox::PopupBox(QWidget *parent)
        : QDialog(parent, 0, true, Qt::WType_Popup)
{
    setPalette(parent->palette());
    setFont(parent->font());

    Q3VBoxLayout *lay  = new Q3VBoxLayout(this, 5);

    Q3VGroupBox  *vbox = new Q3VGroupBox(tr("Add New Website"),this);
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

    Q3HBoxLayout *hbox = new Q3HBoxLayout(lay);

    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    MythPushButton *okButton = new MythPushButton(tr("&Ok"), this);
    okButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hbox->addWidget(okButton);

    hbox->addItem(new QSpacerItem(100, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    connect(okButton, SIGNAL(clicked()), this, SLOT(slotOkClicked()));

//    gContext->ThemeWidget(this);
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
    Q3ListViewItem *catItem = new Q3ListViewItem(myBookmarksView, cat->name,"");
    catItem->setOpen(true);
    for(BookmarkItem* site = cat->siteList.first(); site; site = cat->siteList.next() ) {
        new BookmarkViewItem(catItem, site);
    }
    }
}


void BookmarksConfig::setupView()
{
    Q3VBoxLayout *vbox = new Q3VBoxLayout(this, (int)(hmult*10));

    // Top fancy stuff
    QLabel *topLabel = new QLabel(this);
    topLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    topLabel->setText(tr("MythBrowser Bookmarks Settings"));

    vbox->addWidget(topLabel);

    QLabel *helpLabel = new QLabel(this);
    helpLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    helpLabel->setFrameStyle(Q3Frame::Box + Q3Frame::Sunken);
    helpLabel->setMargin(int(hmult*4));
    helpLabel->setText(tr("Press the 'New Bookmark' button to "
               "add a new site/group.\n"
               "Pressing SPACE/Enter on a selected entry "
               "removes it from the listview."));
    vbox->addWidget(helpLabel);

    // Add List View of Tabs and their bookmaks
    vbox->addWidget(myBookmarksView);

    connect(myBookmarksView, SIGNAL(doubleClicked(Q3ListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(Q3ListViewItem*)));
    connect(myBookmarksView, SIGNAL(spacePressed(Q3ListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(Q3ListViewItem*)));

    Q3Frame *hbar2 = new Q3Frame( this, "<hr>", 0 );
    hbar2->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar2->setFrameStyle( Q3Frame::Sunken + Q3Frame::HLine );
    hbar2->setFixedHeight( 12 );
    vbox->addWidget(hbar2);

    // Zoom Level & Browser Command
    Q3HBoxLayout *hbox2 = new Q3HBoxLayout(vbox);

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
    browser->setText(gContext->GetSetting("WebBrowserCommand", GetInstallPrefix() + "/bin/mythbrowser"));

    // Scroll Settings 
    Q3HBoxLayout *hbox3 = new Q3HBoxLayout(vbox);

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
    scrollspeed->setMaxValue(16);
    scrollspeed->setLineStep(1);
    hbox3->addWidget(scrollspeed);
    scrollspeed->setValue(gContext->GetNumSetting("WebBrowserScrollSpeed", 4));

    // Add new bookmark ------------------------------------
    Q3HBoxLayout *hbox = new Q3HBoxLayout(vbox);

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


void BookmarksConfig::slotBookmarksViewExecuted(Q3ListViewItem *item)
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
    if ( !urlStr->startsWith("http://") && !urlStr->startsWith("https://") && 
            !urlStr->startsWith("file:/") )
        urlStr->prepend("http://");

    if (groupStr->isEmpty() || urlStr->isEmpty())
        return;

    urlStr->replace("&amp;","&");

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
        Q3ListViewItem *catItem = new Q3ListViewItem(myBookmarksView, cat->name,"");
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
    Q3VBoxLayout *vbox = new Q3VBoxLayout(this, (int)(hmult*10));

    // Top fancy stuff
    QLabel *topLabel = new QLabel(this);
    topLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    topLabel->setText(tr("MythBrowser: Select group or single site to view"));

    Q3Frame *hbar1 = new Q3Frame( this, "<hr>", 0 );
    hbar1->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar1->setFrameStyle( Q3Frame::Sunken + Q3Frame::HLine );
    hbar1->setFixedHeight( 12 );

    vbox->addWidget(topLabel);
    vbox->addWidget(hbar1);

    // Add List View of Tabs and their bookmaks
    vbox->addWidget(myBookmarksView);

    connect(myBookmarksView, SIGNAL(doubleClicked(Q3ListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(Q3ListViewItem*)));
    connect(myBookmarksView, SIGNAL(spacePressed(Q3ListViewItem*)),
        this, SLOT(slotBookmarksViewExecuted(Q3ListViewItem*)));
}


void Bookmarks::slotBookmarksViewExecuted(Q3ListViewItem *item)
{
    QString cmd = gContext->GetSetting("WebBrowserCommand", GetInstallPrefix() + "/bin/mythbrowser");
    cmd += QString(" -z %1 ").arg(gContext->GetNumSetting("WebBrowserZoomLevel", 1.4));

    if(!item)
        return;

    BookmarkViewItem* viewItem = dynamic_cast<BookmarkViewItem*>(item);
    if (!viewItem) 
    {
        // This is a upper level item, i.e. a "group name"
        Q3ListViewItemIterator it(item);
        ++it;
        while (it.current())
        {
            BookmarkViewItem* leafItem = dynamic_cast<BookmarkViewItem*>(it.current());
            if(leafItem)
                cmd += " " + leafItem->myBookmarkSite->url;
            else
                break;
            ++it;
        }
        gContext->GetMainWindow()->AllowInput(false);
        cmd.replace("&","\\&"); 
        cmd.replace(";","\\;"); 
        myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
        gContext->GetMainWindow()->AllowInput(true);
    }
    else
    {
        cmd += viewItem->myBookmarkSite->url;
        gContext->GetMainWindow()->AllowInput(false);
        cmd.replace("&","\\&");
        cmd.replace(";","\\;");
        myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
        gContext->GetMainWindow()->AllowInput(true);
    }
}
