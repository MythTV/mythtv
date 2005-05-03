#include <qapplication.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include "gametree.h"
#include "gamehandler.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

/**************************************************************************
    GameTreeRoot - helper class holding tree root details
 **************************************************************************/

class GameTreeRoot
{
  public:
    GameTreeRoot(const QString& levels, const QString& filter)
      : m_levels(QStringList::split(" ", levels))
      , m_filter(filter)
    {
    }

    ~GameTreeRoot()
    {
    }

    unsigned getDepth() const                   { return m_levels.size(); }
    const QString& getLevel(unsigned i) const   { return m_levels[i]; }
    const QString& getFilter() const            { return m_filter; }

  private:
    QStringList m_levels;
    QString m_filter;
};

/**************************************************************************
    GameTreeItem - helper class supplying data and methods to each node
 **************************************************************************/

class GameTreeItem
{
  public:
    GameTreeItem(GameTreeRoot* root)
      : m_root(root)
      , m_romInfo(0)
      , m_depth(0)
      , m_isFilled(false)
    {
    }

    ~GameTreeItem()
    {
        if (m_romInfo)
            delete m_romInfo;
    }

    bool isFilled() const             { return m_isFilled; }
    bool isLeaf() const               { return m_depth == m_root->getDepth(); }

    const QString& getLevel() const   { return m_root->getLevel(m_depth - 1); }
    RomInfo* getRomInfo() const       { return m_romInfo; }
    QString getFillSql() const;

    void setFilled(bool isFilled)     { m_isFilled = isFilled; }

    GameTreeItem* createChild(const QSqlQuery& query) const;
    void editSettings() const;

  private:
    GameTreeRoot* m_root;
    RomInfo* m_romInfo;
    unsigned m_depth;
    bool m_isFilled;
};

QString GameTreeItem::getFillSql() const
{
    unsigned childDepth = m_depth + 1;
    bool childIsLeaf = childDepth == m_root->getDepth();
    QString childLevel = m_root->getLevel(childDepth - 1);

    QString columns = childIsLeaf
                    ? childLevel + ",system,year,genre,gamename"
                    : childLevel;

    QString filter = m_root->getFilter();
    QString conj = "where ";

    if (!filter.isEmpty())
    {
        filter = conj + filter;
        conj = " and ";
    }

    //  this whole section ought to be in rominfo.cpp really, but I've put it
    //  in here for now to minimise the number of files changed by this mod
    if (m_romInfo)
    {
        if (!m_romInfo->System().isEmpty())
        {
            filter += conj + "trim(system)='" + m_romInfo->System() + "'";
            conj = " and ";
        }
        if (m_romInfo->Year() != 0)
        {
            filter += conj + "year=" + QString::number(m_romInfo->Year());
            conj = " and ";
        }
        if (!m_romInfo->Genre().isEmpty())
        {
            filter += conj + "trim(genre)='" + m_romInfo->Genre() + "'";
            conj = " and ";
        }
        if (!m_romInfo->Gamename().isEmpty())
        {
            filter += conj + "trim(gamename)='" + m_romInfo->Gamename() + "'";
        }
    }

    QString sql = "select distinct "
                + columns
                + " from gamemetadata "
                + filter
                + " order by "
                + childLevel
                + ";";

    return sql;
}

GameTreeItem* GameTreeItem::createChild(const QSqlQuery& query) const
{
    GameTreeItem *childItem = new GameTreeItem(m_root);
    childItem->m_depth = m_depth + 1;

    QString current = query.value(0).toString().stripWhiteSpace();
    if (childItem->isLeaf())
    {
        RomInfo temp;
        temp.setSystem(query.value(1).toString().stripWhiteSpace());
        childItem->m_romInfo = GameHandler::CreateRomInfo(&temp);

        childItem->m_romInfo->setSystem(temp.System());
        childItem->m_romInfo->setYear(query.value(2).toInt());
        childItem->m_romInfo->setGenre(query.value(3).toString().stripWhiteSpace());
        childItem->m_romInfo->setGamename(query.value(4).toString().stripWhiteSpace());
    }
    else
    {
        childItem->m_romInfo = m_romInfo
                             ? new RomInfo(*m_romInfo)
                             : new RomInfo();
        childItem->m_romInfo->setField(childItem->getLevel(), current);
    }

    return childItem;
}

void GameTreeItem::editSettings() const
{
    QString level = getLevel();

    if (level == "system")
        GameHandler::EditSystemSettings(m_romInfo);
    else if (level == "gamename")
        GameHandler::EditSettings(m_romInfo);
}

/**************************************************************************
    GameTree - main game tree class
 **************************************************************************/

GameTree::GameTree(MythMainWindow *parent, QString windowName,
                   QString themeFilename, const char *name)
        : MythThemedDialog(parent, windowName, themeFilename, name)
{
    QString levels;
    GameTreeRoot *root;
    GenericTree *node;

    m_gameTree = new GenericTree("game root", 0, false);

    wireUpTheme();

    //  create system filter to only select games where handlers are present
    QString systemFilter;
    for (unsigned i = 0; i < GameHandler::count(); ++i)
    {
        QString system = GameHandler::getHandler(i)->Systemname();
        if (i == 0)
            systemFilter = "system in ('" + system + "'";
        else
            systemFilter += ",'" + system + "'";
    }
    if (systemFilter.isEmpty())
    {
        systemFilter = "1=0";
        cerr << "gametree.o: Couldn't find any game handlers" << endl;
    }
    else
        systemFilter += ")";

    //  create a few top level nodes - this could be moved to a config based
    //  approach with multiple roots if/when someone has the time to create
    //  the relevant dialog screens
    levels = gContext->GetSetting("GameAllTreeLevels");
    root = new GameTreeRoot(levels, systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("All Games"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Name"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("year gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Year"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("system gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By System"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("genre gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Genre"), m_gameTreeItems.size(), false);
    levels = gContext->GetSetting("GameFavTreeLevels");
    root = new GameTreeRoot(levels, systemFilter + " and favorite=1");
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("Favourites"), m_gameTreeItems.size(), false);

    m_gameTreeUI->assignTreeData(m_gameTree);
    m_gameTreeUI->enter();
    m_gameTreeUI->pushDown();

    updateForeground();
}

GameTree::~GameTree()
{
    delete m_gameTree;
}

void GameTree::handleTreeListEntry(int nodeInt, IntVector *)
{
    m_gameImage->SetImage("");
    m_gameTitle->SetText("");
    m_gameSystem->SetText("");
    m_gameYear->SetText("");
    m_gameGenre->SetText("");
    m_gameFavourite->SetText("");

    GameTreeItem *item = nodeInt ? m_gameTreeItems[nodeInt - 1] : 0;
    RomInfo *romInfo = item ? item->getRomInfo() : 0;

    if (item && !item->isLeaf() && !item->isFilled())
    {
        GenericTree *node = m_gameTreeUI->getCurrentNode();
        if (!node)
        {
            cerr << "gametree.o: Couldn't get current node\n";
            return;
        }
        fillNode(node);
    }

    if (romInfo)
    {
        if (item->isLeaf() && romInfo->Romname().isEmpty())
            romInfo->fillData();

        if (!romInfo->Gamename().isEmpty())
            m_gameTitle->SetText(romInfo->Gamename());

        if (!romInfo->System().isEmpty())
            m_gameSystem->SetText(romInfo->System());

        if (romInfo->Year() > 0)
            m_gameYear->SetText(QString::number(romInfo->Year()));

        if (!romInfo->Genre().isEmpty())
            m_gameGenre->SetText(romInfo->Genre());

        if (item->isLeaf())
        {
            if (romInfo->Favorite())
                m_gameFavourite->SetText("Yes");
            else
                m_gameFavourite->SetText("No");

            QString imagename;
            if (romInfo->FindImage("screenshot", &imagename))
                m_gameImage->SetImage(imagename);
        }
    }

    m_gameImage->LoadImage();
}

void GameTree::handleTreeListSelection(int nodeInt, IntVector *)
{
    if (nodeInt > 0)
    {
        GameTreeItem *item = m_gameTreeItems[nodeInt - 1];

        if (item->isLeaf())
        {
            GameHandler::Launchgame(item->getRomInfo());
            raise();
            setActiveWindow();
        }
    }
}

void GameTree::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Game", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            m_gameTreeUI->select();
        else if (action == "MENU" || action == "INFO")
            edit();
        else if (action == "UP")
            m_gameTreeUI->moveUp();
        else if (action == "DOWN")
            m_gameTreeUI->moveDown();
        else if (action == "LEFT")
            m_gameTreeUI->popUp();
        else if (action == "RIGHT")
            m_gameTreeUI->pushDown();
        else if (action == "PAGEUP")
            m_gameTreeUI->pageUp();
        else if (action == "PAGEDOWN")
            m_gameTreeUI->pageDown();
        else if (action == "TOGGLEFAV")
            toggleFavorite();
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void GameTree::wireUpTheme(void)
{
    m_gameTreeUI = getUIManagedTreeListType("gametreelist");
    if (!m_gameTreeUI)
    {
        cerr << "gametree.o: Couldn't find a gametreelist in your theme" << endl;
        exit(0);
    }
    m_gameTreeUI->showWholeTree(true);
    m_gameTreeUI->colorSelectables(true);

    connect(m_gameTreeUI, SIGNAL(nodeSelected(int, IntVector*)),
            this, SLOT(handleTreeListSelection(int, IntVector*)));
    connect(m_gameTreeUI, SIGNAL(nodeEntered(int, IntVector*)),
            this, SLOT(handleTreeListEntry(int, IntVector*)));

    m_gameTitle = getUITextType("gametitle");
    if (!m_gameTitle)
        cerr << "gametree.o: Couldn't find a text area gametitle\n";

    m_gameSystem = getUITextType("systemname");
    if (!m_gameSystem)
        cerr << "gametree.o: Couldn't find a text area systemname\n";

    m_gameYear = getUITextType("yearname");
    if (!m_gameYear)
        cerr << "gametree.o: Couldn't find a text area yearname\n";

    m_gameGenre = getUITextType("genrename");
    if (!m_gameGenre)
        cerr << "gametree.o: Couldn't find a text area genrename\n";

    m_gameFavourite = getUITextType("showfavorite");
    if (!m_gameFavourite)
        cerr << "gametree.o: Couldn't find a text area showfavorite\n";

    m_gameImage = getUIImageType("gameimage");
    if (!m_gameImage)
        cerr << "gametree.o: Couldn't find an image gameimage\n";
}

void GameTree::fillNode(GenericTree *node)
{
    int i = node->getInt();
    GameTreeItem* curItem = m_gameTreeItems[i - 1];

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(curItem->getFillSql());

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            GameTreeItem *childItem = curItem->createChild(query);
            m_gameTreeItems.push_back(childItem);
            node->addNode(query.value(0).toString().stripWhiteSpace(),
                              m_gameTreeItems.size(), childItem->isLeaf());
        }
    }
    curItem->setFilled(true);
}

void GameTree::edit(void)
{
    GenericTree *curNode = m_gameTreeUI->getCurrentNode();
    int i = curNode->getInt();
    GameTreeItem *curItem = i ? m_gameTreeItems[i - 1] : 0;

    if (curItem)
        curItem->editSettings();
}

void GameTree::toggleFavorite(void)
{
    GenericTree *node = m_gameTreeUI->getCurrentNode();
    int i = node->getInt();
    GameTreeItem *item = i ? m_gameTreeItems[i - 1] : 0;

    if (item && item->isLeaf())
    {
        item->getRomInfo()->setFavorite();
        if (item->getRomInfo()->Favorite())
            m_gameFavourite->SetText("Yes");
        else
            m_gameFavourite->SetText("No");
    }
}
