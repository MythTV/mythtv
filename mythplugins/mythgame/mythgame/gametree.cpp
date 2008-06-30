#include <qapplication.h>
#include <qpushbutton.h>
//Added by qt3to4:
#include <QSqlQuery>
#include <QKeyEvent>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

#include "gametree.h"
#include "gamehandler.h"

#define LOC_ERR QString("MythGame:GAMETREE Error: ")
#define LOC QString("MythGame:GAMETREE: ")

QString GameTreeItem::getFillSql(QString layer) const
{
    unsigned childDepth = m_depth + 1;
    bool childIsLeaf = childDepth == m_root->getDepth();
    QString childLevel = m_root->getLevel(childDepth - 1);
    QString columns;

    QString filter = m_root->getFilter();
    QString conj = "where ";

    if (!filter.isEmpty())
    {
        filter = conj + filter;
        conj = " and ";
    }
    if ((childLevel == "gamename") && (m_gameShowFileName))
    {
        columns = childIsLeaf
                    ? "romname,system,year,genre,gamename"
                    : "romname";

        if (m_showHashed)
            filter += " and romname like '" + layer + "%'";

    }
    else if ((childLevel == "gamename") && (layer.length() == 1)) {
        columns = childIsLeaf
                    ? childLevel + ",system,year,genre,gamename"
                    : childLevel;

        if (m_showHashed) 
            filter += " and gamename like '" + layer + "%'";

    }
    else if (childLevel == "hash") {
        columns = "left(gamename,1)";
    }
    else {

        columns = childIsLeaf
                    ? childLevel + ",system,year,genre,gamename"
                    : childLevel;
    }

    //  this whole section ought to be in rominfo.cpp really, but I've put it
    //  in here for now to minimise the number of files changed by this mod
    if (m_romInfo) {
    
        if (!m_romInfo->System().isEmpty())
        {
            filter += conj + "trim(system)=:SYSTEM";
            conj = " and ";
        }
        if (m_romInfo->Year() != "")
        {
            filter += conj + "year=:YEAR";
            conj = " and ";
        }
        if (!m_romInfo->Genre().isEmpty())
        {
            filter += conj + "trim(genre)=:GENRE";
            conj = " and ";
        }
        if (!m_romInfo->Publisher().isEmpty())
        {
            filter += conj + "publisher=:PUBLISHER";
            conj = " and ";
        }

        if (!m_romInfo->Gamename().isEmpty())
        {
            filter += conj + "trim(gamename)=:GAMENAME";
        }

    }

    filter += conj + " display = 1 ";

    QString sql;

    if ((childLevel == "gamename") && (m_gameShowFileName))
    {   
        sql = "select distinct "
                + columns
                + " from gamemetadata "
                + filter
                + " order by romname"
                + ";";
    }
    else if (childLevel == "hash") {
        sql = "select distinct "
                + columns
                + " from gamemetadata "
                + filter
                + " order by gamename,romname"
                + ";";
    }
    else
    {
        sql = "select distinct "
                + columns
                + " from gamemetadata "
                + filter
                + " order by "
                + childLevel
                + ";";
    }

    return sql;
}

GameTreeItem* GameTreeItem::createChild(QSqlQuery *query) const
{
    GameTreeItem *childItem = new GameTreeItem(m_root, m_showHashed);
    childItem->m_depth = m_depth + 1;

    QString current = query->value(0).toString().stripWhiteSpace();
    if (childItem->isLeaf())
    {
        RomInfo temp;
        temp.setSystem(query->value(1).toString().stripWhiteSpace());
        childItem->m_romInfo = GameHandler::CreateRomInfo(&temp);

        childItem->m_romInfo->setSystem(temp.System());
        childItem->m_romInfo->setYear(query->value(2).toString());
        childItem->m_romInfo->setGenre(query->value(3).toString().stripWhiteSpace());
        childItem->m_romInfo->setGamename(query->value(4).toString().stripWhiteSpace());
    }
    else
    {
        childItem->m_romInfo = m_romInfo
                             ? new RomInfo(*m_romInfo)
                             : new RomInfo();
        if (childItem->getLevel() != "hash")
            childItem->m_romInfo->setField(childItem->getLevel(), current);

    }

    return childItem;
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
    int pos = 0;

    m_gameTree = new GenericTree("game root", 0, false);

    timer = new QTimer( this ); 
    connect( timer, SIGNAL(timeout()),  SLOT(showImageTimeout()) ); 

    wireUpTheme();
    //  create system filter to only select games where handlers are present
    QString systemFilter;

    // The call to GameHandler::count() fills the handler list for us
    // to move through.
    unsigned handlercount = GameHandler::count();

    for (unsigned i = 0; i < handlercount; ++i)
    {
        QString system = GameHandler::getHandler(i)->SystemName();
        if (i == 0)
            systemFilter = "system in ('" + system + "'";
        else
            systemFilter += ",'" + system + "'";
    }
    if (systemFilter.isEmpty())
    {
        systemFilter = "1=0";
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find any game handlers!"));
    }
    else
        systemFilter += ")";

    m_showHashed = gContext->GetSetting("GameTreeView").toInt();

    //  create a few top level nodes - this could be moved to a config based
    //  approach with multiple roots if/when someone has the time to create
    //  the relevant dialog screens

    levels = gContext->GetSetting("GameFavTreeLevels");
    root = new GameTreeRoot(levels, systemFilter + " and favorite=1");
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("Favourites"), m_gameTreeItems.size(), false);
    m_favouriteNode = node;

    levels = gContext->GetSetting("GameAllTreeLevels");

    if (m_showHashed) {
        pos = levels.find("gamename",0);

        if (pos != -1) 
            levels.insert(pos, " hash ");

    }

    root = new GameTreeRoot(levels, systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root, m_showHashed));
    node = m_gameTree->addNode(tr("All Games"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("genre gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Genre"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("year gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Year"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Name"), m_gameTreeItems.size(), false);

    root = new GameTreeRoot("publisher gamename", systemFilter);
    m_gameTreeRoots.push_back(root);
    m_gameTreeItems.push_back(new GameTreeItem(root));
    node = m_gameTree->addNode(tr("-   By Publisher"), m_gameTreeItems.size(), false);


    m_gameTreeUI->assignTreeData(m_gameTree);
    m_gameTreeUI->enter();
    m_gameTreeUI->pushDown();

    updateForeground();
}

GameTree::~GameTree()
{
    delete m_gameTree;
}

void GameTree::updateRomInfo(RomInfo *rom) {

    m_gameTitle->SetText(rom->Gamename());
    m_gameSystem->SetText(rom->AllSystems());
    m_gameYear->SetText(rom->Year());
    m_gameGenre->SetText(rom->Genre());

    if (rom->Favorite())
        m_gameFavourite->SetText("Yes");
    else
        m_gameFavourite->SetText("No");

    if (!rom->ImagePath().isEmpty())
        m_gameImage->SetImage(rom->ImagePath());

}

void GameTree::clearRomInfo(void) {
    m_gameTitle->SetText("");
    m_gameSystem->SetText("");
    m_gameYear->SetText("");
    m_gameGenre->SetText("");
    m_gameFavourite->SetText("");
    m_gameImage->SetImage("");

    if (m_gameImage->isShown())
        m_gameImage->hide();

}

void GameTree::handleTreeListEntry(int nodeInt, IntVector *)
{
    GameTreeItem *item = nodeInt ? m_gameTreeItems[nodeInt - 1] : 0;
    RomInfo *romInfo = item ? item->getRomInfo() : 0;

    if (item && !item->isLeaf())
    {
        GenericTree *node = m_gameTreeUI->getCurrentNode();
        if (!node)
        {
            VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't get current node"));
            return;
        }
        if (!item->isFilled() || node == m_favouriteNode)
        {
            node->deleteAllChildren();
            fillNode(node);
        }
    } 

    if (romInfo)
    {
        if (item->isLeaf() && romInfo->Romname().isEmpty())
            romInfo->fillData();

        updateRomInfo(romInfo);

        if (item->isLeaf())
        {
            if (!romInfo->ImagePath().isEmpty()) 
            {
                if ( timer->isActive() ) 
                    timer->changeInterval(330); 
                else 
                    timer->start(330, true); 

            } 
        } 
        else {
            if ( timer->isActive() )
                timer->stop();   

            clearRomInfo();
        }
    }
    else 
    {   // Otherwise use some defaults.
        if ( timer->isActive() )
            timer->stop();

            clearRomInfo();
    }

}

void GameTree::showImageTimeout(void) 
{ 
    m_gameImage->LoadImage(); 
    if (!m_gameImage->isShown())
        m_gameImage->show();
} 


QString getElement(QStringList list, int pos)
{
    int curpos = 0;

    for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it ) {
        if (curpos == pos)
            return *it;
        curpos++;
    }
    return NULL;
}

void GameTree::handleTreeListSelection(int nodeInt, IntVector *)
{
    if (nodeInt > 0)
    {
        GameTreeItem *item = nodeInt ? m_gameTreeItems[nodeInt - 1] : 0;

        if (item->isLeaf())
        {
            if (item->getRomInfo()->RomCount() == 1)
                GameHandler::Launchgame(item->getRomInfo(),NULL);
            else if (item->getRomInfo()->RomCount() > 1)
            {
                QString all_systems = item->getRomInfo()->AllSystems();
                QStringList players = QStringList::split(",", all_systems);
                players += QObject::tr("Cancel");

                DialogCode val = MythPopupBox::ShowButtonPopup(
                    gContext->GetMainWindow(),
                    "", tr("Players Available. \n\n Please pick one."),
                    players, kDialogCodeButton0);

                int idx = MythDialog::CalcItemIndex(val);
                if ((0 <= idx) && (idx < ((int)players.size() - 1)))
                {
                    QString systemname = getElement(players, idx);
                    if (!systemname.isEmpty())
                        GameHandler::Launchgame(item->getRomInfo(),systemname);
                }
            } 
            raise();
            setActiveWindow();
        }
    }
}

void GameTree::showInfo(void)
{
    GenericTree *curNode = m_gameTreeUI->getCurrentNode();
    int i = curNode->getInt();
    GameTreeItem *curItem = i ? m_gameTreeItems[i - 1] : 0;
    if (curItem && curItem->getRomInfo() && curItem->isLeaf())
        curItem->showGameInfo(curItem->getRomInfo());
}

void GameTreeItem::showGameInfo(RomInfo *rom) 
{
    if (info_popup) 
        return;

    info_popup = new MythPopupBox(gContext->GetMainWindow(), "info_popup");

    //info_popup->addLabel(QObject::tr("Rom Information\n"));
    info_popup->addLabel(QString("Name: %1 (%2)")
                         .arg(rom->Gamename()).arg(rom->GameType()));
    info_popup->addLabel(QString("Rom : %1").arg(rom->Romname()));
    info_popup->addLabel(QString("CRC : %1").arg(rom->CRC_VALUE()));
    info_popup->addLabel(QString("Path: %1").arg(rom->Rompath()));
    //info_popup->addLabel(QString("Type: %1").arg(rom->GameType()));
    info_popup->addLabel(QString("Genre: %1").arg(rom->Genre()));
    info_popup->addLabel(QString("Year: %1").arg(rom->Year()));
    info_popup->addLabel(QString("Country: %1").arg(rom->Country()));
    info_popup->addLabel(QString("Publisher: %1").arg(rom->Publisher()));
    //info_popup->addLabel(QString("DiskCount : %1").arg(rom->DiskCount()));
    info_popup->addLabel(QString("Player(s): %1").arg(rom->AllSystems())); 

    OKButton = info_popup->addButton(QString("OK"), this,
                              SLOT(closeGameInfo()));
    OKButton->setFocus();
    info_popup->addButton(QString("EDIT"), this,
                              SLOT(edit()));

    info_popup->ShowPopup(this,SLOT(closeGameInfo()));
}

void GameTreeItem::closeGameInfo(void)
{
    if (info_popup)
    {
        info_popup->deleteLater();
        info_popup = NULL;
    }
}

void GameTreeItem::edit(void)
{
    if (m_romInfo)
    {
        m_romInfo->edit_rominfo();
    }

    if (info_popup)
    {
        info_popup->deleteLater();
        info_popup = NULL;
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
            showInfo();
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
        else if (action == "INCSEARCH")
            m_gameTreeUI->incSearchStart();
        else if (action == "INCSEARCHNEXT")
            m_gameTreeUI->incSearchNext();
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
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a gametreelist in your theme"));
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
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a text area gametitle"));

    m_gameSystem = getUITextType("systemname");
    if (!m_gameSystem)
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a text area systemname"));

    m_gameYear = getUITextType("yearname");
    if (!m_gameYear)
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a text area yearname"));

    m_gameGenre = getUITextType("genrename");
    if (!m_gameGenre)
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a text area genrename"));

    m_gameFavourite = getUITextType("showfavorite");
    if (!m_gameFavourite)
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find a text area showfavorite"));

    m_gameImage = getUIImageType("gameimage");
    if (!m_gameImage)
	VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find an imagetag gameimage"));
}

void GameTree::fillNode(GenericTree *node)
{
    int i = node->getInt();
    GameTreeItem* curItem = m_gameTreeItems[i - 1];
    QString layername = node->getString();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(curItem->getFillSql(layername));

    if (curItem->m_romInfo)
    {
        if (!curItem->m_romInfo->System().isEmpty())
            query.bindValue(":SYSTEM",  curItem->m_romInfo->System());
        else if (curItem->m_romInfo->Year() != "")
            query.bindValue(":YEAR", curItem->m_romInfo->Year());
        else if (!curItem->m_romInfo->Genre().isEmpty())
            query.bindValue(":GENRE", curItem->m_romInfo->Genre());
        else if (!curItem->m_romInfo->Publisher().isEmpty())
            query.bindValue(":PUBLISHER", curItem->m_romInfo->Publisher());
        else if (!curItem->m_romInfo->Gamename().isEmpty())
            query.bindValue(":GAMENAME", curItem->m_romInfo->Gamename());
    }

    query.exec();

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            GameTreeItem *childItem = curItem->createChild(&query);
            m_gameTreeItems.push_back(childItem);
            node->addNode(query.value(0).toString().stripWhiteSpace(),
                              m_gameTreeItems.size(), childItem->isLeaf());
        }
    }
    curItem->setFilled(true);
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
