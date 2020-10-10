// MythTV
#include "mythcorecontext.h"
#include "audioplayer.h"
#include "mythmainwindow.h"
#include "tv_play.h"
#include "mythplayervisualiserui.h"

MythPlayerVisualiserUI::MythPlayerVisualiserUI(MythMainWindow *MainWindow, TV *Tv,
                                           PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerUIBase(MainWindow, Tv, Context, Flags)
{
    m_uiScreenRect = m_mainWindow->GetUIScreenRect();
    connect(m_mainWindow, &MythMainWindow::UIScreenRectChanged, this, &MythPlayerVisualiserUI::UIScreenRectChanged);
    connect(m_tv, &TV::EmbedPlayback, this, &MythPlayerVisualiserUI::EmbedVisualiser);
}

MythPlayerVisualiserUI::~MythPlayerVisualiserUI()
{
    MythPlayerVisualiserUI::DestroyVisualiser();
}

void MythPlayerVisualiserUI::UIScreenRectChanged(const QRect& Rect)
{
    m_uiScreenRect = Rect;
}

void MythPlayerVisualiserUI::PrepareVisualiser()
{
    // Note - we must call the visualiser to drain its buffers even if it is not
    // to be displayed (otherwise it fails). So do not check for output rect
    // size (embedding) etc
    if (!m_visual)
        return;
    if (m_visual->NeedsPrepare())
        m_visual->Prepare(m_embedding ? m_embedRect : m_uiScreenRect);
}

void MythPlayerVisualiserUI::RenderVisualiser()
{
    // As for PrepareVisualiser - always call the visualiser if it is present
    if (!m_visual)
        return;
    m_visual->Draw(m_embedding ? m_embedRect : m_uiScreenRect, m_painter, nullptr);
}

void MythPlayerVisualiserUI::DestroyVisualiser()
{
    delete m_visual;
    m_visual = nullptr;
}

bool MythPlayerVisualiserUI::CanVisualise()
{
    return VideoVisual::CanVisualise(&m_audio, m_render);
}

bool MythPlayerVisualiserUI::IsVisualising()
{
    return m_visual != nullptr;
}

QString MythPlayerVisualiserUI::GetVisualiserName()
{
    if (m_visual)
        return m_visual->Name();
    return QString();
}

QStringList MythPlayerVisualiserUI::GetVisualiserList()
{
    return VideoVisual::GetVisualiserList(m_render->Type());
}

bool MythPlayerVisualiserUI::EnableVisualiser(bool Enable, const QString &Name)
{
    if (!Enable)
    {
        DestroyVisualiser();
        return false;
    }
    DestroyVisualiser();
    m_visual = VideoVisual::Create(Name, &m_audio, m_render);
    return m_visual != nullptr;
}

/*! \brief Enable visualisation if possible, there is no video and user has requested.
*/
void MythPlayerVisualiserUI::AutoVisualise(bool HaveVideo)
{
    if (!m_audio.HasAudioIn() || HaveVideo)
        return;

    if (!CanVisualise() || IsVisualising())
        return;

    auto visualiser = gCoreContext->GetSetting("AudioVisualiser", "");
    if (!visualiser.isEmpty())
        EnableVisualiser(true, visualiser);
}

void MythPlayerVisualiserUI::EmbedVisualiser(bool Embed, const QRect &Rect)
{
    m_embedRect = Rect;
    m_embedding = Embed;
}

bool MythPlayerVisualiserUI::IsEmbedding()
{
    return m_embedding;
}
