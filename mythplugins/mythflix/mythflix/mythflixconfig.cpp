/* ============================================================
 * File  : mythflixconfig.cpp
 * Author: John Petrocik <john@petrocik.net>
 * Date  : 2005-10-28
 * Description :
 *
 * Copyright 2005 by John Petrocik

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

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

#include "mythflixconfig.h"

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

class MythFlixConfigPriv
{
public:

    NewsCategory::List categoryList;
    QStringList selectedSitesList;

    MythFlixConfigPriv() {
        categoryList.setAutoDelete(true);
    }

    ~MythFlixConfigPriv() {
        categoryList.clear();
    }

};

// ---------------------------------------------------

MythFlixConfig::MythFlixConfig(MythMainWindow *parent,
                               const char *name)
    : MythDialog(parent, name)
{
    m_priv            = new MythFlixConfigPriv;
    m_updateFreqTimer = new QTimer(this);
    m_updateFreq      = gContext->GetNumSetting("NewsUpdateFrequency", 30);

    connect(m_updateFreqTimer, SIGNAL(timeout()),
            this, SLOT(slotUpdateFreqTimerTimeout()));

    // Create the database if not exists
    QString queryString( "CREATE TABLE IF NOT EXISTS netflix "
                         "( name VARCHAR(100) NOT NULL PRIMARY KEY,"
                         "  category  VARCHAR(255) NOT NULL,"
                         "  url  VARCHAR(255) NOT NULL,"
                         "  ico  VARCHAR(255),"
                         "  updated INT UNSIGNED,"
                         "  is_queue INT UNSIGNED);");

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec(queryString))
        VERBOSE(VB_IMPORTANT, QString("MythFlixConfig: Error in creating sql table"));

    m_Theme       = 0;
    m_UICategory  = 0;
    m_UISite      = 0;
    m_SpinBox     = 0;

    m_Context     = 0;
    m_InColumn    = 1;
    
    populateSites();

    setNoErase();
    loadTheme();
}

MythFlixConfig::~MythFlixConfig()
{
    delete m_priv;
    delete m_Theme;
}

void MythFlixConfig::populateSites()
{
    QString filename = gContext->GetShareDir()
                       + "mythflix/netflix-rss.xml";
    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(IO_ReadOnly)) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Cannot open netflix-rss.xml"));
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    QDomDocument domDoc;

    if (!domDoc.setContent(&xmlFile, false, &errorMsg, &errorLine, &errorColumn)) 
    {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Error in reading content of netflix-rss.xml"));
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Error, parsing %1\n"
                                      "at line: %2  column: %3 msg: %4").
                arg(filename).arg(errorLine).arg(errorColumn).arg(errorMsg));
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

void MythFlixConfig::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "config", "netflix-");

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

                if (name.lower() == "config-sites")
                    m_SiteRect = area;
                else if (name.lower() == "config-freq")
                    m_FreqRect = area;
                else if (name.lower() == "config-bottom")
                    m_BotRect = area;
            }
            else {
                VERBOSE(VB_IMPORTANT, QString("MythFlix: Unknown element: %1").arg(e.tagName()));
                exit(-1);
            }
        }
    }


    LayerSet *container  = m_Theme->GetSet("config-sites");
    if (!container) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get sites container."));
        exit(-1);
    }

    UITextType* ttype = (UITextType*)container->GetType("context_switch");
    if (ttype) {
        ttype->SetText(tr("Press MENU to set the update frequency."));
    }


    m_UICategory = (UIListBtnType*)container->GetType("category");
    if (!m_UICategory) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get category list area."));
        exit(-1);
    }

    m_UISite = (UIListBtnType*)container->GetType("sites");
    if (!m_UISite) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get site list area."));
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
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get frequency container."));
        exit(-1);
    }

    UIBlackHoleType* spinboxHolder =
        (UIBlackHoleType*)container->GetType("spinbox_holder");
    if (spinboxHolder) {
        m_SpinBox = new MythFlixSpinBox(this);
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

    ttype = (UITextType*)container->GetType("help");
    if (ttype) {
        ttype->SetText(tr("Set update frequency by using the up/down arrows.\n"
                          "The minimum allowed value is 30 Minutes."));
    }

    ttype = (UITextType*)container->GetType("context_switch");
    if (ttype) {
        ttype->SetText(tr("Press MENU to return to feed selection."));
    }
        
    connect(m_UICategory, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotCategoryChanged(UIListBtnTypeItem*)));

    m_UICategory->SetActive(true);
}


void MythFlixConfig::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();


    if (r.intersects(m_BotRect)) {
     //   updateBot();
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


void MythFlixConfig::updateSites()
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

void MythFlixConfig::updateFreq()
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

void MythFlixConfig::updateBot()
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

void MythFlixConfig::toggleItem(UIListBtnTypeItem *item)
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

bool MythFlixConfig::findInDB(const QString& name)
{
    bool val = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM netflix WHERE name = :NAME ;");
    query.bindValue(":NAME", name.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("new find in db", query);
        return val;
    }

    val = query.numRowsAffected() > 0;

    return val;
}

bool MythFlixConfig::insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    if (findInDB(site->name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO netflix (name,category,url,ico, is_queue) "
                  " VALUES( :NAME, :CATEGORY, :URL, :ICON, 0);");
    query.bindValue(":NAME", site->name.utf8());
    query.bindValue(":CATEGORY", site->category.utf8());
    query.bindValue(":URL", site->url.utf8());
    query.bindValue(":ICON", site->ico.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("netlix: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool MythFlixConfig::removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM netflix WHERE name = :NAME ;");
    query.bindValue(":NAME", site->name.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("netflix: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}


void MythFlixConfig::slotCategoryChanged(UIListBtnTypeItem *item)
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

void MythFlixConfig::slotUpdateFreqChanged()
{
    m_updateFreqTimer->stop();
    m_updateFreqTimer->start(500, true);
}

void MythFlixConfig::slotUpdateFreqTimerTimeout()
{
    if (m_updateFreqTimer->isActive()) return;

    if (m_SpinBox) {
        gContext->SaveSetting("NewsUpdateFrequency",
                              m_SpinBox->value());
    }
}


void MythFlixConfig::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("NetFlix", e, actions);
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP") {
            cursorUp();
        }
        else if (action == "PAGEUP") {
             cursorUp(true);
        }
        else if (action == "DOWN") {
            cursorDown();
        }
        else if (action == "PAGEDOWN") {
             cursorDown(true);
        }
        else if (action == "LEFT") {
            cursorLeft();
        }
        else if (action == "RIGHT") {
            cursorRight();
        }
        else if (action == "MENU") {
           changeContext();
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

void MythFlixConfig::cursorUp(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_Context == 0) {
        if (m_InColumn == 1)
            m_UICategory->MoveUp(unit);
        else
            m_UISite->MoveUp(unit);
    }
    
    update();
}

void MythFlixConfig::cursorDown(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_Context == 0) {
        if (m_InColumn == 1)
            m_UICategory->MoveDown(unit);
        else
            m_UISite->MoveDown(unit);
    }

    update();
}

void MythFlixConfig::cursorLeft()
{
    if (m_InColumn == 1)
        return;    

    m_InColumn--;

    if (m_Context == 0) {
        if (m_InColumn == 1) {
            m_UICategory->SetActive(true);
            m_UISite->SetActive(false);
        }
    }
    update();
}


void MythFlixConfig::cursorRight()
{
    if (m_InColumn == 2 || (m_InColumn == 1 && m_Context == 1 ))
        return;    

    m_InColumn++;


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
    update();
}

// ---------------------------------------------------------------------

MythFlixSpinBox::MythFlixSpinBox(QWidget* parent, const char* widgetName )
    : MythSpinBox(parent,widgetName)
{
    
}

bool MythFlixSpinBox::eventFilter(QObject* o, QEvent* e)
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
            else if (action == "SELECT" || action == "LEFT" || action == "MENU") {
                QKeyEvent *ev = (QKeyEvent*)e;
                QApplication::postEvent(parentWidget(),
                                        new QKeyEvent(ev->type(),ev->key(),
                                                      ev->ascii(),
                                                      ev->state()));
            }
            else if (action == "ESCAPE")
                return false;
            else
                handled = false;
        }
    }

    return true;
}


void MythFlixConfig::changeContext()
{
    if(m_Context == 1)
    {
        m_Context = 0;
        m_SpinBox->hide();
        m_SpinBox->clearFocus();
    }
    else
    {
        m_Context = 1;
        m_SpinBox->show();
        m_SpinBox->setFocus();
    }

    update();
}

