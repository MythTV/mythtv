/* ============================================================
 * File  : mythnewsconfig.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-09-02
 * Description :
 *
 * Copyright 2003 by Renchi Raju

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

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

#include "mythnewsconfig.h"

using namespace std;

// ---------------------------------------------------

class NewsSiteItem
{
public:

    typedef QPtrList<NewsSiteItem> List;

    QString name;
    QString category;
    QString url;
    QString ico;
};

// ---------------------------------------------------

class NewsCategory
{
public:

    typedef QPtrList<NewsCategory> List;

    QString         name;
    NewsSiteItem::List  siteList;

    NewsCategory() {
        siteList.setAutoDelete(true);
    }

    ~NewsCategory() {
        siteList.clear();
    }

    void clear() {
        siteList.clear();
    };
};

// ---------------------------------------------------

class NewsSiteViewItem : public QListViewItem
{
public:

    NewsSiteViewItem(QListViewItem* parent,
                     NewsSiteItem* siteItem)
        : QListViewItem(parent, siteItem->name)
        { m_newsSite = siteItem; }

    ~NewsSiteViewItem()
        {}

    NewsSiteItem* m_newsSite;

};

// ---------------------------------------------------

class NewsSelSiteViewItem : public QListBoxText
{
public:

    NewsSelSiteViewItem(QListBox* parent,
                     NewsSiteItem* siteItem)
        : QListBoxText(parent, siteItem->name)
        { m_newsSite = siteItem; }

    ~NewsSelSiteViewItem()
        { if (m_newsSite)
            delete m_newsSite;
        }

    NewsSiteItem* m_newsSite;

};


// ---------------------------------------------------

class CMythListBox : public MythListBox
{

public:

    CMythListBox(QWidget *parent):MythListBox(parent) {}

    virtual void keyPressEvent(QKeyEvent* e) {

        switch (e->key())
        {
        case Key_Space:
        case Key_Return:
        case Key_Enter:
            emit returnPressed(item(currentItem()));
            break;
        default:
            MythListBox::keyPressEvent(e);
        }
    }

    virtual void focusInEvent(QFocusEvent *e) {

        QListBox::focusInEvent(e);
        setSelected(item(currentItem()), true);
    }

};

// ---------------------------------------------------

CPopupBox::CPopupBox(QWidget *parent)
        : QDialog(parent, 0, true, WType_Popup)
{
    setPalette(parent->palette());
    setFont(parent->font());

    QVBoxLayout *lay  = new QVBoxLayout(this, 5);
    QVGroupBox  *vbox = new QVGroupBox("Add Custom Site",
                                       this);
    lay->addWidget(vbox);

    QLabel *lab;

    lab = new QLabel("News Feed Name:", vbox);
    lab->setBackgroundOrigin(QWidget::WindowOrigin);
    m_nameEdit = new QLineEdit(vbox);

    lab =new QLabel("News Feed Site URL:", vbox);
    lab->setBackgroundOrigin(QWidget::WindowOrigin);
    m_urlEdit = new QLineEdit(vbox);

    lab = new QLabel("(Optional) News Feed Site Icon URL:", vbox);
    lab->setBackgroundOrigin(QWidget::WindowOrigin);
    m_icoEdit = new QLineEdit(vbox);

    QHBoxLayout *hbox = new QHBoxLayout(lay);

    hbox->addItem(new QSpacerItem(100, 0,
                                  QSizePolicy::Expanding,
                                  QSizePolicy::Minimum));
    MythPushButton *okButton =
        new MythPushButton("&Ok", this);
    okButton->setSizePolicy(QSizePolicy::Expanding,
                            QSizePolicy::Fixed);
    hbox->addWidget(okButton);

    hbox->addItem(new QSpacerItem(100, 0,
                                  QSizePolicy::Expanding,
                                  QSizePolicy::Minimum));

    connect(okButton, SIGNAL(clicked()),
            this, SLOT(slotOkClicked()));

    gContext->ThemeWidget(this);

}

CPopupBox::~CPopupBox()
{

}

void CPopupBox::slotOkClicked()
{
    emit finished(m_nameEdit->text(),
                  m_urlEdit->text(),
                  m_icoEdit->text());
    close();
}

// ---------------------------------------------------

class MythNewsConfigPriv
{
public:

    NewsCategory::List categoryList;
    QStringList selectedSitesList;

    MythNewsConfigPriv() {
        categoryList.setAutoDelete(true);
    }

    ~MythNewsConfigPriv() {
        categoryList.clear();
    }

};

// ---------------------------------------------------

MythNewsConfig::MythNewsConfig(QSqlDatabase *db,
                               MythMainWindow *parent,
                               const char *name)
    : MythDialog(parent, name)
{

    setPalette(parent->palette());

    m_db = db;
    m_priv   = new MythNewsConfigPriv;
    m_updateFreqTimer = new QTimer(this);

    connect(m_updateFreqTimer, SIGNAL(timeout()),
            this, SLOT(slotUpdateFreqTimerTimeout()));

    // Create the database if not exists
    QString queryString( "CREATE TABLE IF NOT EXISTS newssites "
                         "( name VARCHAR(100) NOT NULL PRIMARY KEY,"
                         "  category  VARCHAR(255) NOT NULL,"
                         "  url  VARCHAR(255) NOT NULL,"
                         "  ico  VARCHAR(255),"
                         "  updated INT UNSIGNED );");
    QSqlQuery query(QString::null, m_db);
    if (!query.exec(queryString)) {
	    cerr << "MythNewsConfig: Error in creating sql table" << endl;
    }

    populateSites();
    setupView();

    setCursor(QCursor(Qt::ArrowCursor));
}

MythNewsConfig::~MythNewsConfig()
{
    delete m_priv;
}

void MythNewsConfig::populateSites()
{
    QString filename = gContext->GetInstallPrefix()
                       + "/share/mythtv/mythnews/news-sites.xml";
    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(IO_ReadOnly)) {
        cerr << "MythNews: Cannot open news-sites.xml" << endl;
        return;
    }

    QDomDocument domDoc;
    if (!domDoc.setContent(&xmlFile)) {
        cerr << "MythNews: Error in reading content of news-sites.xml" << endl;
        return;
    }

    m_priv->categoryList.clear();

    QDomNodeList catList =
        domDoc.elementsByTagName(QString::fromLatin1("category"));

    QDomNode catNode;
    QDomNode siteNode;
    for (unsigned int i = 0; i < catList.count(); i++) {
        catNode = catList.item(i);

        NewsCategory *cat = new NewsCategory();
        cat->name = catNode.toElement().attribute("name");

        m_priv->categoryList.append(cat);

        QDomNodeList siteList = catNode.childNodes();

        for (unsigned int j = 0; j < siteList.count(); j++) {
            siteNode = siteList.item(j);

            NewsSiteItem *site = new NewsSiteItem();
            site->name =
                siteNode.namedItem(QString("title")).toElement().text();
            site->category =
                cat->name;
            site->url =
                siteNode.namedItem(QString("url")).toElement().text();
            site->ico =
                siteNode.namedItem(QString("ico")).toElement().text();

            cat->siteList.append(site);
        }

    }

    xmlFile.close();
}

void MythNewsConfig::setupView()
{
    QVBoxLayout *vbox = new QVBoxLayout(this,
                                        (int)(hmult*10));

    // Top fancy stuff

    QLabel *topLabel = new QLabel(this);
    topLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    topLabel->setText("MythNews Settings");

    QFrame *hbar1 = new QFrame( this, "<hr>", 0 );
    hbar1->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar1->setFrameStyle( QFrame::Sunken + QFrame::HLine );
    hbar1->setFixedHeight( 12 );

    vbox->addWidget(topLabel);
    vbox->addWidget(hbar1);

    QLabel *helpLabel = new QLabel(this);
    helpLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    helpLabel->setFrameStyle(QFrame::Box + QFrame::Sunken);
    helpLabel->setMargin(int(hmult*4));
    helpLabel->setText("Select and press Enter/Space to choose "
                       "news feed sites from left\n"
                       "Selected sites are shown on right "
                       "(pressing Enter/Space there would remove them)");
    vbox->addWidget(helpLabel);

    // List Views

    QHBoxLayout *hbox = new QHBoxLayout(vbox);

    m_leftView = new MythListView(this);
    m_leftView->header()->hide();
    m_leftView->addColumn("Sites");
    m_leftView->setRootIsDecorated(true);
    hbox->addWidget(m_leftView);

    m_rightView = new CMythListBox(this);
    hbox->addWidget(m_rightView);

    // Populate Views - Left View

    for (NewsCategory* cat = m_priv->categoryList.first();
         cat; cat = m_priv->categoryList.next() ) {
        QListViewItem *catItem =
            new QListViewItem(m_leftView, cat->name);
        catItem->setOpen(true);
        for (NewsSiteItem* site = cat->siteList.first();
             site; site = cat->siteList.next() ) {
            new NewsSiteViewItem(catItem, site);
        }
    }

    // Populate Views - Right View;

    QSqlQuery query("SELECT name FROM newssites", m_db);
    if (!query.isActive()) {
        cerr << "MythNewsConfig: Error in loading from DB" << endl;
    }
    else {
        while ( query.next() ) {
            NewsSiteItem* site = new NewsSiteItem();
            site->name = query.value(0).toString();
            new NewsSelSiteViewItem(m_rightView, site);
        }
        m_rightView->sort();
    }

    connect(m_leftView, SIGNAL(doubleClicked(QListViewItem*)),
            this, SLOT(slotSitesViewExecuted(QListViewItem*)));
    connect(m_leftView, SIGNAL(spacePressed(QListViewItem*)),
            this, SLOT(slotSitesViewExecuted(QListViewItem*)));
    connect(m_rightView, SIGNAL(doubleClicked(QListBoxItem*)),
            this, SLOT(slotSelSitesViewExecuted(QListBoxItem*)));
    connect(m_rightView, SIGNAL(returnPressed(QListBoxItem*)),
            this, SLOT(slotSelSitesViewExecuted(QListBoxItem*)));

    QFrame *hbar2 = new QFrame( this, "<hr>", 0 );
    hbar2->setBackgroundOrigin(QWidget::WindowOrigin);
    hbar2->setFrameStyle( QFrame::Sunken + QFrame::HLine );
    hbar2->setFixedHeight( 12 );
    vbox->addWidget(hbar2);

    // Bottom view ------------------------------

    // custom site ------------------------------------

    hbox = new QHBoxLayout(vbox);

    MythPushButton *customSiteBtn =
        new MythPushButton("&Custom Site", this);
    hbox->addWidget(customSiteBtn);

    connect(customSiteBtn, SIGNAL(clicked()),
            this, SLOT(slotAddCustomSite()));

    QLabel *customSiteLabel = new QLabel(this);
    customSiteLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    customSiteLabel->setText("Add a Custom New Feed Site");

    hbox->addWidget(customSiteLabel);

    // update freq -----------------------------------

    hbox->addItem(new QSpacerItem(100, 0,
                                  QSizePolicy::Expanding,
                                  QSizePolicy::Minimum));


    m_updateFreqBox = new MythSpinBox(this);
    m_updateFreqBox->setMinValue(30);
    m_updateFreqBox->setMaxValue(10000);
    m_updateFreqBox->setLineStep(5);
    hbox->addWidget(m_updateFreqBox);

    m_updateFreqBox->setValue(
        gContext->GetNumSetting("NewsUpdateFrequency", 30));

    connect(m_updateFreqBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotUpdateFreqChanged()));

    QLabel *updateFreqLabel = new QLabel(this);
    updateFreqLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    updateFreqLabel->setText("Update Frequency (in minutes)");
    hbox->addWidget(updateFreqLabel);


}


void MythNewsConfig::slotSitesViewExecuted(QListViewItem *item)
{
    if (!item) return;

    NewsSiteViewItem* viewItem =
        dynamic_cast<NewsSiteViewItem*>(item);
    if (!viewItem) {
        // This is a upper level item
        item->setOpen(!(item->isOpen()));
    }
    else {
        if (insertInDB(viewItem->m_newsSite)) {
            NewsSiteItem* site = new NewsSiteItem;
            site->name = viewItem->m_newsSite->name;
            new NewsSelSiteViewItem(m_rightView, site);
            m_rightView->sort();
        }
    }
}

void MythNewsConfig::slotSelSitesViewExecuted(QListBoxItem *item)
{
    if (!item) return;

    NewsSelSiteViewItem* viewItem =
        dynamic_cast<NewsSelSiteViewItem*>(item);
    if (!viewItem) {
        cerr << "MythNewsConfig: qlistboxitem not derived from newselsiteviewitem"
             << endl;
        return;
    }

    if (removeFromDB(viewItem->m_newsSite)) {
        delete viewItem;
        m_rightView->setSelected(m_rightView->item(0),true);
    }
}

bool MythNewsConfig::findInDB(const QString& name)
{
    bool val = false;

    QSqlQuery query( "SELECT name FROM newssites WHERE name='"
                     + name + "'", m_db);
    if (!query.isActive()) {
        cerr << "MythNewsConfig: Error in finding in DB" << endl;
        return val;
    }

    val = query.numRowsAffected() > 0;

    return val;
}

bool MythNewsConfig::insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    if (findInDB(site->name))
        return false;

    QSqlQuery query( QString("INSERT INTO newssites "
                             " (name,category,url,ico) "
                             " VALUES( '") +
                     site->name     + "', '" +
                     site->category + "', '" +
                     site->url      + "', '" +
                     site->ico      + "' );");
    if (!query.isActive()) {
        cerr << "MythNewsConfig: Error in inserting in DB" << endl;
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool MythNewsConfig::removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    QSqlQuery query( "DELETE FROM newssites WHERE name='"
                     + site->name + "'", m_db);
    if (!query.isActive()) {
        cerr << "MythNewsConfig: Error in Deleting from DB" << endl;
        return false;
    }

    return (query.numRowsAffected() > 0);
}

void MythNewsConfig::slotAddCustomSite()
{
    CPopupBox *popupBox = new CPopupBox(this);
    connect(popupBox, SIGNAL(finished(const QString&,
                                      const QString&,
                                      const QString&)),
            this, SLOT(slotCustomSiteAdded(const QString&,
                                           const QString&,
                                           const QString&)));
    popupBox->show();
}

void MythNewsConfig::slotCustomSiteAdded(const QString& name,
                                         const QString& url,
                                         const QString& ico)
{
    if (name.isEmpty() || url.isEmpty()) return;

    NewsSiteItem* site = new NewsSiteItem;
    site->name = name;
    site->url  = url;
    site->ico  = ico;
    site->category = "Miscellaneous";

    if (insertInDB(site)) {
        new NewsSelSiteViewItem(m_rightView, site);
        m_rightView->sort();
    }
    else
        delete site;
}

void MythNewsConfig::slotUpdateFreqChanged()
{
    m_updateFreqTimer->stop();
    m_updateFreqTimer->start(500, true);
}

void MythNewsConfig::slotUpdateFreqTimerTimeout()
{
    if (m_updateFreqTimer->isActive()) return;

    gContext->SaveSetting("NewsUpdateFrequency",
                         m_updateFreqBox->value());
}
