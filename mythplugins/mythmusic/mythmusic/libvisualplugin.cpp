
#include <iostream>
using namespace std;

#include <mythcontext.h>
#include <compat.h>
#include <mythlogging.h>

#include "libvisualplugin.h"

#if defined(SDL_SUPPORT) && defined(LIBVISUAL_SUPPORT)

static char AppName[] = "mythmusic";

LibVisualPlugin::LibVisualPlugin(
    MainVisual *parent, long int winid, const QString &pluginName)
{
    fps = 30;
    m_parent = parent;
    m_pVisBin = 0;
    m_pVisVideo = 0;
    m_pSurface = 0;
    m_paused = false;

    // SDL initialisation
    char SDL_windowhack[32];
    sprintf(SDL_windowhack, "%ld", winid);
    setenv("SDL_WINDOWID", SDL_windowhack, 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to init SDL");
        return;
    }
    SDL_ShowCursor(0);


    visual_log_set_verboseness(VISUAL_LOG_VERBOSENESS_LOW);

    // LibVisualPlugin initialisation
    if (!visual_is_initialized())
    {
        char **argv;
        int argc;
        argv = (char**)malloc(sizeof(char*));
        argv[0] = AppName;
        argc = 1;
        visual_init(&argc, &argv);
        free(argv);
    }


    // get list of available plugins
    const char *plugin = 0;
    while( (plugin = visual_actor_get_next_by_name( plugin )) )
        m_pluginList << plugin;

    m_currentPlugin = 0;
    if (!pluginName.isEmpty() &&
            (m_pluginList.find(pluginName) != m_pluginList.end()))
        switchToPlugin(pluginName);
    else
        switchToPlugin(m_pluginList[0]);
}

void LibVisualPlugin::handleKeyPress(const QString &action)
{
    if (action == "SELECT")
    {
        m_currentPlugin++;
        if (m_currentPlugin >= (uint)m_pluginList.count())
            m_currentPlugin = 0;

        // gstreamer causes a segfault so don't use it
        if (m_pluginList[m_currentPlugin] == "gstreamer")
        {
            m_currentPlugin++;
            if (m_currentPlugin >= (uint)m_pluginList.count())
                m_currentPlugin = 0;
        }

        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
            SDL_LockSurface(m_pSurface);

        visual_bin_set_morph_by_name(
            m_pVisBin, const_cast<char*>("alphablend"));

        QByteArray plugin = m_pluginList[m_currentPlugin].toAscii();
        visual_bin_switch_actor_by_name(
            m_pVisBin, const_cast<char*>(plugin.constData()));

        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
            SDL_UnlockSurface(m_pSurface);

        m_parent->showBanner(
            "Visualizer: " + m_pluginList[m_currentPlugin], 8000);
    }
}

void LibVisualPlugin::switchToPlugin(const QString &pluginName)
{
    if (m_pVisVideo)
    {
        visual_object_unref(VISUAL_OBJECT(m_pVisVideo));
        m_pVisVideo = 0;
    }

    if (m_pVisBin)
    {
        visual_object_unref(VISUAL_OBJECT(m_pVisBin));
        m_pVisBin = 0;
    }

    m_pVisBin = visual_bin_new();
    if (!m_pVisBin)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error allocating LibVisualPlugin 'Bin' object");
        return;
    }

    visual_bin_set_supported_depth(m_pVisBin, VISUAL_VIDEO_DEPTH_ALL);

    m_pVisVideo = visual_video_new();
    if (!m_pVisVideo)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error allocating LibVisualPlugin 'Video' object");
        return;
    }

    if (visual_bin_set_video(m_pVisBin, m_pVisVideo) != VISUAL_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error connecting LibVisualPlugin"
                                 " 'Video' object to 'Bin' object");
        return;
    }

    QByteArray plugin = pluginName.toAscii();
    if (visual_bin_connect_by_names(
            m_pVisBin, const_cast<char*>(plugin.constData()), 0) != VISUAL_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error connecting LibVisualPlugin"
                                 " 'Plugin' object to 'Bin' object");
        return;
    }

    // Work around visualisers that don't initialise their buffer:
    QSize size(100, 100);
    visual_video_set_dimension(m_pVisVideo, size.width(), size.height());
    createScreen(size.width(), size.height());

    if (visual_input_set_callback(
            visual_bin_get_input(m_pVisBin), AudioCallback, this) == VISUAL_OK)
    {
        visual_bin_switch_set_style(m_pVisBin, VISUAL_SWITCH_STYLE_MORPH);
        visual_bin_switch_set_automatic(m_pVisBin, true);
        visual_bin_switch_set_steps(m_pVisBin, 100);
        visual_bin_realize(m_pVisBin);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Error connecting LibVisualPlugin"
                                 " 'Input' object to our data source object");
    }
}

LibVisualPlugin::~LibVisualPlugin()
{
    if (m_pVisVideo)
    {
         visual_object_unref(VISUAL_OBJECT(m_pVisVideo));
         m_pVisVideo = 0;
    }

    if (m_pVisBin)
    {
         visual_object_unref(VISUAL_OBJECT(m_pVisBin));
         m_pVisBin = 0;
    }

    SDL_Quit();

    //    visual_quit();   // This appears to cause memory corruption :-(

    unsetenv("SDL_WINDOWID");
}

void LibVisualPlugin::resize(const QSize &size)
{
    visual_video_set_dimension(m_pVisVideo, size.width(), size.height());
    createScreen(size.width(), size.height());
    visual_bin_sync(m_pVisBin, false);
}


bool LibVisualPlugin::process(VisualNode *node)
{
    if (!node || node->length == 0 || !m_pSurface)
        return true;

    int numSamps = 512;
    if (node->length < 512)
        numSamps = node->length;

    int i = 0;
    for (i = 0; i < numSamps; i++)
    {
        m_Audio[0][i] = node->left[i];
        if (node->right)
            m_Audio[1][i] = node->right[i];
        else
            m_Audio[1][i] = m_Audio[0][i];
    }

    for (; i < 512; i++)
    {
        m_Audio[0][i] = 0;
        m_Audio[1][i] = 0;
    }

    return false;
}

bool LibVisualPlugin::createScreen(int width, int height)
{
    SDL_FreeSurface(m_pSurface);

    if (visual_bin_get_depth(m_pVisBin) == VISUAL_VIDEO_DEPTH_GL)
    {
        visual_video_set_depth(m_pVisVideo, VISUAL_VIDEO_DEPTH_GL);

        if (const SDL_VideoInfo* VideoInfo = SDL_GetVideoInfo())
        {
            int VideoFlags = SDL_OPENGL | SDL_GL_DOUBLEBUFFER | SDL_HWPALETTE;
            VideoFlags |= VideoInfo->hw_available
                          ? SDL_HWSURFACE : SDL_SWSURFACE;
            VideoFlags |= VideoInfo->blit_hw ? SDL_HWACCEL : 0;

            SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);

            m_pSurface = SDL_SetVideoMode(width, height, 16, VideoFlags);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Error obtaining SDL video information");
        }
    }
    else
    {
        m_pSurface = SDL_SetVideoMode(width, height, m_pVisVideo->bpp * 8, 0);
    }

    visual_video_set_buffer(m_pVisVideo, m_pSurface->pixels);
    visual_video_set_pitch(m_pVisVideo, m_pSurface->pitch);

    return true;
}

bool LibVisualPlugin::draw(QPainter *, const QColor&)
{
    if (visual_bin_depth_changed(m_pVisBin))
    {
        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
        {
            SDL_LockSurface(m_pSurface);
        }

        createScreen(m_pSurface->w, m_pSurface->h);
        visual_bin_sync(m_pVisBin, true);
        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
        {
            SDL_UnlockSurface(m_pSurface);
        }

    }

    if (visual_bin_get_depth(m_pVisBin) == VISUAL_VIDEO_DEPTH_GL)
    {
        visual_bin_run(m_pVisBin);
        SDL_GL_SwapBuffers();
    }
    else
    {
        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
        {
            SDL_LockSurface(m_pSurface);
        }

        visual_video_set_buffer(m_pVisVideo, m_pSurface->pixels);
        visual_bin_run(m_pVisBin);

        if (SDL_MUSTLOCK(m_pSurface) == SDL_TRUE)
        {
            SDL_UnlockSurface(m_pSurface);
        }

        if (VisPalette* pVisPalette = visual_bin_get_palette(m_pVisBin))
        {
            SDL_Color Palette[256];

            for(int i = 0; i < 256; i ++)
            {
                Palette[i].r = pVisPalette->colors[i].r;
                Palette[i].g = pVisPalette->colors[i].g;
                Palette[i].b = pVisPalette->colors[i].b;
            }
            SDL_SetColors(m_pSurface, Palette, 0, 256);
        }
        SDL_Flip(m_pSurface);
    }

    return false;
}

// static member function
int LibVisualPlugin::AudioCallback(VisInput*, VisAudio* audio, void* priv)
{
    LibVisualPlugin* that = static_cast<LibVisualPlugin*>(priv);

    VisBuffer buf;
    visual_buffer_init(&buf, that->m_Audio, 1024, 0);
    visual_audio_samplepool_input(
        audio->samplepool, &buf, VISUAL_AUDIO_SAMPLE_RATE_44100,
        VISUAL_AUDIO_SAMPLE_FORMAT_S16, VISUAL_AUDIO_SAMPLE_CHANNEL_STEREO);

    return 0;
}

// static member function
uint LibVisualPlugin::plugins(QStringList *list)
{
    visual_log_set_verboseness(VISUAL_LOG_VERBOSENESS_LOW);

    if (!visual_is_initialized())
    {
        char **argv;
        int argc;
        argv = (char**)malloc(sizeof(char*));
        argv[0] = AppName;
        argc = 1;
        visual_init(&argc, &argv);
        free(argv);
    }

    // get list of available plugins
    const char *plugin = 0;
    uint count = 0;
    while((plugin = visual_actor_get_next_by_name(plugin)))
    {
        *list << QString("LibVisual-") + plugin;
        count++;
    }

//    visual_quit();

    return count;
}

static class LibVisualFactory : public VisFactory
{
    public:
        const QString &name(void) const
        {
            static QString name("LibVisual");
            return name;
        }

        uint plugins(QStringList *list) const
        {
            return LibVisualPlugin::plugins(list);
        }

        VisualBase *create(MainVisual *parent,
                           long int winid, const QString &pluginName) const
        {
            return new LibVisualPlugin(parent, winid, pluginName);
        }
}LibVisualFactory;

#endif // SDL_SUPPORT && LIBVISUAL_SUPPORT
