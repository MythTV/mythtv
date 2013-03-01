// QT Headers
#include <QStringList>
#include <QFile>

// MythTV headers
#include <mythuitext.h>
#include <mythuibuttonlist.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythmainwindow.h>
#include <mythdirs.h>

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
    NewsCategory::List categoryList;
    QStringList selectedSitesList;
};

// ---------------------------------------------------

MythNewsConfig::MythNewsConfig(MythScreenStack *parent, const QString &name)
    : MythScreenType(parent, name),
      m_lock(QMutex::Recursive),
      m_priv(new MythNewsConfigPriv), m_categoriesList(NULL),
      m_siteList(NULL),               m_helpText(NULL),
      m_contextText(NULL),
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
        .arg(GetShareDir()).arg("mythnews/news-sites.xml");

    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot open news-sites.xml");
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    QDomDocument domDoc;

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

    m_priv->categoryList.clear();

    QDomNodeList catList =
        domDoc.elementsByTagName(QString::fromUtf8("category"));

    QDomNode catNode;
    QDomNode siteNode;
    for (int i = 0; i < catList.count(); i++)
    {
        catNode = catList.item(i);

        NewsCategory cat;
        cat.name = catNode.toElement().attribute("name");

        QDomNodeList siteList = catNode.childNodes();

        for (int j = 0; j < siteList.count(); j++)
        {
            siteNode = siteList.item(j);

            NewsSiteItem site = NewsSiteItem();
            site.name = siteNode.namedItem(QString("title")).toElement().text();
            site.category = cat.name;
            site.url = siteNode.namedItem(QString("url")).toElement().text();
            site.ico = siteNode.namedItem(QString("ico")).toElement().text();
            site.podcast = false;
            site.inDB = findInDB(site.name);

            cat.siteList.push_back(site);
        }

        m_priv->categoryList.push_back(cat);
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

    connect(m_categoriesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotCategoryChanged(MythUIButtonListItem*)));
    connect(m_siteList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(toggleItem(MythUIButtonListItem*)));

    BuildFocusList();

    SetFocusWidget(m_categoriesList);

    loadData();

    return true;
}

void MythNewsConfig::loadData(void)
{
    QMutexLocker locker(&m_lock);

    NewsCategory::List::iterator it = m_priv->categoryList.begin();
    for (; it != m_priv->categoryList.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_categoriesList, (*it).name);
        item->SetData(qVariantFromValue(&(*it)));
        if (!(*it).siteList.empty())
            item->setDrawArrow(true);
    }
    slotCategoryChanged(m_categoriesList->GetItemFirst());
}

void MythNewsConfig::toggleItem(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item )
        return;

    NewsSiteItem *site = qVariantValue<NewsSiteItem*>(item->GetData());
    if (!site)
        return;

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked)
    {
        if (insertInDB(site))
        {
            site->inDB = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else
    {
        if (removeFromDB(site))
        {
            site->inDB = false;
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

    NewsCategory *cat = qVariantValue<NewsCategory*>(item->GetData());
    if (!cat)
        return;

    NewsSiteItem::List::iterator it = cat->siteList.begin();
    for (; it != cat->siteList.end(); ++it)
    {
        MythUIButtonListItem *newitem =
            new MythUIButtonListItem(m_siteList, (*it).name, 0, true,
                                     (*it).inDB ?
                                     MythUIButtonListItem::FullChecked :
                                     MythUIButtonListItem::NotChecked);
        newitem->SetData(qVariantFromValue(&(*it)));
    }
}

bool MythNewsConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
