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

GameTree::GameTree(MythMainWindow *parent, QString window_name, 
                   QString theme_filename, QString &paths, const char *name)
        : MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_paths = paths;
    m_pathlist = QStringList::split(" ", m_paths);
    showfavs = gContext->GetSetting("GameShowFavorites");

    wireUpTheme();

    game_tree_root = new GenericTree("game root", 0, false);
    game_tree_data = game_tree_root->addNode(tr("All Games"), 0, false);

    buildGameList();

    game_tree_list->enter();
    updateForeground();
}

GameTree::~GameTree()
{
    delete game_tree_root;
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
            game_tree_list->select();
        else if (action == "MENU" || action == "INFO") 
            edit();
        else if (action == "UP")
            game_tree_list->moveUp();
        else if (action == "DOWN")
            game_tree_list->moveDown();
        else if (action == "LEFT")
            game_tree_list->popUp();
        else if (action == "RIGHT")
            goRight();
        else if (action == "PAGEUP")
            game_tree_list->pageUp();
        else if (action == "PAGEDOWN")
            game_tree_list->pageDown();
        else if (action == "TOGGLEFAV")
            toggleFavorite();
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void GameTree::buildGameList(void)
{
    QString first = m_pathlist.front();

    QStringList regSystems;
    if (first == "system")
    {
        for (uint i = 0; i < GameHandler::count(); ++i)
            regSystems.append(GameHandler::getHandler(i)->Systemname());
    }

    bool isleaf = (first == "gamename");
    QString selcols = isleaf ? "gamename, system" : first;

    QString thequery = QString("SELECT DISTINCT %1 FROM gamemetadata "
                               "ORDER BY %2;").arg(selcols).arg(first);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            // Don't display non-registered systems, even if they are in the
            // database.

            if (first == "system" &&
                (regSystems.find(current) == regSystems.end()))
                continue;

            RomInfo* rinfo;
            if (isleaf)
            {
                //  no guarantee System has been set so create a temp RomInfo so
                //  that CreateRomInfo can create the correct subtype
                RomInfo* temp = new RomInfo();
                temp->setSystem(query.value(1).toString());
                rinfo = GameHandler::CreateRomInfo(temp);
                delete temp;

                rinfo->setGamename(query.value(0).toString());
                rinfo->setSystem(query.value(1).toString());
                rinfo->fillData();
            }
            else
            {
                rinfo = new RomInfo();
                rinfo->setField(first, current);
            }

            GameTreeItem *titem = new GameTreeItem(first, rinfo);

            treeList.push_back(titem);

            game_tree_data->addNode(current, treeList.size(), false);
        }
    }

    game_tree_list->assignTreeData(game_tree_root);
}

void GameTree::handleTreeListEntry(int node_int, IntVector *)
{
    game_shot->SetImage("");
    game_title->SetText("");
    game_system->SetText("");
    game_year->SetText("");
    game_genre->SetText("");
    game_favorite->SetText("");

    if (node_int > 0)
    {
        curitem = treeList[node_int - 1];

        if (curitem->isleaf)
        {
            QString imagename;

            if (curitem->rominfo->FindImage("screenshot", &imagename))
            {
                game_shot->SetImage(imagename);
            }
        }

        for (QStringList::Iterator field = m_pathlist.begin();
             field != m_pathlist.end(); ++field)
        {
            if (*field == "system")
                game_system->SetText(curitem->rominfo->System());
            else if (*field == "year")
            {
                int year = curitem->rominfo->Year();
                if (year == 0)
                    game_year->SetText("");
                else
                    game_year->SetText(QString::number(year));
            }
            else if (*field == "genre")
                game_genre->SetText(curitem->rominfo->Genre());
            else if (*field == "gamename")
            {
                game_title->SetText(curitem->rominfo->Gamename());
                if (curitem->rominfo->Favorite())
                    game_favorite->SetText("Yes");
                else
                    game_favorite->SetText("No");
            }
        }
    }
    else
        curitem = NULL;

    game_shot->LoadImage();
}

void GameTree::handleTreeListSelection(int node_int, IntVector *)
{
    if (node_int > 0)
    {
        curitem = treeList[node_int - 1];
    
        if (curitem->isleaf)
        {
            GameHandler::Launchgame(curitem->rominfo);
            raise();
            setActiveWindow();
        }
    }    
}

void GameTree::goRight(void)
{
    if (curitem)
    {
        if (!curitem->filled)
            FillListFrom(curitem);
    }

    game_tree_list->pushDown();
}

void GameTree::edit(void)
{
    if (!curitem)
        return;

    if (curitem->level == "system")
        GameHandler::EditSystemSettings(curitem->rominfo);
    else if (curitem->level == "gamename" && curitem->isleaf)
        GameHandler::EditSettings(curitem->rominfo);
}

void GameTree::toggleFavorite(void)
{
    if (!curitem)
        return;

    if (curitem->level == "gamename" && curitem->isleaf)
    {
        curitem->rominfo->setFavorite();
        if (curitem->rominfo->Favorite())
            game_favorite->SetText("Yes");
        else
            game_favorite->SetText("No");
    }
}

void GameTree::FillListFrom(GameTreeItem *item)
{
    QString whereClause;
    QString conjunction;
    QString column;

    for (QStringList::Iterator field = m_pathlist.begin();
         field != m_pathlist.end(); ++field)
    {
        whereClause += conjunction + getClause(*field, item);
        conjunction = " AND ";

        if (*field == item->level)
        {
            ++field;
            column = *field;
            break;
        }
    }
    
    if (showfavs == "1")
      whereClause += " AND favorite=1";


    QStringList regSystems;
    if (column == "system")
    {
        for (uint i = 0; i < GameHandler::count(); ++i)
            regSystems.append(GameHandler::getHandler(i)->Systemname());
    }

    bool isleaf = (column == "gamename");
    QString selcols = isleaf ? "gamename, system" : column;

    QString thequery = QString("SELECT DISTINCT %1 FROM gamemetadata "
                               "WHERE %2 ORDER BY %3;")
                               .arg(selcols).arg(whereClause).arg(column);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            if (column == "system" && 
                (regSystems.find(current) == regSystems.end()))
                continue;

            RomInfo* rinfo;
            if (isleaf)
            {
                //  no guarantee System has been set so create a temp RomInfo so
                //  that CreateRomInfo can create the correct subtype
                RomInfo* temp = new RomInfo();
                temp->setSystem(query.value(1).toString());
                rinfo = GameHandler::CreateRomInfo(temp);
                delete temp;

                rinfo->setGamename(query.value(0).toString());
                rinfo->setSystem(query.value(1).toString());
                rinfo->fillData();
            }
            else
            {
                rinfo = new RomInfo(*(item->rominfo));
                rinfo->setField(column, current);
            }

            GameTreeItem *titem = new GameTreeItem(column, rinfo);

            treeList.push_back(titem);

            GenericTree *node = game_tree_list->getCurrentNode();

            current = current.stripWhiteSpace();

            if (node)
                node->addNode(current, treeList.size(), isleaf);
            else
            {
                cerr << "Couldn't get active node\n";
            }
        }
    }

    item->filled = true;
}

QString GameTree::getClause(QString field, GameTreeItem *item)
{
    if (!item)
        return "";

    QString clause = field + " = \"";
    if (field == "system")
        clause += item->rominfo->System();
    else if (field == "year")
        clause += QString::number(item->rominfo->Year());
    else if (field == "genre")
        clause += item->rominfo->Genre();
    else if (field == "gamename")
        clause += item->rominfo->Gamename();
    clause += "\"";

    return clause;
}

void GameTree::wireUpTheme(void)
{
    game_tree_list = getUIManagedTreeListType("gametreelist");
    if (!game_tree_list)
    {
        cerr << "gametree.o: Couldn't find a gametreelist in your theme" << endl;
        exit(0);
    }
    game_tree_list->showWholeTree(true);
    game_tree_list->colorSelectables(true);

    connect(game_tree_list, SIGNAL(nodeSelected(int, IntVector*)), 
            this, SLOT(handleTreeListSelection(int, IntVector*)));
    connect(game_tree_list, SIGNAL(nodeEntered(int, IntVector*)), 
            this, SLOT(handleTreeListEntry(int, IntVector*)));

    game_title = getUITextType("gametitle");
    if (!game_title)
    {
        cerr << "gametree.o: Couldn't find a text area gametitle\n";
    }

    game_system = getUITextType("systemname");
    if (!game_system)
    {
        cerr << "gametree.o: Couldn't find a text area systemname\n";
    }

    game_year = getUITextType("yearname");
    if (!game_year)
    {
        cerr << "gametree.o: Couldn't find a text area yearname\n";
    }

    game_genre = getUITextType("genrename");
    if (!game_genre)
    {
        cerr << "gametree.o: Couldn't find a text area genrename\n";
    }

    game_favorite = getUITextType("showfavorite");
    if (!game_favorite)
    {
        cerr << "gametree.o: Couldn't find a text area showfavorite\n";
    }

    game_shot = getUIImageType("gameimage");
    if (!game_shot)
    {
        cerr << "gametree.o: Couldn't find an image gameimage\n"; 
    }
}

