// C++
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// qt
#include <QDomDocument>
#include <QKeyEvent>
#include <QThread>

// MythTV
#include <libmyth/audio/audiooutput.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythdownloadmanager.h>
#include <libmythbase/mythversion.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>
#include <libmythui/mythuiutils.h>

// mythmusic
#include "lyricsview.h"
#include "musiccommon.h"
#include "musicplayer.h"

///////////////////////////////////////////////////////////////////////////////
// LyricsView

LyricsView::LyricsView(MythScreenStack *parent, MythScreenType *parentScreen)
    : MusicCommon(parent, parentScreen, "lyricsview")
{
    m_currentView = MV_LYRICS;

    gCoreContext->addListener(this);
}

LyricsView::~LyricsView()
{
    if (m_lyricData)
    {
        m_lyricData->disconnect();
        m_lyricData = nullptr;
    }

    gCoreContext->removeListener(this);
}

bool LyricsView::Create(void)
{
    QString windowName = gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO ? "streamlyricsview" : "trackslyricsview";

    // Load the theme for this screen
    bool err = LoadWindowFromXML("music-ui.xml", windowName, this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    // find widgets specific to this view
    UIUtilE::Assign(this, m_lyricsList,      "lyrics_list", &err);
    UIUtilW::Assign(this, m_statusText,      "status_text");
    UIUtilW::Assign(this, m_loadingState,    "loading_state");

    // for streams
    UIUtilW::Assign(this, m_bufferStatus,   "bufferstatus", &err);
    UIUtilW::Assign(this, m_bufferProgress, "bufferprogress", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot load screen '%1'").arg(windowName));
        return false;
    }

    connect(m_lyricsList, &MythUIButtonList::itemClicked, this, &LyricsView::setLyricTime);

    BuildFocusList();

    findLyrics();

    return true;
}

void LyricsView::customEvent(QEvent *event)
{
    bool handled = false;

    if ((event->type() == MusicPlayerEvent::kTrackChangeEvent) ||
        (event->type() == MusicPlayerEvent::kPlayedTracksChangedEvent))
    {
        findLyrics();
    }
    else if (event->type() == OutputEvent::kInfo)
    {
        if (m_autoScroll)
        {
            auto *oe = dynamic_cast<OutputEvent *>(event);
            MusicMetadata *curMeta = gPlayer->getCurrentMetadata();

            if (!oe || !curMeta)
                return;

            std::chrono::milliseconds rs = 0ms;

            if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
            {
                rs = gPlayer->getCurrentTrackTime();
            }
            else
            {
                rs = oe->elapsedSeconds();
            }

            int pos = 0;
            for (int x = 0; x < m_lyricsList->GetCount(); x++)
            {
                MythUIButtonListItem * item = m_lyricsList->GetItemAt(x);
                auto *lyric = item->GetData().value<LyricsLine*>();
                if (lyric)
                {
                    if (lyric->m_time > 1s && rs >= lyric->m_time)
                        pos = x;
                }
            }

            m_lyricsList->SetItemCurrent(pos);
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        if (resultid == "actionmenu")
        {
            if (resulttext == tr("Save Lyrics"))
            {
                saveLyrics();
            }
            else if ((resulttext == tr("Edit Lyrics")) ||
                     (resulttext == tr("Add Lyrics")))
            {
                editLyrics();
            }
            else if (resulttext == tr("Auto Scroll Lyrics"))
            {
                m_autoScroll = true;
            }

            handled = true;
        }
        else if (resultid == "findlyricsmenu")
        {
            if (dce->GetResult() >= 0)
            {
                QString grabber = dce->GetData().toString();
                m_lyricData->clear();
                findLyrics(grabber);
            }

            handled = true;
        }
    }
    else if (event->type() == DecoderHandlerEvent::kOperationStart)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;
        if (dhe->getMessage() && m_bufferStatus)
        {
            m_bufferStatus->SetText(*dhe->getMessage());
        }
    }
    else if (event->type() == DecoderHandlerEvent::kBufferStatus)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        int available = 0;
        int maxSize = 0;
        dhe->getBufferStatus(&available, &maxSize);

        if (m_bufferStatus)
        {
            QString status = QString("%1%").arg((int)(100.0 / ((double)maxSize / (double)available)));
            m_bufferStatus->SetText(status);
        }

        if (m_bufferProgress)
        {
            m_bufferProgress->SetTotal(maxSize);
            m_bufferProgress->SetUsed(available);
        }
    }
    else if (event->type() == DecoderHandlerEvent::kOperationStop)
    {
        if (m_bufferStatus)
            m_bufferStatus->Reset();
    }

    if (!handled)
        MusicCommon::customEvent(event);
}

void LyricsView::ShowMenu(void)
{
    QString label = tr("Actions");

    auto *menu = new MythMenu(label, this, "actionmenu");

    if (m_lyricData)
    {
        menu->AddItem(tr("Find Lyrics"), nullptr, createFindLyricsMenu());

        if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
        {
            if (!m_lyricData->lyrics()->isEmpty())
                menu->AddItem(tr("Edit Lyrics"), nullptr, nullptr);
            else
                menu->AddItem(tr("Add Lyrics"), nullptr, nullptr);

            if (!m_lyricData->lyrics()->isEmpty() && m_lyricData->changed())
                menu->AddItem(tr("Save Lyrics"), nullptr, nullptr);
        }

        if (!m_autoScroll)
            menu->AddItem(tr("Auto Scroll Lyrics"), nullptr, nullptr);
    }

    menu->AddItem(tr("Other Options"), nullptr, createMainMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

MythMenu* LyricsView::createFindLyricsMenu(void)
{
    QString label = tr("Find Lyrics");

    auto *menu = new MythMenu(label, this, "findlyricsmenu");
    menu->AddItemV(tr("Search All Grabbers"), QVariant::fromValue(QString("ALL")));

    QStringList strList("MUSIC_LYRICS_GETGRABBERS");
    if (gCoreContext->SendReceiveStringList(strList))
    {
        for (int x = 1; x < strList.count(); x++)
            menu->AddItemV(tr("Search %1").arg(strList.at(x)), QVariant::fromValue(strList.at(x)));
    }

    return menu;
}

bool LyricsView::keyPressEvent(QKeyEvent *event)
{
    bool handled = false;
    QStringList actions;

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
    {
        // if the lyrics list widget has focus and the UP/DOWN or PAGEUP/PAGEDOWN keys
        // are pressed turn off auto scroll
        if (GetFocusWidget() == m_lyricsList)
        {
            GetMythMainWindow()->TranslateKeyPress("Global", event, actions);
            if (actions.contains("UP") || actions.contains("DOWN") ||
                actions.contains("PAGEUP") || actions.contains("PAGEDOWN"))
                m_autoScroll = false;
        }

        return true;
    }

    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "EDIT")
        {
            if (GetFocusWidget() == m_lyricsList)
            {
                if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
                    editLyrics();
            }
            else
            {
                // pass the EDIT action on for normal tracks if not on the lyrics list
                if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
                    handled = false;
            }
        }
        else if (GetFocusWidget() == m_lyricsList && (action == "MARK" || action == "SELECT"))
        {
            setLyricTime();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    return handled;
}

void LyricsView::setLyricTime(void)
{
    // change the time of the next lyric to the playback time
    if (gPlayer->isPlaying() && m_lyricsList->GetCurrentPos() < m_lyricsList->GetCount())
    {
        m_lyricsList->SetItemCurrent(m_lyricsList->GetCurrentPos() + 1);

        MythUIButtonListItem *item = m_lyricsList->GetItemCurrent();
        if (item)
        {
            auto *lyric = item->GetData().value<LyricsLine*>();
            if (lyric)
            {
                lyric->m_time = gPlayer->getOutput()->GetAudiotime() - 750ms;
                m_lyricData->setChanged(true);
                m_lyricData->setSyncronized(true);
                m_autoScroll = false;
            }
        }
    }
}

void LyricsView::findLyrics(const QString &grabber)
{
    if (m_lyricData)
    {
        if (m_lyricData->changed())
            m_lyricData->save();

        m_lyricData->disconnect();
        m_lyricData = nullptr;
    }

    MusicMetadata *mdata = nullptr;

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
    {
        if (!gPlayer->getPlayedTracksList().isEmpty())
            mdata = gPlayer->getPlayedTracksList().last();
    }
    else
    {
        mdata = gPlayer->getCurrentMetadata();
    }

    if (!mdata)
        return;

    m_lyricsList->Reset();

    if (m_loadingState)
        m_loadingState->DisplayState("on");

    m_lyricData = mdata->getLyricsData();

    connect(m_lyricData, &LyricsData::statusChanged, 
            this,  &LyricsView::lyricStatusChanged);

    m_lyricData->findLyrics(grabber);
}

void LyricsView::lyricStatusChanged(LyricsData::Status status, const QString &message)
{
    switch (status)
    {
        case LyricsData::STATUS_SEARCHING:
        {
            showMessage(message);
            break;
        }

        case LyricsData::STATUS_NOTFOUND:
        {
            if (m_loadingState)
                m_loadingState->DisplayState("off");

            showMessage(message);
            break;
        }

        case LyricsData::STATUS_FOUND:
        {
            showLyrics();
            break;
        }

        default:
            if (m_loadingState)
                m_loadingState->DisplayState("off");

            showMessage("");
            break;
    }
}

void LyricsView::showMessage(const QString &message)
{
    if (m_statusText)
    {
        if (message.isEmpty())
        {
            m_statusText->Reset();
            m_statusText->Hide();
        }
        else
        {
            m_statusText->SetText(message);
            m_statusText->Show();
        }
    }
}

void LyricsView::showLyrics(void)
{
    if (m_loadingState)
        m_loadingState->DisplayState("off");

    showMessage("");

    m_lyricsList->Reset();

    QString syncronized = m_lyricData->syncronized() ? tr("Syncronized") : tr("Unsyncronized");
    new MythUIButtonListItem(m_lyricsList, tr("** Lyrics from %1 (%2) **")
                             .arg(m_lyricData->grabber(), syncronized));

    for (auto * line : std::as_const(*m_lyricData->lyrics()))
    {
        if (line)
            new MythUIButtonListItem(m_lyricsList, line->m_lyric, QVariant::fromValue(line));
    }

    m_autoScroll = true;
}

void LyricsView::saveLyrics(void)
{
    if (m_lyricData)
        m_lyricData->save();
}

void LyricsView::editLyrics(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditLyricsDialog(mainStack, m_lyricData);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }
    connect(editDialog, &EditLyricsDialog::haveResult, this, &LyricsView::editFinished);
    mainStack->AddScreen(editDialog);
}

void LyricsView::editFinished(bool result)
{
    if (result)
        showLyrics();
}

///////////////////////////////////////////////////////////////////////////////
// EditLyricsDialog

EditLyricsDialog::EditLyricsDialog(
    MythScreenStack *parent, LyricsData *sourceData) :
    MythScreenType(parent, "EditLyricsDialog"),
    m_sourceData(sourceData)
{
}

bool EditLyricsDialog::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("music-ui.xml", "editlyrics", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_grabberEdit, "grabberedit", &err);
    UIUtilE::Assign(this, m_syncronizedCheck, "syncronizedcheck", &err);
    UIUtilE::Assign(this, m_titleEdit, "titleedit", &err);
    UIUtilE::Assign(this, m_artistEdit, "artistedit", &err);
    UIUtilE::Assign(this, m_albumEdit, "albumedit", &err);
    UIUtilE::Assign(this, m_lyricsEdit, "lyricsedit", &err);
    UIUtilE::Assign(this, m_okButton, "okbutton", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancelbutton", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editlyrics'");
        return false;
    }

    connect(m_okButton, &MythUIButton::Clicked, this, &EditLyricsDialog::okPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &EditLyricsDialog::cancelPressed);
    connect(m_syncronizedCheck, &MythUICheckBox::toggled, this, &EditLyricsDialog::syncronizedChanged);

    m_grabberEdit->SetText(m_sourceData->grabber());
    m_syncronizedCheck->SetCheckState(m_sourceData->syncronized());
    m_titleEdit->SetText(m_sourceData->title());
    m_artistEdit->SetText(m_sourceData->artist());
    m_albumEdit->SetText(m_sourceData->album());

    loadLyrics();

    BuildFocusList();

    return true;
}

bool EditLyricsDialog::keyPressEvent(QKeyEvent *event)
{
    bool handled = false;
    QStringList actions;

    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];

        if (action == "ESCAPE" && somethingChanged())
        {
            cancelPressed();
            return true;
        }
    }

    return MythScreenType::keyPressEvent(event);
}

void EditLyricsDialog::loadLyrics(void)
{
    QString lyrics;
    LyricsLineMap::iterator i = m_sourceData->lyrics()->begin();
    while (i != m_sourceData->lyrics()->end())
    {
        LyricsLine *line = (*i);
        ++i;

        lyrics += line->toString(m_syncronizedCheck->GetBooleanCheckState());

        if (i != m_sourceData->lyrics()->end())
            lyrics += '\n';
    }

    m_lyricsEdit->SetText(lyrics);
}

void EditLyricsDialog::syncronizedChanged([[maybe_unused]] bool syncronized)
{
    loadLyrics();
}

bool EditLyricsDialog::somethingChanged(void)
{
    bool changed = false;

    changed |= m_sourceData->artist() != m_artistEdit->GetText();
    changed |= m_sourceData->album() != m_albumEdit->GetText();
    changed |= m_sourceData->title() != m_titleEdit->GetText();
    changed |= m_sourceData->grabber() != m_grabberEdit->GetText();
    changed |= m_sourceData->syncronized() != m_syncronizedCheck->GetBooleanCheckState();

    QStringList lines = m_lyricsEdit->GetText().split('\n');

    if (lines.count() != m_sourceData->lyrics()->count())
        return true;

    int x = 0;
    for (auto * line : std::as_const(*m_sourceData->lyrics()))
    {
        if (line->toString(m_sourceData->syncronized()) != lines.at(x))
            changed = true;
        ++x;
    }

    return changed;
}

void EditLyricsDialog::okPressed(void)
{
    if (!somethingChanged())
    {
        emit haveResult(false);
        Close();
        return;
    }

    QStringList lyrics = m_lyricsEdit->GetText().split('\n');

    m_sourceData->setChanged(true);
    m_sourceData->setArtist(m_artistEdit->GetText());
    m_sourceData->setAlbum(m_albumEdit->GetText());
    m_sourceData->setTitle(m_titleEdit->GetText());
    m_sourceData->setGrabber(m_grabberEdit->GetText());
    m_sourceData->setSyncronized(m_syncronizedCheck->GetBooleanCheckState());
    m_sourceData->setLyrics(lyrics);
    m_sourceData->save();
    m_sourceData->setChanged(false);

    emit haveResult(true);
    Close();
}

void EditLyricsDialog::saveEdits(bool ok)
{
    if (!ok)
    {
        emit haveResult(false);
        Close();
        return;
    }

    okPressed();
}

void EditLyricsDialog::cancelPressed(void )
{
    if (somethingChanged())
    {
        ShowOkPopup(tr("Save changes?"), this, &EditLyricsDialog::saveEdits, true);
        return;
    }

    emit haveResult(false);
    Close();
}
