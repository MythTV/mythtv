// Qt
#include <QDir>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/remoteutil.h"
#include "libmythmetadata/dbaccess.h"
#include "libmythmetadata/videometadata.h"
#include "libmythmetadata/videoutils.h"
#include "libmythui/mythmainwindow.h"

// MythFrontend
#include "videoplayercommand.h"

namespace
{
    QString ShellEscape(const QString &src)
    {
        return QString(src)
                .replace("\"",  "\\\"")
                .replace("`",   "\\`")
                .replace("\\$", "\\$");
    }

    QString ExpandPlayCommand(const QString &command, const QString &filename)
    {
        // If handler contains %d, substitute default player command
        // This would be used to add additional switches to the default without
        // needing to retype the whole default command.  But, if the
        // command and the default command both contain %s, drop the %s from
        // the default since the new command already has it
        //
        // example: default: mplayer -fs %s
        //          custom : %d -ao alsa9:spdif %s
        //          result : mplayer -fs -ao alsa9:spdif %s
        QString tmp = command;
        if (tmp.contains("%d"))
        {
            QString default_handler =
                    gCoreContext->GetSetting("VideoDefaultPlayer");
            if (tmp.contains("%s") && default_handler.contains("%s"))
                default_handler = default_handler.replace("%s", "");
            tmp.replace("%d", default_handler);
        }

        QString arg = QString("\"%1\"").arg(ShellEscape(filename));

        if (tmp.contains("%s"))
            return tmp.replace("%s", arg);

        return QString("%1 %2").arg(tmp, arg);
    }
}

////////////////////////////////////////////////////////////////////////

struct VideoPlayProc
{
  protected:
    VideoPlayProc() = default;
    VideoPlayProc &operator=(const VideoPlayProc &);

  public:
    virtual ~VideoPlayProc() = default;
    VideoPlayProc(const VideoPlayProc&) = default;

    // returns true if item played
    virtual bool Play() const = 0;

    virtual QString GetCommandDisplayName() const = 0;

    virtual VideoPlayProc *Clone() const = 0;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayHandleMedia : public VideoPlayProc
{
  private:
    VideoPlayHandleMedia(QString handler, QString mrl,
            QString plot, QString title, QString subtitle,
            QString director, int season, int episode, QString inetref,
            std::chrono::minutes length, QString year, QString id) :
        m_handler(std::move(handler)), m_mrl(std::move(mrl)),
        m_plot(std::move(plot)), m_title(std::move(title)),
        m_subtitle(std::move(subtitle)),
        m_director(std::move(director)), m_season(season),
        m_episode(episode), m_inetref(std::move(inetref)),
        m_length(length), m_year(std::move(year)),
        m_id(std::move(id))
    {
    }

  public:
    static VideoPlayHandleMedia *Create(const QString &handler,
            const QString &mrl, const QString &plot, const QString &title,
            const QString &subtitle, const QString &director,
            int season, int episode, const QString &inetref,
            std::chrono::minutes length, const QString &year, const QString &id)
    {
        return new VideoPlayHandleMedia(handler, mrl, plot, title, subtitle,
                director, season, episode, inetref, length, year, id);
    }

    bool Play() const override // VideoPlayProc
    {
        return GetMythMainWindow()->HandleMedia(m_handler, m_mrl,
                m_plot, m_title, m_subtitle, m_director, m_season,
                m_episode, m_inetref, m_length, m_year, m_id, true);
    }

    QString GetCommandDisplayName() const override // VideoPlayProc
    {
        return m_handler;
    }

    VideoPlayHandleMedia *Clone() const override // VideoPlayProc
    {
        return new VideoPlayHandleMedia(*this);
    }

  private:
    QString m_handler;
    QString m_mrl;
    QString m_plot;
    QString m_title;
    QString m_subtitle;
    QString m_director;
    int m_season;
    int m_episode;
    QString m_inetref;
    std::chrono::minutes m_length;
    QString m_year;
    QString m_id;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayMythSystem : public VideoPlayProc
{
  private:
    VideoPlayMythSystem(QString disp_command,
            QString play_command) :
        m_displayCommand(std::move(disp_command)), m_playCommand(std::move(play_command))
    {
    }

  public:
    static VideoPlayMythSystem *Create(const QString &command,
            const QString &filename)
    {
        return new VideoPlayMythSystem(command,
                ExpandPlayCommand(command, filename));
    }

    bool Play() const override // VideoPlayProc
    {
        myth_system(m_playCommand);

        return true;
    }

    QString GetCommandDisplayName() const override // VideoPlayProc
    {
        return m_displayCommand;
    }

    VideoPlayMythSystem *Clone() const override // VideoPlayProc
    {
        return new VideoPlayMythSystem(*this);
    }

  private:
    QString m_displayCommand;
    QString m_playCommand;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayerCommandPrivate
{
  public:
    VideoPlayerCommandPrivate() = default;

    VideoPlayerCommandPrivate(const VideoPlayerCommandPrivate &other)
    {
        auto playerclone = [](auto *player) { return player->Clone(); };
        std::transform(other.m_playerProcs.cbegin(), other.m_playerProcs.cend(),
                       std::back_inserter(m_playerProcs), playerclone);
    }

    VideoPlayerCommandPrivate &operator=(const VideoPlayerCommandPrivate &rhs) = delete;

    ~VideoPlayerCommandPrivate()
    {
        ClearPlayerList();
    }

    void AltPlayerFor(const VideoMetadata *item)
    {
        if (item)
        {
            QString play_command =
                   gCoreContext->GetSetting("mythvideo.VideoAlternatePlayer");
            QString filename;

            if (item->IsHostSet())
            {
                filename = generate_file_url("Videos", item->GetHost(),
                        item->GetFilename());
            }
            else
            {
                filename = item->GetFilename();
            }

            if (!play_command.isEmpty())
            {
                AddPlayer(play_command, filename, item->GetPlot(),
                        item->GetTitle(), item->GetSubtitle(),
                        item->GetDirector(), item->GetSeason(),
                        item->GetEpisode(), item->GetInetRef(),
                        item->GetLength(), QString::number(item->GetYear()),
                        QString::number(item->GetID()));
            }
            else
            {
                PlayerFor(filename, item);
            }
        }
    }

    void PlayerFor(const VideoMetadata *item)
    {
        if (item)
        {
            const QString& play_command = item->GetPlayCommand();
            QString filename;

            if (item->IsHostSet())
            {
                filename = generate_file_url("Videos", item->GetHost(),
                        item->GetFilename());
            }
            else
            {
                filename = item->GetFilename();
            }

            if (!play_command.isEmpty())
            {
                AddPlayer(play_command, filename, item->GetPlot(),
                        item->GetTitle(), item->GetSubtitle(),
                        item->GetDirector(), item->GetSeason(),
                        item->GetEpisode(), item->GetInetRef(),
                        item->GetLength(), QString::number(item->GetYear()),
                        QString::number(item->GetID()));
            }
            else
            {
                PlayerFor(filename, item);
            }
        }
    }

    void PlayerFor(const QString &filename, const VideoMetadata *extraData = nullptr)
    {
        QString extension = filename.section(".", -1, -1);
        QDir dir_test(QString("%1/VIDEO_TS").arg(filename));
        if (dir_test.exists())
            extension = "VIDEO_TS";
        QDir bd_dir_test(QString("%1/BDMV").arg(filename));
        if (bd_dir_test.exists())
            extension = "BDMV";

        QString play_command = gCoreContext->GetSetting("VideoDefaultPlayer");

        const FileAssociations::association_list fa_list =
                FileAssociations::getFileAssociation().getList();
        auto sameext = [extension](const auto & fa)
            { return fa.extension.toLower() == extension.toLower() &&
                     !fa.use_default; };
        auto fa = std::find_if(fa_list.cbegin(), fa_list.cend(), sameext);
        if (fa != fa_list.cend())
            play_command = fa->playcommand;

        if (play_command.trimmed().isEmpty())
            play_command = "Internal";

        QString plot;
        QString title = VideoMetadata::FilenameToMeta(filename, 1);
        QString subtitle = VideoMetadata::FilenameToMeta(filename, 4);
        QString director;
        int season = 0;
        int episode = 0;
        QString inetref;
        std::chrono::minutes length = 0min;
        QString year = QString::number(VIDEO_YEAR_DEFAULT);
        QString id;

        if (extraData)
        {
            plot = extraData->GetPlot();
            title = extraData->GetTitle();
            subtitle = extraData->GetSubtitle();
            director = extraData->GetDirector();
            season = extraData->GetSeason();
            episode = extraData->GetEpisode();
            inetref = extraData->GetInetRef();
            length = extraData->GetLength();
            year = QString::number(extraData->GetYear());
            id = QString::number(extraData->GetID());
        }

        AddPlayer(play_command, filename, plot, title, subtitle, director,
                                season, episode, inetref, length, year, id);
    }

    void ClearPlayerList()
    {
        for (auto & player : m_playerProcs)
            delete player;
        m_playerProcs.clear();
    }

    void Play() const
    {
        // Do this until one of the players returns true
        (void)std::any_of(m_playerProcs.cbegin(), m_playerProcs.cend(),
                          [](auto *player){ return player->Play(); } );
    }

    QString GetCommandDisplayName() const
    {
        if (!m_playerProcs.empty())
            return m_playerProcs.front()->GetCommandDisplayName();
        return {};
    }

  private:
    void AddPlayer(const QString &player, const QString &filename,
            const QString &plot, const QString &title, const QString &subtitle,
            const QString &director, int season, int episode, const QString &inetref,
                   std::chrono::minutes length, const QString &year, const QString &id)
    {
        m_playerProcs.push_back(VideoPlayHandleMedia::Create(player, filename,
                        plot, title, subtitle, director, season, episode, inetref,
                        length, year, id));
        m_playerProcs.push_back(VideoPlayMythSystem::Create(player, filename));
    }

  private:
    using player_list = std::vector<VideoPlayProc *>;
    player_list m_playerProcs;
};

////////////////////////////////////////////////////////////////////////

VideoPlayerCommand VideoPlayerCommand::AltPlayerFor(const VideoMetadata *item)
{
    VideoPlayerCommand ret;
    ret.m_d->AltPlayerFor(item);
    return ret;
}

VideoPlayerCommand VideoPlayerCommand::PlayerFor(const VideoMetadata *item)
{
    VideoPlayerCommand ret;
    ret.m_d->PlayerFor(item);
    return ret;
}

VideoPlayerCommand VideoPlayerCommand::PlayerFor(const QString &filename)
{
    VideoPlayerCommand ret;
    ret.m_d->PlayerFor(filename);
    return ret;
}

VideoPlayerCommand::VideoPlayerCommand()
  : m_d(new VideoPlayerCommandPrivate)
{
}

VideoPlayerCommand::~VideoPlayerCommand()
{
    delete m_d;
    m_d = nullptr;
}

VideoPlayerCommand::VideoPlayerCommand(const VideoPlayerCommand &other)
  : m_d(new VideoPlayerCommandPrivate(*other.m_d))
{
}

VideoPlayerCommand &VideoPlayerCommand::operator=(const VideoPlayerCommand &rhs)
{
    if (this != &rhs)
    {
        delete m_d;
        m_d = new VideoPlayerCommandPrivate(*rhs.m_d);
    }
    return *this;
}

void VideoPlayerCommand::Play() const
{
    LCD *lcd = LCD::Get();

    if (lcd) {
        lcd->setFunctionLEDs(FUNC_TV, false);
        lcd->setFunctionLEDs(FUNC_MOVIE, true);
    }
    gCoreContext->WantingPlayback(nullptr);
    GetMythMainWindow()->PauseIdleTimer(true);
    m_d->Play();
    gCoreContext->emitTVPlaybackStopped();
    GetMythMainWindow()->PauseIdleTimer(false);
    GetMythMainWindow()->raise();
    GetMythMainWindow()->activateWindow();
    if (lcd)
        lcd->setFunctionLEDs(FUNC_MOVIE, false);
}

QString VideoPlayerCommand::GetCommandDisplayName() const
{
    return m_d->GetCommandDisplayName();
}
