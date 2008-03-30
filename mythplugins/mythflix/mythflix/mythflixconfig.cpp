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

// QT headers
#include <qapplication.h>
#include <q3ptrlist.h>
#include <qstring.h>
#include <qfile.h>
#include <qdom.h>
//Added by qt3to4:
#include <QKeyEvent>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythFlix headers
#include "mythflixconfig.h"

using namespace std;

// ---------------------------------------------------

class NewsSiteItem
{
public:

    typedef Q3PtrList<NewsSiteItem> List;

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

    typedef Q3PtrList<NewsCategory> List;

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

MythFlixConfig::MythFlixConfig(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name)
{
    m_priv            = new MythFlixConfigPriv;

    m_siteList = m_genresList = NULL;

    populateSites();
}

MythFlixConfig::~MythFlixConfig()
{
    delete m_priv;
}

void MythFlixConfig::populateSites()
{
    QString filename = gContext->GetShareDir()
                       + "mythflix/netflix-rss.xml";
    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly)) {
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
    for (int i = 0; i < catList.count(); i++) {
        catNode = catList.item(i);

        NewsCategory *cat = new NewsCategory();
        cat->name = catNode.toElement().attribute("name");

        m_priv->categoryList.append(cat);

        QDomNodeList siteList = catNode.childNodes();

        for (int j = 0; j < siteList.count(); j++) {
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

bool MythFlixConfig::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("netflix-ui.xml", "config", this);

    if (!foundtheme)
    {
        VERBOSE(VB_IMPORTANT, "Unable to load window 'config' from "
                              "netflix-ui.xml");
        return false;
    }

    m_genresList = dynamic_cast<MythListButton *>
                (GetChild("sites"));

    m_siteList = dynamic_cast<MythListButton *>
                (GetChild("category"));

    if (!m_genresList || !m_siteList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_siteList, SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT(slotCategoryChanged(MythListButtonItem*)));
    connect(m_genresList, SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(toggleItem(MythListButtonItem*)));

    m_siteList->SetCanTakeFocus(false);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_genresList);
    m_genresList->SetActive(true);
    m_siteList->SetActive(false);

    loadData();

    return true;
}

void MythFlixConfig::loadData()
{

    for (NewsCategory* cat = m_priv->categoryList.first();
         cat; cat = m_priv->categoryList.next() ) {
        MythListButtonItem* item =
            new MythListButtonItem(m_siteList, cat->name);
        item->setData(cat);
    }
    slotCategoryChanged(m_siteList->GetItemFirst());

}

void MythFlixConfig::toggleItem(MythListButtonItem *item)
{
    if (!item || !item->getData())
        return;

    NewsSiteItem* site = (NewsSiteItem*) item->getData();

    bool checked = (item->state() == MythListButtonItem::FullChecked);

    if (!checked) {
        if (insertInDB(site))
        {
            site->inDB = true;
            item->setChecked(MythListButtonItem::FullChecked);
        }
    }
    else {
        if (removeFromDB(site))
        {
            site->inDB = false;
            item->setChecked(MythListButtonItem::NotChecked);
        }
    }
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


void MythFlixConfig::slotCategoryChanged(MythListButtonItem *item)
{
    if (!item)
        return;

    m_genresList->Reset();

    NewsCategory* cat = (NewsCategory*) item->getData();
    if (cat) {

        for (NewsSiteItem* site = cat->siteList.first();
             site; site = cat->siteList.next() ) {
            MythListButtonItem* item =
                new MythListButtonItem(m_genresList, site->name, 0, true,
                                      site->inDB ?
                                      MythListButtonItem::FullChecked :
                                      MythListButtonItem::NotChecked);
            item->setData(site);
        }
    }
}

bool MythFlixConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("NetFlix", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            GetMythMainWindow()->GetMainStack()->PopScreen();
        else
            handled = false;
    }

    return handled;
}

