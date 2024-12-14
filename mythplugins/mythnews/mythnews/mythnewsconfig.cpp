// QT Headers
#include <QFile>
#include <QStringList>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitext.h>

// MythNews headers
#include "mythnewsconfig.h"
#include "newsdbutil.h"
#include "newssite.h"

#define LOC      QString("MythNewsConfig: ")
#define LOC_WARN QString("MythNewsConfig, Warning: ")
#define LOC_ERR  QString("MythNewsConfig, Error: ")

// ---------------------------------------------------

class MythNewsConfigPriv
{
  public:
    NewsCategory::List m_categoryList;
    QStringList        m_selectedSitesList;
};

// ---------------------------------------------------

MythNewsConfig::MythNewsConfig(MythScreenStack *parent, const QString &name)
    : MythScreenType(parent, name),
      m_priv(new MythNewsConfigPriv),
      m_updateFreq(gCoreContext->GetNumSetting("NewsUpdateFrequency", 30))
{
    populateSites();
}

MythNewsConfig::~MythNewsConfig()
{
    delete m_priv;
}

void MythNewsConfig::populateSites(void)
{
    QMutexLocker locker(&m_lock);

    QString filename = QString("%1%2")
        .arg(GetShareDir(), "mythnews/news-sites.xml");

    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot open news-sites.xml");
        return;
    }

    QDomDocument domDoc;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!domDoc.setContent(&xmlFile, false, &errorMsg,
                           &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not read content of news-sites.xml" +
                QString("\n\t\t\tError parsing %1").arg(filename) +
                QString("\n\t\t\tat line: %1  column: %2 msg: %3")
                .arg(errorLine).arg(errorColumn).arg(errorMsg));
        return;
    }
#else
    auto parseResult = domDoc.setContent(&xmlFile);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not read content of news-sites.xml" +
                QString("\n\t\t\tError parsing %1").arg(filename) +
                QString("\n\t\t\tat line: %1  column: %2 msg: %3")
                .arg(parseResult.errorLine).arg(parseResult.errorColumn)
                .arg(parseResult.errorMessage));
        return;
    }
#endif

    m_priv->m_categoryList.clear();

    QDomNodeList catList =
        domDoc.elementsByTagName(QString::fromUtf8("category"));

    QDomNode catNode;
    QDomNode siteNode;
    for (int i = 0; i < catList.count(); i++)
    {
        catNode = catList.item(i);

        NewsCategory cat;
        cat.m_name = catNode.toElement().attribute("name");

        QDomNodeList siteList = catNode.childNodes();

        for (int j = 0; j < siteList.count(); j++)
        {
            siteNode = siteList.item(j);

            NewsSiteItem site = NewsSiteItem();
            site.m_name = siteNode.namedItem(QString("title")).toElement().text();
            site.m_category = cat.m_name;
            site.m_url = siteNode.namedItem(QString("url")).toElement().text();
            site.m_ico = siteNode.namedItem(QString("ico")).toElement().text();
            site.m_podcast = false;
            site.m_inDB = findInDB(site.m_name);

            cat.m_siteList.push_back(site);
        }

        m_priv->m_categoryList.push_back(cat);
    }

    xmlFile.close();
}

bool MythNewsConfig::Create(void)
{
    QMutexLocker locker(&m_lock);

    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("news-ui.xml", "config", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_categoriesList, "category", &err);
    UIUtilE::Assign(this, m_siteList, "sites", &err);
    UIUtilW::Assign(this, m_helpText, "help", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'config'");
        return false;
    }

    connect(m_categoriesList, &MythUIButtonList::itemSelected,
            this, &MythNewsConfig::slotCategoryChanged);
    connect(m_siteList, &MythUIButtonList::itemClicked,
            this, &MythNewsConfig::toggleItem);

    BuildFocusList();

    SetFocusWidget(m_categoriesList);

    loadData();

    return true;
}

void MythNewsConfig::loadData(void)
{
    QMutexLocker locker(&m_lock);

    for (auto & category : m_priv->m_categoryList)
    {
        auto *item = new MythUIButtonListItem(m_categoriesList,category.m_name);
        item->SetData(QVariant::fromValue(&category));
        if (!category.m_siteList.empty())
            item->setDrawArrow(true);
    }
    slotCategoryChanged(m_categoriesList->GetItemFirst());
}

void MythNewsConfig::toggleItem(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item )
        return;

    auto *site = item->GetData().value<NewsSiteItem*>();
    if (!site)
        return;

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked)
    {
        if (insertInDB(site))
        {
            site->m_inDB = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else
    {
        if (removeFromDB(site))
        {
            site->m_inDB = false;
            item->setChecked(MythUIButtonListItem::NotChecked);
        }
    }
}

void MythNewsConfig::slotCategoryChanged(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item)
        return;

    m_siteList->Reset();

    auto *cat = item->GetData().value<NewsCategory*>();
    if (!cat)
        return;

    for (auto & site : cat->m_siteList)
    {
        auto *newitem =
            new MythUIButtonListItem(m_siteList, site.m_name, nullptr, true,
                                     site.m_inDB ?
                                     MythUIButtonListItem::FullChecked :
                                     MythUIButtonListItem::NotChecked);
        newitem->SetData(QVariant::fromValue(&site));
    }
}

bool MythNewsConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
