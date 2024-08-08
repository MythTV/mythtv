// Qt
#include <QApplication>
#include <QFile>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// MythGame
#include "gamedetails.h"

void GameDetailsPopup::handleText(const QString &name, const QString &value)
{
    MythUIText *textarea = nullptr;
    UIUtilE::Assign(this, textarea, name);
    if (textarea)
    {
        textarea->SetText(value);
    }
}

void GameDetailsPopup::handleImage(const QString &name, const QString &filename)
{
    MythUIImage *image = nullptr;
    UIUtilW::Assign(this, image, name);
    if (image)
    {
        if (!filename.isEmpty())
        {
            image->SetFilename(filename);
            image->Load();
        }
        else
        {
            image->Reset();
        }
    }
}

bool GameDetailsPopup::Create(void)
{
    if (!LoadWindowFromXML("game-ui.xml", "gamedetailspopup", this))
        return false;

    UIUtilW::Assign(this, m_playButton, "play_button");
    UIUtilW::Assign(this, m_doneButton, "done_button");

    if (m_playButton)
        connect(m_playButton, &MythUIButton::Clicked, this, &GameDetailsPopup::Play);

    if (m_doneButton)
        connect(m_doneButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

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
        auto *dce = new DialogCompletionEvent(m_id, 0, "", "");
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

