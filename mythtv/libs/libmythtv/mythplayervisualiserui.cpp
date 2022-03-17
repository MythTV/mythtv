// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythui/mythmainwindow.h"

#include "audioplayer.h"
#include "mythplayervisualiserui.h"
#include "tv_play.h"

#define LOC QString("PlayerVis: ")

MythPlayerVisualiserUI::MythPlayerVisualiserUI(MythMainWindow *MainWindow, TV *Tv,
                                               PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerVideoUI(MainWindow, Tv, Context, Flags)
{   
    // Register our state type for signalling
    qRegisterMetaType<MythVisualiserState>();

    // Connect signals and slots
    connect(&m_audio, &AudioPlayer::AudioPlayerStateChanged, this, &MythPlayerVisualiserUI::AudioPlayerStateChanged);
    connect(m_mainWindow, &MythMainWindow::UIScreenRectChanged, this, &MythPlayerVisualiserUI::UIScreenRectChanged);
    connect(m_tv, &TV::EnableVisualiser, this, &MythPlayerVisualiserUI::EnableVisualiser);
    connect(m_tv, &TV::EmbedPlayback, this, &MythPlayerVisualiserUI::EmbedVisualiser);
    connect(this, &MythPlayerVisualiserUI::VisualiserStateChanged, m_tv, &TV::VisualiserStateChanged);


    m_defaultVisualiser = gCoreContext->GetSetting("AudioVisualiser", "");
    m_uiScreenRect = m_mainWindow->GetUIScreenRect();
    if (m_render)
    {
        m_visualiserState.m_visualiserList = VideoVisual::GetVisualiserList(m_render->Type());
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise m_visualiserList, m_render null");
    }
}

/// \brief Set initial state and update player
void MythPlayerVisualiserUI::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialising visualiser");
    emit VisualiserStateChanged(m_visualiserState);
    MythPlayerVideoUI::InitialiseState();
}

MythPlayerVisualiserUI::~MythPlayerVisualiserUI()
{
    MythPlayerVisualiserUI::DestroyVisualiser();
}

void MythPlayerVisualiserUI::AudioPlayerStateChanged(const MythAudioPlayerState& State)
{
    m_visualiserState.m_canVisualise = State.m_channels == 1 || State.m_channels == 2;
    emit VisualiserStateChanged(m_visualiserState);
}

void MythPlayerVisualiserUI::UIScreenRectChanged(QRect Rect)
{
    m_uiScreenRect = Rect;
}

void MythPlayerVisualiserUI::DestroyVisualiser()
{
    delete m_visual;
    m_visual = nullptr;
    m_visualiserState.m_visualiserName = QString();
    m_visualiserState.m_visualising = false;
}

void MythPlayerVisualiserUI::EnableVisualiser(bool Enable, bool Toggle, const QString &Name)
{
    bool osd = true;
    QString visualiser = Name;
    if (visualiser.startsWith("_AUTO_"))
    {
        visualiser = visualiser.mid(6);
        osd = false;
    }

    bool want = Enable || !visualiser.isEmpty();
    if (Toggle)
        want = !m_visualiserState.m_visualising;

    if (!want || !m_visualiserState.m_canVisualise)
    {
        DestroyVisualiser();
    }
    else if (m_visualiserState.m_canVisualise)
    {
        if (visualiser.isEmpty())
            visualiser = m_defaultVisualiser;

        if (m_visualiserState.m_visualiserName != visualiser)
        {
            DestroyVisualiser();
            m_visual = VideoVisual::Create(visualiser, &m_audio, m_render);
            if (m_visual)
            {
                m_visualiserState.m_visualiserName = m_visual->Name();
                m_visualiserState.m_visualising = true;
            }
        }
    }

    if (osd)
        UpdateOSDMessage(m_visual ? m_visualiserState.m_visualiserName : tr("Visualisation Off"));
    emit VisualiserStateChanged(m_visualiserState);
}

/*! \brief Enable visualisation if possible, there is no video and user has requested.
*/
void MythPlayerVisualiserUI::AutoVisualise(bool HaveVideo)
{
    m_checkAutoVisualise = false;

    if (HaveVideo || !m_audio.HasAudioIn() || m_defaultVisualiser.isEmpty())
        return;

    if (!m_visualiserState.m_canVisualise || m_visualiserState.m_visualising)
        return;

    EnableVisualiser(true, false, "_AUTO_" + m_defaultVisualiser);
}

void MythPlayerVisualiserUI::EmbedVisualiser(bool Embed, QRect Rect)
{
    m_embedRect = Rect;
    m_visualiserState.m_embedding = Embed;
    emit VisualiserStateChanged(m_visualiserState);
}

void MythPlayerVisualiserUI::PrepareVisualiser()
{
    // Note - we must call the visualiser to drain its buffers even if it is not
    // to be displayed (otherwise it fails). So do not check for output rect
    // size (embedding) etc
    if (!m_visual)
        return;
    if (m_visual->NeedsPrepare())
        m_visual->Prepare(m_visualiserState.m_embedding ? m_embedRect : m_uiScreenRect);
}

void MythPlayerVisualiserUI::RenderVisualiser()
{
    // As for PrepareVisualiser - always call the visualiser if it is present
    if (!m_visual)
        return;
    m_visual->Draw(m_visualiserState.m_embedding ? m_embedRect : m_uiScreenRect, m_painter, nullptr);
}
