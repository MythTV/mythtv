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

// QT Headers
#include <qapplication.h>
#include <q3ptrlist.h>
#include <qstring.h>
#include <qfile.h>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythNews headers
#include "mythnewsconfig.h"
#include "newsdbutil.h"

using namespace std;

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

MythNewsConfig::MythNewsConfig(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name)
{
    m_priv            = new MythNewsConfigPriv;
    m_updateFreq      = gContext->GetNumSetting("NewsUpdateFrequency", 30);

    // Create the database if not exists
    QString queryString( "CREATE TABLE IF NOT EXISTS newssites "
                         "( name VARCHAR(100) NOT NULL PRIMARY KEY,"
                         "  category  VARCHAR(255) NOT NULL,"
                         "  url  VARCHAR(255) NOT NULL,"
                         "  ico  VARCHAR(255),"
                         "  updated INT UNSIGNED );");

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec(queryString)) {
        VERBOSE(VB_IMPORTANT, "MythNewsConfig: Error in creating sql table");
    }

//    m_SpinBox = NULL;
    m_siteList = m_categoriesList = NULL;

    populateSites();
}

MythNewsConfig::~MythNewsConfig()
{
    delete m_priv;

//     if (m_SpinBox) {
//         gContext->SaveSetting("NewsUpdateFrequency",
//                               m_SpinBox->value());
//     }
}

void MythNewsConfig::populateSites()
{
    QString filename = gContext->GetShareDir()
                       + "mythnews/news-sites.xml";
    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly)) {
        VERBOSE(VB_IMPORTANT, "MythNews: Cannot open news-sites.xml");
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    QDomDocument domDoc;

    if (!domDoc.setContent(&xmlFile, false, &errorMsg, &errorLine, &errorColumn)) 
    {
        VERBOSE(VB_IMPORTANT, "MythNews: Error in reading content of news-sites.xml");
        VERBOSE(VB_IMPORTANT, QString("MythNews: Error, parsing %1\n"
                                      "at line: %2  column: %3 msg: %4")
                                      .arg(filename).arg(errorLine)
                                      .arg(errorColumn).arg(errorMsg));
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

bool MythNewsConfig::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("news-ui.xml", "config", this);

    if (!foundtheme)
        return false;

    m_categoriesList = dynamic_cast<MythListButton *>
                (GetChild("category"));

    m_siteList = dynamic_cast<MythListButton *>
                (GetChild("sites"));

    m_helpText = dynamic_cast<MythUIText *>
                (GetChild("help"));

    if (!m_categoriesList || !m_siteList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_categoriesList, SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT(slotCategoryChanged(MythListButtonItem*)));
    connect(m_siteList, SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(toggleItem(MythListButtonItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_categoriesList);
    m_categoriesList->SetActive(true);
    m_siteList->SetActive(false);

    loadData();

    if (m_helpText) {
        m_helpText->SetText(tr("Set update frequency by using the up/down arrows."
                          "Minimum value is 30 Minutes."));
    }

//     m_SpinBox = new MythNewsSpinBox(this);
//     m_SpinBox->setRange(30,1000);
//     m_SpinBox->setLineStep(10);

    return true;
}

void MythNewsConfig::loadData()
{

    for (NewsCategory* cat = m_priv->categoryList.first();
         cat; cat = m_priv->categoryList.next() ) {
        MythListButtonItem* item =
            new MythListButtonItem(m_categoriesList, cat->name);
        item->setData(cat);
    }
    slotCategoryChanged(m_categoriesList->GetItemFirst());

}

void MythNewsConfig::toggleItem(MythListButtonItem *item)
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

void MythNewsConfig::slotCategoryChanged(MythListButtonItem *item)
{
    if (!item)
        return;

    m_siteList->Reset();

    NewsCategory* cat = (NewsCategory*) item->getData();
    if (cat) {

        for (NewsSiteItem* site = cat->siteList.first();
             site; site = cat->siteList.next() ) {
            MythListButtonItem* item =
                new MythListButtonItem(m_siteList, site->name, 0, true,
                                      site->inDB ?
                                      MythListButtonItem::FullChecked :
                                      MythListButtonItem::NotChecked);
            item->setData(site);
        }
    }
}

bool MythNewsConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            NextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            NextPrevWidgetFocus(true);
        }
        else if (action == "ESCAPE")
            GetMythMainWindow()->GetMainStack()->PopScreen();
        else
            handled = false;
    }

    return handled;
}
