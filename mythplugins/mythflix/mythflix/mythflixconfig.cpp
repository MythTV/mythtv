
// QT headers
#include <QApplication>
#include <Q3PtrList>
#include <QString>
#include <QFile>
#include <QDomDocument>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdb.h>
#include <mythtv/mythverbose.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/mythdirs.h>

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

Q_DECLARE_METATYPE(NewsSiteItem *)

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

Q_DECLARE_METATYPE(NewsCategory *)

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
    QString filename = GetShareDir()
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
        return false;

    m_genresList = dynamic_cast<MythUIButtonList *>
                (GetChild("sites"));

    m_siteList = dynamic_cast<MythUIButtonList *>
                (GetChild("category"));

    if (!m_genresList || !m_siteList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_siteList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotCategoryChanged(MythUIButtonListItem*)));
    connect(m_genresList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(toggleItem(MythUIButtonListItem*)));

    m_siteList->SetCanTakeFocus(false);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_genresList);

    loadData();

    return true;
}

void MythFlixConfig::loadData()
{

    for (NewsCategory* cat = m_priv->categoryList.first();
         cat; cat = m_priv->categoryList.next() ) {
        MythUIButtonListItem* item =
            new MythUIButtonListItem(m_siteList, cat->name);
        item->SetData(qVariantFromValue(cat));
    }
    slotCategoryChanged(m_siteList->GetItemFirst());

}

void MythFlixConfig::toggleItem(MythUIButtonListItem *item)
{
    if (!item || item->GetData().isNull())
        return;

    NewsSiteItem* site = qVariantValue<NewsSiteItem *>(item->GetData());

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked) {
        if (insertInDB(site))
        {
            site->inDB = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else {
        if (removeFromDB(site))
        {
            site->inDB = false;
            item->setChecked(MythUIButtonListItem::NotChecked);
        }
    }
}

bool MythFlixConfig::findInDB(const QString& name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM netflix WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("new find in db", query);
        return false;
    }

    return query.size() > 0;
}

bool MythFlixConfig::insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    if (findInDB(site->name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO netflix (name,category,url,ico, is_queue) "
                  " VALUES( :NAME, :CATEGORY, :URL, :ICON, 0);");
    query.bindValue(":NAME", site->name);
    query.bindValue(":CATEGORY", site->category);
    query.bindValue(":URL", site->url);
    query.bindValue(":ICON", site->ico);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netlix: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool MythFlixConfig::removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM netflix WHERE name = :NAME ;");
    query.bindValue(":NAME", site->name);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("netflix: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}


void MythFlixConfig::slotCategoryChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_genresList->Reset();

    NewsCategory* cat = qVariantValue<NewsCategory *>(item->GetData());
    if (cat)
    {
        for (NewsSiteItem* site = cat->siteList.first();
             site; site = cat->siteList.next() )
        {
            MythUIButtonListItem* newItem =
                new MythUIButtonListItem(m_genresList, site->name, 0, true,
                                      site->inDB ?
                                      MythUIButtonListItem::FullChecked :
                                      MythUIButtonListItem::NotChecked);
            newItem->SetData(qVariantFromValue(site));
        }
    }
}
