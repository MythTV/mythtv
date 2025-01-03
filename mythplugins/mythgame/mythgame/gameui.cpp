// Qt
#include <QMetaType>
#include <QStringList>
#include <QTimer>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdirs.h>
#include <libmythmetadata/mythuimetadataresults.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythgenerictree.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibuttontree.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythuitext.h>

// MythGame headers
#include "gamehandler.h"
#include "rominfo.h"
#include "gamedetails.h"
#include "romedit.h"
#include "gamescan.h"
#include "gameui.h"

static const QString sLocation = "MythGame";

class GameTreeInfo
{
  public:
    GameTreeInfo(const QString& levels, QString  filter)
      : m_levels(levels.split(" "))
      , m_filter(std::move(filter))
    {
    }

    int getDepth() const                        { return m_levels.size(); }
    const QString& getLevel(unsigned i) const   { return m_levels[i]; }
    const QString& getFilter() const            { return m_filter; }

  private:
    QStringList m_levels;
    QString m_filter;
};

Q_DECLARE_METATYPE(GameTreeInfo *)

GameUI::GameUI(MythScreenStack *parent)
       : MythScreenType(parent, "GameUI"),
         m_query(new MetadataDownload(this)),
         m_imageDownload(new MetadataImageDownload(this))
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

bool GameUI::Create()
{
    if (!LoadWindowFromXML("game-ui.xml", "gameui", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_gameUITree, "gametreelist", &err);
    UIUtilW::Assign(this, m_gameTitleText, "title");
    UIUtilW::Assign(this, m_gameSystemText, "system");
    UIUtilW::Assign(this, m_gameYearText, "year");
    UIUtilW::Assign(this, m_gameGenreText, "genre");
    UIUtilW::Assign(this, m_gameFavouriteState, "favorite");
    UIUtilW::Assign(this, m_gamePlotText, "description");
    UIUtilW::Assign(this, m_gameImage, "screenshot");
    UIUtilW::Assign(this, m_fanartImage, "fanart");
    UIUtilW::Assign(this, m_boxImage, "coverart");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'gameui'");
        return false;
    }

    connect(m_gameUITree, &MythUIButtonTree::itemClicked,
            this, &GameUI::itemClicked);

    connect(m_gameUITree, &MythUIButtonTree::nodeChanged,
            this, &GameUI::nodeChanged);

    m_gameShowFileName = gCoreContext->GetBoolSetting("GameShowFileNames");

    BuildTree();

    BuildFocusList();

    return true;
}

void GameUI::BuildTree()
{
    if (m_gameTree)
    {
        m_gameUITree->Reset();
        delete m_gameTree;
        m_gameTree = nullptr;
    }

    m_gameTree = new MythGenericTree("game root", 0, false);

    //  create system filter to only select games where handlers are present
    QString systemFilter;

    // The call to GameHandler::count() fills the handler list for us
    // to move through.
    unsigned handlercount = GameHandler::count();

    for (unsigned i = 0; i < handlercount; ++i)
    {
        QString system = GameHandler::getHandler(i)->SystemName();
        if (i == 0)
            systemFilter = "`system` in ('" + system + "'";
        else
            systemFilter += ",'" + system + "'";
    }
    if (systemFilter.isEmpty())
    {
        systemFilter = "1=0";
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find any game handlers!"));
    }
    else
    {
        systemFilter += ")";
    }

    m_showHashed = gCoreContext->GetBoolSetting("GameTreeView");

    //  create a few top level nodes - this could be moved to a config based
    //  approach with multiple roots if/when someone has the time to create
    //  the relevant dialog screens

    QString levels = gCoreContext->GetSetting("GameFavTreeLevels", "gamename");

    auto *new_node = new MythGenericTree(tr("Favorites"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo(levels, systemFilter + " and favorite=1")));
    m_favouriteNode = m_gameTree->addNode(new_node);

    levels = gCoreContext->GetSetting("GameAllTreeLevels", "system gamename");

    if (m_showHashed)
    {
        int pos = levels.indexOf("gamename");
        if (pos >= 0)
            levels.insert(pos, " hash ");
    }

    new_node = new MythGenericTree(tr("All Games"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo(levels, systemFilter)));
    m_gameTree->addNode(new_node);

    new_node = new MythGenericTree(tr("-   By Genre"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo("genre gamename", systemFilter)));
    m_gameTree->addNode(new_node);

    new_node = new MythGenericTree(tr("-   By Year"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo("year gamename", systemFilter)));
    m_gameTree->addNode(new_node);

    new_node = new MythGenericTree(tr("-   By Name"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo("gamename", systemFilter)));
    m_gameTree->addNode(new_node);

    new_node = new MythGenericTree(tr("-   By Publisher"), 1, true);
    new_node->SetData(QVariant::fromValue(
                new GameTreeInfo("publisher gamename", systemFilter)));
    m_gameTree->addNode(new_node);

    m_gameUITree->AssignTree(m_gameTree);
    nodeChanged(m_gameUITree->GetCurrentNode());
}

bool GameUI::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Game", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
            ShowMenu();
        else if (action == "EDIT")
            edit();
        else if (action == "INFO")
            showInfo();
        else if (action == "TOGGLEFAV")
            toggleFavorite();
        else if ((action == "INCSEARCH") || (action == "INCSEARCHNEXT"))
            searchStart();
        else if (action == "DOWNLOADDATA")
            gameSearch();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void GameUI::nodeChanged(MythGenericTree* node)
{
    if (!node)
        return;

    if (!isLeaf(node))
    {
        if (node->childCount() == 0 || node == m_favouriteNode)
        {
            node->deleteAllChildren();
            fillNode(node);
        }
        clearRomInfo();
    }
    else
    {
        auto *romInfo = node->GetData().value<RomInfo *>();
        if (!romInfo)
            return;
        if (romInfo->Romname().isEmpty())
            romInfo->fillData();
        updateRomInfo(romInfo);
        if (!romInfo->Screenshot().isEmpty() || !romInfo->Fanart().isEmpty() ||
            !romInfo->Boxart().isEmpty())
            showImages();
        else
        {
            if (m_gameImage)
                m_gameImage->Reset();
            if (m_fanartImage)
                m_fanartImage->Reset();
            if (m_boxImage)
                m_boxImage->Reset();
        }
    }
}

void GameUI::itemClicked(MythUIButtonListItem* /*item*/)
{
    MythGenericTree *node = m_gameUITree->GetCurrentNode();
    if (isLeaf(node))
    {
        auto *romInfo = node->GetData().value<RomInfo *>();
        if (!romInfo)
            return;
        if (romInfo->RomCount() == 1)
        {
            GameHandler::Launchgame(romInfo, nullptr);
        }
        else
        {
            //: %1 is the game name
            QString msg = tr("Choose System for:\n%1").arg(node->GetText());
            MythScreenStack *popupStack = GetMythMainWindow()->
                                              GetStack("popup stack");
            auto *chooseSystemPopup = new MythDialogBox(
                msg, popupStack, "chooseSystemPopup");

            if (chooseSystemPopup->Create())
            {
                chooseSystemPopup->SetReturnEvent(this, "chooseSystemPopup");
                QString all_systems = romInfo->AllSystems();
                QStringList players = all_systems.split(',');
                for (const auto & player : std::as_const(players))
                    chooseSystemPopup->AddButton(player);
                popupStack->AddScreen(chooseSystemPopup);
            }
            else
            {
                delete chooseSystemPopup;
            }
        }
    }
}

void GameUI::showImages(void)
{
    if (m_gameImage)
        m_gameImage->Load();
    if (m_fanartImage)
        m_fanartImage->Load();
    if (m_boxImage)
        m_boxImage->Load();
}

void GameUI::searchComplete(const QString& string)
{
    if (!m_gameUITree->GetCurrentNode())
        return;

    MythGenericTree *parent = m_gameUITree->GetCurrentNode()->getParent();
    if (!parent)
        return;

    MythGenericTree *new_node = parent->getChildByName(string);
    if (new_node)
        m_gameUITree->SetCurrentNode(new_node);
}

void GameUI::updateRomInfo(RomInfo *rom)
{
    if (m_gameTitleText)
        m_gameTitleText->SetText(rom->Gamename());
    if (m_gameSystemText)
        m_gameSystemText->SetText(rom->System());
    if (m_gameYearText)
        m_gameYearText->SetText(rom->Year());
    if (m_gameGenreText)
        m_gameGenreText->SetText(rom->Genre());
    if (m_gamePlotText)
        m_gamePlotText->SetText(rom->Plot());

    if (m_gameFavouriteState)
    {
        if (rom->Favorite())
            m_gameFavouriteState->DisplayState("yes");
        else
            m_gameFavouriteState->DisplayState("no");
    }

    if (m_gameImage)
    {
        m_gameImage->Reset();
        m_gameImage->SetFilename(rom->Screenshot());
    }
    if (m_fanartImage)
    {
        m_fanartImage->Reset();
        m_fanartImage->SetFilename(rom->Fanart());
    }
    if (m_boxImage)
    {
        m_boxImage->Reset();
        m_boxImage->SetFilename(rom->Boxart());
    }
}

void GameUI::clearRomInfo(void)
{
    if (m_gameTitleText)
        m_gameTitleText->Reset();
    if (m_gameSystemText)
        m_gameSystemText->Reset();
    if (m_gameYearText)
        m_gameYearText->Reset();
    if (m_gameGenreText)
        m_gameGenreText->Reset();
    if (m_gamePlotText)
        m_gamePlotText->Reset();
    if (m_gameFavouriteState)
        m_gameFavouriteState->Reset();

    if (m_gameImage)
        m_gameImage->Reset();

    if (m_fanartImage)
        m_fanartImage->Reset();

    if (m_boxImage)
        m_boxImage->Reset();
}

void GameUI::edit(void)
{
    MythGenericTree *node = m_gameUITree->GetCurrentNode();
    if (isLeaf(node))
    {
        auto *romInfo = node->GetData().value<RomInfo *>();

        MythScreenStack *screenStack = GetScreenStack();

        auto *md_editor = new EditRomInfoDialog(screenStack,
            "mythgameeditmetadata", romInfo);

        if (md_editor->Create())
        {
            screenStack->AddScreen(md_editor);
            md_editor->SetReturnEvent(this, "editMetadata");
        }
        else
        {
            delete md_editor;
        }
    }
}

void GameUI::showInfo()
{
    MythGenericTree *node = m_gameUITree->GetCurrentNode();
    if (isLeaf(node))
    {
        auto *romInfo = node->GetData().value<RomInfo *>();
        if (!romInfo)
            return;
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *details_dialog  = new GameDetailsPopup(mainStack, romInfo);

        if (details_dialog->Create())
        {
            mainStack->AddScreen(details_dialog);
            details_dialog->SetReturnEvent(this, "detailsPopup");
        }
        else
        {
            delete details_dialog;
        }
    }
}

void GameUI::ShowMenu()
{
    MythGenericTree *node = m_gameUITree->GetCurrentNode();

    MythScreenStack *popupStack = GetMythMainWindow()->
                                          GetStack("popup stack");
    auto *showMenuPopup =
            new MythDialogBox(node->GetText(), popupStack, "showMenuPopup");

    if (showMenuPopup->Create())
    {
        showMenuPopup->SetReturnEvent(this, "showMenuPopup");

        showMenuPopup->AddButton(tr("Scan For Changes"));
        if (isLeaf(node))
        {
            auto *romInfo = node->GetData().value<RomInfo *>();
            if (romInfo)
            {
                showMenuPopup->AddButton(tr("Show Information"));
                if (romInfo->Favorite())
                    showMenuPopup->AddButton(tr("Remove Favorite"));
                else
                    showMenuPopup->AddButton(tr("Make Favorite"));
                showMenuPopup->AddButton(tr("Retrieve Details"));
                showMenuPopup->AddButton(tr("Edit Details"));
            }
        }
        popupStack->AddScreen(showMenuPopup);
    }
    else
    {
        delete showMenuPopup;
    }
}

void GameUI::searchStart(void)
{
    MythGenericTree *parent = m_gameUITree->GetCurrentNode()->getParent();

    if (parent != nullptr)
    {
        QStringList childList;
        QList<MythGenericTree*>::iterator it;
        QList<MythGenericTree*> *children = parent->getAllChildren();

        for (it = children->begin(); it != children->end(); ++it)
        {
            MythGenericTree *child = *it;
            childList << child->GetText();
        }

        MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");
        auto *searchDialog = new MythUISearchDialog(popupStack,
            tr("Game Search"), childList, true, "");

        if (searchDialog->Create())
        {
            connect(searchDialog, &MythUISearchDialog::haveResult,
                    this, &GameUI::searchComplete);

            popupStack->AddScreen(searchDialog);
        }
        else
        {
            delete searchDialog;
        }
    }
}

void GameUI::toggleFavorite(void)
{
    MythGenericTree *node = m_gameUITree->GetCurrentNode();
    if (isLeaf(node))
    {
        auto *romInfo = node->GetData().value<RomInfo *>();
        romInfo->setFavorite(true);
        updateChangedNode(node, romInfo);
    }
}

void GameUI::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (dce == nullptr)
            return;
        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "showMenuPopup")
        {
            if (resulttext == tr("Edit Details"))
            {
                edit();
            }
            if (resulttext == tr("Scan For Changes"))
            {
                doScan();
            }
            else if (resulttext == tr("Show Information"))
            {
                showInfo();
            }
            else if (resulttext == tr("Make Favorite") ||
                     resulttext == tr("Remove Favorite"))
            {
                toggleFavorite();
            }
            else if (resulttext == tr("Retrieve Details"))
            {
                gameSearch();
            }
        }
        else if (resultid == "chooseSystemPopup")
        {
            if (!resulttext.isEmpty() && resulttext != tr("Cancel"))
            {
                MythGenericTree *node = m_gameUITree->GetCurrentNode();
                auto *romInfo = node->GetData().value<RomInfo *>();
                GameHandler::Launchgame(romInfo, resulttext);
            }
        }
        else if (resultid == "editMetadata")
        {
            MythGenericTree *node = m_gameUITree->GetCurrentNode();
            auto *oldRomInfo = node->GetData().value<RomInfo *>();
            delete oldRomInfo;

            auto *romInfo = dce->GetData().value<RomInfo *>();
            node->SetData(QVariant::fromValue(romInfo));
            node->SetText(romInfo->Gamename());

            romInfo->SaveToDatabase();
            updateChangedNode(node, romInfo);
        }
        else if (resultid == "detailsPopup")
        {
            // Play button pushed
            itemClicked(nullptr);
        }
    }
    if (event->type() == MetadataLookupEvent::kEventType)
    {
        auto *lue = dynamic_cast<MetadataLookupEvent *>(event);
        if (lue == nullptr)
            return;
        MetadataLookupList lul = lue->m_lookupList;

        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        if (lul.isEmpty())
            return;

        if (lul.count() == 1)
        {
            OnGameSearchDone(lul[0]);
        }
        else
        {
            auto *resultsdialog =
                  new MetadataResultsDialog(m_popupStack, lul);

            connect(resultsdialog, &MetadataResultsDialog::haveResult,
                    this, &GameUI::OnGameSearchListSelection,
                    Qt::QueuedConnection);

            if (resultsdialog->Create())
                m_popupStack->AddScreen(resultsdialog);
        }
    }
    else if (event->type() == MetadataLookupFailure::kEventType)
    {
        auto *luf = dynamic_cast<MetadataLookupFailure *>(event);
        if (luf == nullptr)
            return;
        MetadataLookupList lul = luf->m_lookupList;

        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        if (!lul.empty())
        {
            MetadataLookup *lookup = lul[0];
            auto *node = lookup->GetData().value<MythGenericTree *>();
            if (node)
            {
                auto *metadata = node->GetData().value<RomInfo *>();
                if (metadata)
                {
                }
            }
            LOG(VB_GENERAL, LOG_ERR,
                QString("No results found for %1").arg(lookup->GetTitle()));
        }
    }
    else if (event->type() == ImageDLEvent::kEventType)
    {
        auto *ide = dynamic_cast<ImageDLEvent *>(event);
        if (ide == nullptr)
            return;
        MetadataLookup *lookup = ide->m_item;

        if (!lookup)
            return;

        handleDownloadedImages(lookup);
    }
    else if (event->type() == ImageDLFailureEvent::kEventType)
    {
        MythErrorNotification n(tr("Failed to retrieve image(s)"),
                                sLocation,
                                tr("Check logs"));
        GetNotificationCenter()->Queue(n);
    }
}

QString GameUI::getFillSql(MythGenericTree *node) const
{
    QString layer = node->GetText();
    int childDepth = node->getInt() + 1;
    QString childLevel = getChildLevelString(node);
    QString filter = getFilter(node);
    bool childIsLeaf = childDepth == getLevelsOnThisBranch(node) + 1;
    auto *romInfo = node->GetData().value<RomInfo *>();

    if (childLevel.isEmpty())
        childLevel = "gamename";

    QString columns;
    QString conj = "where ";

    if (!filter.isEmpty())
    {
        filter = conj + filter;
        conj = " and ";
    }
    if ((childLevel == "gamename") && (m_gameShowFileName))
    {
        columns = childIsLeaf
                    ? "romname,`system`,year,genre,gamename"
                    : "romname";

        if (m_showHashed)
            filter += " and romname like '" + layer + "%'";

    }
    else if ((childLevel == "gamename") && (layer.length() == 1))
    {
        columns = childIsLeaf
                    ? childLevel + ",`system`,year,genre,gamename"
                    : childLevel;

        if (m_showHashed)
            filter += " and gamename like '" + layer + "%'";

    }
    else if (childLevel == "hash")
    {
        columns = "left(gamename,1)";
    }
    else
    {

        columns = childIsLeaf
                    ? childLevel + ",`system`,year,genre,gamename"
                    : childLevel;
    }

    //  this whole section ought to be in rominfo.cpp really, but I've put it
    //  in here for now to minimise the number of files changed by this mod
    if (romInfo)
    {
        if (!romInfo->System().isEmpty())
        {
            filter += conj + "trim(system)=:SYSTEM";
            conj = " and ";
        }
        if (!romInfo->Year().isEmpty())
        {
            filter += conj + "year=:YEAR";
            conj = " and ";
        }
        if (!romInfo->Genre().isEmpty())
        {
            filter += conj + "trim(genre)=:GENRE";
            conj = " and ";
        }
        if (!romInfo->Plot().isEmpty())
        {
            filter += conj + "plot=:PLOT";
            conj = " and ";
        }
        if (!romInfo->Publisher().isEmpty())
        {
            filter += conj + "publisher=:PUBLISHER";
            conj = " and ";
        }
        if (!romInfo->Gamename().isEmpty())
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
    else if (childLevel == "hash")
    {
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

QString GameUI::getChildLevelString(MythGenericTree *node)
{
    unsigned this_level = node->getInt();
    while (node->getInt() != 1)
        node = node->getParent();

    auto *gi = node->GetData().value<GameTreeInfo *>();
    return gi ? gi->getLevel(this_level - 1) : "<invalid>";
}

QString GameUI::getFilter(MythGenericTree *node)
{
    while (node->getInt() != 1)
        node = node->getParent();
    auto *gi = node->GetData().value<GameTreeInfo *>();
    return gi ? gi->getFilter() : "<invalid>";
}

int GameUI::getLevelsOnThisBranch(MythGenericTree *node)
{
    while (node->getInt() != 1)
        node = node->getParent();

    auto *gi = node->GetData().value<GameTreeInfo *>();
    return gi ? gi->getDepth() : 0;
}

bool GameUI::isLeaf(MythGenericTree *node)
{
  return (node->getInt() - 1) == getLevelsOnThisBranch(node);
}

void GameUI::fillNode(MythGenericTree *node)
{
//  QString layername = node->GetText();
    auto *romInfo = node->GetData().value<RomInfo *>();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(getFillSql(node));

    if (romInfo)
    {
        if (!romInfo->System().isEmpty())
            query.bindValue(":SYSTEM",  romInfo->System());
        if (!romInfo->Year().isEmpty())
            query.bindValue(":YEAR", romInfo->Year());
        if (!romInfo->Genre().isEmpty())
            query.bindValue(":GENRE", romInfo->Genre());
        if (!romInfo->Plot().isEmpty())
            query.bindValue(":PLOT", romInfo->Plot());
        if (!romInfo->Publisher().isEmpty())
            query.bindValue(":PUBLISHER", romInfo->Publisher());
        if (!romInfo->Gamename().isEmpty())
            query.bindValue(":GAMENAME", romInfo->Gamename());
    }

    bool IsLeaf = node->getInt() == getLevelsOnThisBranch(node);
    if (query.exec() && query.size() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString().trimmed();
            auto *new_node =
                new MythGenericTree(current, node->getInt() + 1, false);
            if (IsLeaf)
            {
                auto *temp = new RomInfo();
                temp->setSystem(query.value(1).toString().trimmed());
                temp->setYear(query.value(2).toString());
                temp->setGenre(query.value(3).toString().trimmed());
                temp->setGamename(query.value(4).toString().trimmed());
                new_node->SetData(QVariant::fromValue(temp));
                node->addNode(new_node);
            }
            else
            {
                RomInfo *newRomInfo = nullptr;
                if (node->getInt() > 1)
                {
                    auto *currentRomInfo = node->GetData().value<RomInfo *>();
                    newRomInfo = new RomInfo(*currentRomInfo);
                }
                else
                {
                    newRomInfo = new RomInfo();
                }
                new_node->SetData(QVariant::fromValue(newRomInfo));
                node->addNode(new_node);
                if (getChildLevelString(node) != "hash")
                    newRomInfo->setField(getChildLevelString(node), current);
            }
        }
    }
}

void GameUI::resetOtherTrees(MythGenericTree *node)
{
    MythGenericTree *top_level = node;
    while (top_level->getParent() != m_gameTree)
    {
        top_level = top_level->getParent();
    }

    QList<MythGenericTree*>::iterator it;
    QList<MythGenericTree*> *children = m_gameTree->getAllChildren();

    for (it = children->begin(); it != children->end(); ++it)
    {
        MythGenericTree *child = *it;
        if (child != top_level)
        {
            child->deleteAllChildren();
        }
    }
}

void GameUI::updateChangedNode(MythGenericTree *node, RomInfo *romInfo)
{
    resetOtherTrees(node);

    if (node->getParent() == m_favouriteNode && !romInfo->Favorite())
    {
        // node is being removed
        m_gameUITree->SetCurrentNode(m_favouriteNode);
    }
    else
    {
        nodeChanged(node);
    }
}

void GameUI::gameSearch(MythGenericTree *node,
                              bool automode)
{
    if (!node)
        node = m_gameUITree->GetCurrentNode();

    if (!node)
        return;

    auto *metadata = node->GetData().value<RomInfo *>();
    if (!metadata)
        return;

    auto *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataGame);
    lookup->SetData(QVariant::fromValue(node));

    if (automode)
    {
        lookup->SetAutomatic(true);
    }

    lookup->SetTitle(metadata->Gamename());
    lookup->SetInetref(metadata->Inetref());
    if (m_query->isRunning())
        m_query->prependLookup(lookup);
    else
        m_query->addLookup(lookup);

    if (!automode)
    {
        //: %1 is the game name
        QString msg = tr("Fetching details for %1")
                           .arg(metadata->Gamename());
        createBusyDialog(msg);
    }
}

void GameUI::createBusyDialog(const QString& title)
{
    if (m_busyPopup)
        return;

    const QString& message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythgamebusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

void GameUI::OnGameSearchListSelection(RefCountHandler<MetadataLookup> lookup)
{
    if (!lookup)
        return;

    lookup->SetStep(kLookupData);
    lookup->IncrRef();
    m_query->prependLookup(lookup);
}

void GameUI::OnGameSearchDone(MetadataLookup *lookup)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = nullptr;
    }

    if (!lookup)
       return;

    auto *node = lookup->GetData().value<MythGenericTree *>();
    if (!node)
        return;

    auto *metadata = node->GetData().value<RomInfo *>();
    if (!metadata)
        return;

    metadata->setGamename(lookup->GetTitle());
    metadata->setYear(QString::number(lookup->GetYear()));
    metadata->setPlot(lookup->GetDescription());
    metadata->setSystem(lookup->GetSystem());

    QStringList coverart;
    QStringList fanart;
    QStringList screenshot;

    // Imagery
    ArtworkList coverartlist = lookup->GetArtwork(kArtworkCoverart);
    for (const auto & art : std::as_const(coverartlist))
        coverart.prepend(art.url);
    ArtworkList fanartlist = lookup->GetArtwork(kArtworkFanart);
    for (const auto & art : std::as_const(fanartlist))
        fanart.prepend(art.url);
    ArtworkList screenshotlist = lookup->GetArtwork(kArtworkScreenshot);
    for (const auto & art : std::as_const(screenshotlist))
        screenshot.prepend(art.url);

    StartGameImageSet(node, coverart, fanart, screenshot);

    metadata->SaveToDatabase();
    updateChangedNode(node, metadata);
}

void GameUI::StartGameImageSet(MythGenericTree *node, QStringList coverart,
                                     QStringList fanart, QStringList screenshot)
{
    if (!node)
        return;

    auto *metadata = node->GetData().value<RomInfo *>();
    if (!metadata)
        return;

    DownloadMap map;

    if (metadata->Boxart().isEmpty() && !coverart.empty())
    {
        ArtworkInfo info;
        info.url = coverart.takeAt(0).trimmed();
        map.insert(kArtworkCoverart, info);
    }

    if (metadata->Fanart().isEmpty() && !fanart.empty())
    {
        ArtworkInfo info;
        info.url = fanart.takeAt(0).trimmed();
        map.insert(kArtworkFanart, info);
    }

    if (metadata->Screenshot().isEmpty() && !screenshot.empty())
    {
        ArtworkInfo info;
        info.url = screenshot.takeAt(0).trimmed();
        map.insert(kArtworkScreenshot, info);
    }

    auto *lookup = new MetadataLookup();
    lookup->SetTitle(metadata->Gamename());
    lookup->SetSystem(metadata->System());
    lookup->SetInetref(metadata->Inetref());
    lookup->SetType(kMetadataGame);
    lookup->SetDownloads(map);
    lookup->SetData(QVariant::fromValue(node));

    m_imageDownload->addDownloads(lookup);
}

void GameUI::handleDownloadedImages(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    auto *node = lookup->GetData().value<MythGenericTree *>();
    if (!node)
        return;

    auto *metadata = node->GetData().value<RomInfo *>();
    if (!metadata)
        return;

    DownloadMap downloads = lookup->GetDownloads();

    if (downloads.isEmpty())
        return;

    for (DownloadMap::iterator i = downloads.begin();
            i != downloads.end(); ++i)
    {
        VideoArtworkType type = i.key();
        const ArtworkInfo& info = i.value();
        QString filename = info.url;

        if (type == kArtworkCoverart)
            metadata->setBoxart(filename);
        else if (type == kArtworkFanart)
            metadata->setFanart(filename);
        else if (type == kArtworkScreenshot)
            metadata->setScreenshot(filename);
    }

    metadata->SaveToDatabase();
    updateChangedNode(node, metadata);
}

void GameUI::doScan()
{
    if (!m_scanner)
        m_scanner = new GameScanner();
    connect(m_scanner, &GameScanner::finished, this, &GameUI::reloadAllData);
    m_scanner->doScanAll();
}

void GameUI::reloadAllData(bool dbChanged)
{
    delete m_scanner;
    m_scanner = nullptr;

    if (dbChanged)
        BuildTree();
}

