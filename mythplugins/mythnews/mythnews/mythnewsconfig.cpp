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

#include <qapplication.h>
#include <iostream>

#include <qptrlist.h>
#include <qstring.h>
#include <qfile.h>
#include <qdom.h>
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
    bool    inDB;
};

// ---------------------------------------------------

class NewsCategory
{
public:

    typedef QPtrList<NewsCategory> List;

    QString             name;
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
    m_db              = db;
    m_priv            = new MythNewsConfigPriv;
    m_updateFreqTimer = new QTimer(this);
    m_updateFreq      = gContext->GetNumSetting("NewsUpdateFrequency", 30);

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

    m_Theme       = 0;
    m_UISelector  = 0;
    m_UICategory  = 0;
    m_UISite      = 0;
    m_SpinBox     = 0;

    m_Context     = 0;
    m_InColumn    = 0;
    
    populateSites();

    setNoErase();
    loadTheme();
}

MythNewsConfig::~MythNewsConfig()
{
    delete m_priv;
    delete m_Theme;
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

            site->inDB = findInDB(site->name);

            cat->siteList.append(site);
        }

    }

    xmlFile.close();
}

void MythNewsConfig::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "news", "news-");

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {

        QDomElement e = child.toElement();
        if (!e.isNull()) {

            if (e.tagName() == "font") {
                m_Theme->parseFont(e);
            }
            else if (e.tagName() == "container") {

                QRect area;
                QString name;
                int context;
                m_Theme->parseContainer(e, name, context, area);

                if (name.lower() == "config-selector")
                    m_SelectorRect = area;
                else if (name.lower() == "config-sites")
                    m_SiteRect = area;
                else if (name.lower() == "config-freq")
                    m_FreqRect = area;
                else if (name.lower() == "config-bottom")
                    m_BotRect = area;
            }
            else {
                std::cerr << "Unknown element: " << e.tagName()
                          << std::endl;
                exit(-1);
            }
        }
    }

    LayerSet *container = m_Theme->GetSet("config-selector");
    if (!container) {
        std::cerr << "MythNews: Failed to get selector container."
                  << std::endl;
        exit(-1);
    }

    m_UISelector = (UIListBtnType*)container->GetType("selector");
    if (!m_UISelector) {
        std::cerr << "MythNews: Failed to get selector list area."
                  << std::endl;
        exit(-1);
    }

    container = m_Theme->GetSet("config-sites");
    if (!container) {
        std::cerr << "MythNews: Failed to get sites container." << std::endl;
        exit(-1);
    }

    m_UICategory = (UIListBtnType*)container->GetType("category");
    if (!m_UICategory) {
        std::cerr << "MythNews: Failed to get category list area."
                  << std::endl;
        exit(-1);
    }

    m_UISite = (UIListBtnType*)container->GetType("sites");
    if (!m_UISite) {
        std::cerr << "MythNews: Failed to get site list area." << std::endl;
        exit(-1);
    }

    for (NewsCategory* cat = m_priv->categoryList.first();
         cat; cat = m_priv->categoryList.next() ) {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UICategory, cat->name);
        item->setData(cat);
    }
    slotCategoryChanged(m_UICategory->GetItemFirst());

    container = m_Theme->GetSet("config-freq");
    if (!container) {
        std::cerr << "MythNews: Failed to get frequency container."
                  << std::endl;
        exit(-1);
    }

    UIBlackHoleType* spinboxHolder =
        (UIBlackHoleType*)container->GetType("spinbox_holder");
    if (spinboxHolder) {
        m_SpinBox = new MythNewsSpinBox(this);
        m_SpinBox->setRange(30,1000);
        m_SpinBox->setLineStep(10);
        m_SpinBox->setValue(m_updateFreq);
        QFont f = gContext->GetMediumFont();
        m_SpinBox->setFont(f);
        m_SpinBox->setFocusPolicy(QWidget::NoFocus);
        m_SpinBox->setGeometry(spinboxHolder->getScreenArea());
        m_SpinBox->hide();
        connect(m_SpinBox, SIGNAL(valueChanged(int)),
                SLOT(slotUpdateFreqChanged()));
    }

    UITextType* ttype = (UITextType*)container->GetType("help");
    if (ttype) {
        ttype->SetText(tr("Set Update Frequency in Minutes\n"
                          "Minimum allowed value is 30 Minutes"));
    }
        
    connect(m_UISelector, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotContextChanged(UIListBtnTypeItem*)));
    connect(m_UICategory, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotCategoryChanged(UIListBtnTypeItem*)));

    new UIListBtnTypeItem(m_UISelector, "Select News Feed");
    new UIListBtnTypeItem(m_UISelector, "Set Update Frequency");
    
    m_UISelector->SetActive(true);
}


void MythNewsConfig::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_SelectorRect)) {
        updateSelector();
    }

    if (r.intersects(m_BotRect)) {
        updateBot();
    }
    
    if (m_Context == 0) {
        if (r.intersects(m_SiteRect)) {
            updateSites();
        }
    }
    else {
        if (r.intersects(m_FreqRect)) {
            updateFreq();
        }
    }
}

void MythNewsConfig::updateSelector()
{
    QPixmap pix(m_SelectorRect.size());
    pix.fill(this, m_SelectorRect.topLeft());
    QPainter p(&pix);
    
    LayerSet* container = m_Theme->GetSet("config-selector");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();
    
    bitBlt(this, m_SelectorRect.left(), m_SelectorRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNewsConfig::updateSites()
{
    QPixmap pix(m_SiteRect.size());
    pix.fill(this, m_SiteRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("config-sites");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();
    
    bitBlt(this, m_SiteRect.left(), m_SiteRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNewsConfig::updateFreq()
{
    QPixmap pix(m_FreqRect.size());
    pix.fill(this, m_FreqRect.topLeft());
    QPainter p(&pix);
    
    LayerSet* container = m_Theme->GetSet("config-freq");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();
    
    bitBlt(this, m_FreqRect.left(), m_FreqRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNewsConfig::updateBot()
{
    QPixmap pix(m_BotRect.size());
    pix.fill(this, m_BotRect.topLeft());
    QPainter p(&pix);
    
    LayerSet* container = m_Theme->GetSet("config-bottom");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();
    
    bitBlt(this, m_BotRect.left(), m_BotRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNewsConfig::toggleItem(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;

    NewsSiteItem* site = (NewsSiteItem*) item->getData();

    bool checked = (item->state() == UIListBtnTypeItem::FullChecked);

    if (!checked) {
        if (insertInDB(site)) {
            site->inDB = true;
            item->setChecked(UIListBtnTypeItem::FullChecked);
        }
        else {
            site->inDB = false;
            item->setChecked(UIListBtnTypeItem::NotChecked);
        }
    }
    else {
        if (removeFromDB(site)) {
            site->inDB = false;
            item->setChecked(UIListBtnTypeItem::NotChecked);
        } 
        else {
            site->inDB = true;
            item->setChecked(UIListBtnTypeItem::FullChecked);
        }
    }
    
    updateSites();
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

void MythNewsConfig::slotContextChanged(UIListBtnTypeItem *item)
{
    if (!item)
        return;

    uint context = (uint) m_UISelector->GetItemPos(item);

    if (m_Context == context)
        return;

    m_Context = context;
    if (m_Context == 0)
        m_SpinBox->hide();
    else
        m_SpinBox->show();
    
    update();    
}

void MythNewsConfig::slotCategoryChanged(UIListBtnTypeItem *item)
{
    if (!item)
        return;
    
    m_UISite->Reset();

    NewsCategory* cat = (NewsCategory*) item->getData();
    if (cat) {

        for (NewsSiteItem* site = cat->siteList.first();
             site; site = cat->siteList.next() ) {
            UIListBtnTypeItem* item =
                new UIListBtnTypeItem(m_UISite, site->name, 0, true,
                                      site->inDB ?
                                      UIListBtnTypeItem::FullChecked :
                                      UIListBtnTypeItem::NotChecked);
            item->setData(site);
        }
    }
}

void MythNewsConfig::slotUpdateFreqChanged()
{
    m_updateFreqTimer->stop();
    m_updateFreqTimer->start(500, true);
}

void MythNewsConfig::slotUpdateFreqTimerTimeout()
{
    if (m_updateFreqTimer->isActive()) return;

    if (m_SpinBox) {
        gContext->SaveSetting("NewsUpdateFrequency",
                              m_SpinBox->value());
    }
}


void MythNewsConfig::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", e, actions);
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP") {
            cursorUp();
        }
        else if (action == "DOWN") {
            cursorDown();
        }
        else if (action == "LEFT") {
            cursorLeft();
        }
        else if (action == "RIGHT") {
            cursorRight();
        }
        else if (action == "SELECT") {
            if (m_InColumn == 2 && m_Context == 0) {
                UIListBtnTypeItem *item = m_UISite->GetItemCurrent();
                if (item)
                    toggleItem(item);
            }
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
    else
        update();
}

void MythNewsConfig::cursorUp()
{
    if (m_InColumn == 0) {
        m_UISelector->MoveUp();
    }
    else if (m_Context == 0) {
        if (m_InColumn == 1)
            m_UICategory->MoveUp();
        else
            m_UISite->MoveUp();
    }
    
    update();
}

void MythNewsConfig::cursorDown()
{
    if (m_InColumn == 0) {
        m_UISelector->MoveDown();
    }
    else if (m_Context == 0) {
        if (m_InColumn == 1)
            m_UICategory->MoveDown();
        else
            m_UISite->MoveDown();
    }

    update();
}

void MythNewsConfig::cursorLeft()
{
    if (m_InColumn == 0) return;    

    m_InColumn--;

    if (m_Context == 0) {
        if (m_InColumn == 1) {
            m_UICategory->SetActive(true);
            m_UISite->SetActive(false);
        }
        else {
            m_UICategory->SetActive(false);
            m_UISelector->SetActive(true);
        }
    }
    else {
        m_UISelector->SetActive(true);
        m_SpinBox->clearFocus();
    }
    update();
}


void MythNewsConfig::cursorRight()
{
    if (m_InColumn == 2 || (m_InColumn == 1 && m_Context == 1 ))
        return;    

    m_InColumn++;

    m_UISelector->SetActive(false);

    if (m_Context == 0) {
        if (m_InColumn == 1) {
            m_UICategory->SetActive(true);
        }
        else {
            if (m_UISite->GetCount() == 0) 
                m_InColumn--;
            else {
                m_UICategory->SetActive(false);
                m_UISite->SetActive(true);
            }
        }
    }
    else {
        m_SpinBox->setFocus();
    }
    update();
}

// ---------------------------------------------------------------------

MythNewsSpinBox::MythNewsSpinBox(QWidget* parent, const char* widgetName )
    : MythSpinBox(parent,widgetName)
{
    
}

bool MythNewsSpinBox::eventFilter(QObject* o, QEvent* e)
{
    (void)o;

    if (e->type() == QEvent::FocusIn)
    {
        QColor highlight = colorGroup().highlight();
        editor()->setPaletteBackgroundColor(highlight);
    }
    else if (e->type() == QEvent::FocusOut)
    {
        editor()->unsetPalette();
    }

    if (e->type() != QEvent::KeyPress)
        return FALSE;

    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent *)e,
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP") 
                stepUp();
            else if (action == "DOWN") 
                stepDown();
            else if (action == "PAGEUP")
                stepUp();
            else if (action == "PAGEDOWN")
                stepDown();
            else if (action == "LEFT") {
                QKeyEvent *ev = (QKeyEvent*)e;
                QApplication::postEvent(parentWidget(),
                                        new QKeyEvent(ev->type(),ev->key(),
                                                      ev->ascii(),
                                                      ev->state()));
            }
            else if (action == "SELECT" || action == "ESCAPE")
                return false;
            else
                handled = false;
        }
    }

    return true;
}

