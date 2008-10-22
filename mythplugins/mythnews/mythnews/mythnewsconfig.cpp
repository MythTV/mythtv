// QT Headers
#include <QApplication>
#include <QString>
#include <QFile>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/mythdirs.h>

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
};

// ---------------------------------------------------

MythNewsConfig::MythNewsConfig(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_priv(new MythNewsConfigPriv), m_categoriesList(NULL),
      m_siteList(NULL),               m_helpText(NULL),
      m_contextText(NULL),            //m_SpinBox(NULL),
      m_updateFreq(gContext->GetNumSetting("NewsUpdateFrequency", 30))
{
    populateSites();
}

MythNewsConfig::~MythNewsConfig()
{
    delete m_priv;

//     if (m_SpinBox)
//     {
//         gContext->SaveSetting("NewsUpdateFrequency",
//                               m_SpinBox->value());
//     }
}

void MythNewsConfig::populateSites()
{
    QString filename = GetShareDir()
                       + "mythnews/news-sites.xml";
    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly))
    {
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
    for (int i = 0; i < catList.count(); i++)
    {
        catNode = catList.item(i);

        NewsCategory cat;
        cat.name = catNode.toElement().attribute("name");

        m_priv->categoryList.push_back(cat);

        QDomNodeList siteList = catNode.childNodes();

        for (int j = 0; j < siteList.count(); j++)
        {
            siteNode = siteList.item(j);

            NewsSiteItem site = NewsSiteItem();
            site.name =
                siteNode.namedItem(QString("title")).toElement().text();
            site.category =
                cat.name;
            site.url =
                siteNode.namedItem(QString("url")).toElement().text();
            site.ico =
                siteNode.namedItem(QString("ico")).toElement().text();

            site.inDB = findInDB(site.name);

            cat.siteList.push_back(site);
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

    m_categoriesList = dynamic_cast<MythUIButtonList *>
                (GetChild("category"));

    m_siteList = dynamic_cast<MythUIButtonList *>
                (GetChild("sites"));

    m_helpText = dynamic_cast<MythUIText *>
                (GetChild("help"));

    if (!m_categoriesList || !m_siteList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_categoriesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotCategoryChanged(MythUIButtonListItem*)));
    connect(m_siteList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(toggleItem(MythUIButtonListItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_categoriesList);
    m_categoriesList->SetActive(true);
    m_siteList->SetActive(false);

    loadData();

//    if (m_helpText)
//    {
//        m_helpText->SetText(tr("Set update frequency by using the up/down arrows."
//                          "Minimum value is 30 Minutes."));
//    }

//     m_SpinBox = new MythNewsSpinBox(this);
//     m_SpinBox->setRange(30,1000);
//     m_SpinBox->setLineStep(10);

    return true;
}

void MythNewsConfig::loadData()
{
    NewsCategory::List::iterator it = m_priv->categoryList.begin();
    for (; it != m_priv->categoryList.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_categoriesList, (*it).name);
        item->setData(&(*it));
        if (!(*it).siteList.empty())
            item->setDrawArrow(true);
    }
    slotCategoryChanged(m_categoriesList->GetItemFirst());
}

void MythNewsConfig::toggleItem(MythUIButtonListItem *item)
{
    if (!item || !item->getData())
        return;

    NewsSiteItem *site = (NewsSiteItem*) item->getData();

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
    if (!item)
        return;

    m_siteList->Reset();

    NewsCategory *cat = (NewsCategory*) item->getData();
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
        newitem->setData(&(*it));
    }
}

bool MythNewsConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
