#include <QFile>
#include <QApplication>

#include "mythcontext.h"
#include "mythuihelper.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibutton.h"
#include "mythdialogbox.h"

#include "gamedetails.h"

GameDetailsPopup::GameDetailsPopup(MythScreenStack *parent,
                                   const RomInfo *romInfo)
           : MythScreenType (parent, "gamedetailspopup"),
        m_romInfo(romInfo), m_id(""), m_retObject(NULL),
        m_gameName(NULL), m_gameType(NULL), m_romName(NULL),
        m_crc(NULL), m_romPath(NULL), m_genre(NULL),
        m_year(NULL), m_country(NULL), m_plot(NULL),
        m_publisher(NULL), m_allSystems(NULL), m_fanartImage(NULL),
        m_boxImage(NULL), m_playButton(NULL), m_doneButton(NULL)
{
    m_romInfo = romInfo;
}

GameDetailsPopup::~GameDetailsPopup(void)
{
}

void GameDetailsPopup::handleText(const QString &name, const QString &value)
{
    MythUIText *textarea = NULL;
    UIUtilE::Assign(this, textarea, name);
    if (textarea)
    {
        textarea->SetText(value);
    }
}

void GameDetailsPopup::handleImage(const QString &name, const QString &filename)
{
    MythUIImage *image = NULL;
    UIUtilW::Assign(this, image, name);
    if (image)
    {
        if (filename.size())
        {
            image->SetFilename(filename);
            image->Load();
        }
        else
            image->Reset();
    }
}

bool GameDetailsPopup::Create(void)
{
    if (!LoadWindowFromXML("game-ui.xml", "gamedetailspopup", this))
        return false;

    UIUtilW::Assign(this, m_playButton, "play_button");
    UIUtilW::Assign(this, m_doneButton, "done_button");

    if (m_playButton)
        connect(m_playButton, SIGNAL(Clicked()), SLOT(Play()));

    if (m_doneButton)
        connect(m_doneButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    if (m_playButton || m_doneButton)
        SetFocusWidget(m_playButton ? m_playButton : m_doneButton);

    handleText("title", m_romInfo->Gamename());
    handleText("gametype", m_romInfo->GameType());
    handleText("romname", m_romInfo->Romname());
    handleText("crc", m_romInfo->CRC_VALUE());
    handleText("rompath", m_romInfo->Rompath());
    handleText("genre", m_romInfo->Genre());
    handleText("year", m_romInfo->Year());
    handleText("country", m_romInfo->Country());
    handleText("publisher", m_romInfo->Publisher());
    handleText("description", m_romInfo->Plot());
    handleText("allsystems", m_romInfo->AllSystems());
    handleImage("fanart", m_romInfo->Fanart());
    handleImage("coverart", m_romInfo->Boxart());
    handleImage("screenshot", m_romInfo->Screenshot());

    return true;
}

void GameDetailsPopup::Play()
{
    if (m_retObject)
    {
        DialogCompletionEvent *dce =
            new DialogCompletionEvent(m_id, 0, "", "");
        QApplication::postEvent(m_retObject, dce);
        Close();
    }
}

void GameDetailsPopup::SetReturnEvent(QObject *retobject,
                                      const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

