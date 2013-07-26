#include <QDir>

#include "mythcontext.h"

#include "mythmainwindow.h"
#include "mythsystemlegacy.h"
#include "remoteutil.h"
#include "lcddevice.h"
#include "mythmiscutil.h"
#include "dbaccess.h"
#include "videometadata.h"
#include "videoutils.h"

#include "videoplayercommand.h"

namespace
{
    QString ShellEscape(const QString &src)
    {
        return QString(src)
                .replace(QRegExp("\""), "\\\"")
                .replace(QRegExp("`"), "\\`")
                .replace(QRegExp("\\$"), "\\$");
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
                default_handler = default_handler.replace(QRegExp("%s"), "");
            tmp.replace(QRegExp("%d"), default_handler);
        }

        QString arg = QString("\"%1\"").arg(ShellEscape(filename));

        if (tmp.contains("%s"))
            return tmp.replace(QRegExp("%s"), arg);

        return QString("%1 %2").arg(tmp).arg(arg);
    }
}

////////////////////////////////////////////////////////////////////////

struct VideoPlayProc
{
  protected:
    VideoPlayProc() {}
    VideoPlayProc &operator=(const VideoPlayProc &);

  public:
    virtual ~VideoPlayProc() {}

    // returns true if item played
    virtual bool Play() const = 0;

    virtual QString GetCommandDisplayName() const = 0;

    virtual VideoPlayProc *Clone() const = 0;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayHandleMedia : public VideoPlayProc
{
  private:
    VideoPlayHandleMedia(const QString &handler, const QString &mrl,
            const QString &plot, const QString &title, const QString &subtitle,
            const QString &director, int season, int episode, const QString &inetref,
            int length, const QString &year, const QString &id) :
        m_handler(handler), m_mrl(mrl), m_plot(plot), m_title(title),
        m_subtitle(subtitle), m_director(director), m_season(season),
        m_episode(episode), m_inetref(inetref), m_length(length), m_year(year),
        m_id(id)
    {
    }

  public:
    static VideoPlayHandleMedia *Create(const QString &handler,
            const QString &mrl, const QString &plot, const QString &title,
            const QString &subtitle, const QString &director,
            int season, int episode, const QString &inetref,
            int length, const QString &year, const QString &id)
    {
        return new VideoPlayHandleMedia(handler, mrl, plot, title, subtitle,
                director, season, episode, inetref, length, year, id);
    }

    bool Play() const
    {
        return GetMythMainWindow()->HandleMedia(m_handler, m_mrl,
                m_plot, m_title, m_subtitle, m_director, m_season,
                m_episode, m_inetref, m_length, m_year, m_id, true);
    }

    QString GetCommandDisplayName() const
    {
        return m_handler;
    }

    VideoPlayHandleMedia *Clone() const
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
    int m_length;
    QString m_year;
    QString m_id;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayMythSystem : public VideoPlayProc
{
  private:
    VideoPlayMythSystem(const QString &disp_command,
            const QString &play_command) :
        m_display_command(disp_command), m_play_command(play_command)
    {
    }

  public:
    static VideoPlayMythSystem *Create(const QString &command,
            const QString &filename)
    {
        return new VideoPlayMythSystem(command,
                ExpandPlayCommand(command, filename));
    }

    bool Play() const
    {
        myth_system(m_play_command);

        return true;
    }

    QString GetCommandDisplayName() const
    {
        return m_display_command;
    }

    VideoPlayMythSystem *Clone() const
    {
        return new VideoPlayMythSystem(*this);
    }

  private:
    QString m_display_command;
    QString m_play_command;
};

////////////////////////////////////////////////////////////////////////

class VideoPlayerCommandPrivate
{
  private:
    VideoPlayerCommandPrivate &operator=(const VideoPlayerCommandPrivate &rhs);

  public:
    VideoPlayerCommandPrivate() {}

    VideoPlayerCommandPrivate(const VideoPlayerCommandPrivate &other)
    {
        for (player_list::const_iterator p = other.m_player_procs.begin();
                p != other.m_player_procs.end(); ++p)
        {
            m_player_procs.push_back((*p)->Clone());
        }
    }

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
                filename = generate_file_url("Videos", item->GetHost(),
                        item->GetFilename());
            else
                filename = item->GetFilename();

            if (play_command.length())
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
            QString play_command = item->GetPlayCommand();
            QString filename;

            if (item->IsHostSet())
                filename = generate_file_url("Videos", item->GetHost(),
                        item->GetFilename());
            else
                filename = item->GetFilename();

            if (play_command.length())
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

    void PlayerFor(const QString &filename, const VideoMetadata *extraData = 0)
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
        for (FileAssociations::association_list::const_iterator p =
                fa_list.begin(); p != fa_list.end(); ++p)
        {
            if (p->extension.toLower() == extension.toLower() &&
                    !p->use_default)
            {
                play_command = p->playcommand;
                break;
            }
        }

        if (play_command.trimmed().isEmpty())
            play_command = "Internal";

        QString plot;
        QString title = VideoMetadata::FilenameToMeta(filename, 1);
        QString subtitle = VideoMetadata::FilenameToMeta(filename, 4);
        QString director;
        int season = 0;
        int episode = 0;
        QString inetref;
        int length = 0;
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
        for (player_list::iterator p = m_player_procs.begin();
                        p != m_player_procs.end(); ++p)
        {
            delete *p;
        }
        m_player_procs.clear();
    }

    void Play() const
    {
        for (player_list::const_iterator p = m_player_procs.begin();
                p != m_player_procs.end(); ++p)
        {
            if ((*p)->Play()) break;
        }
    }

    QString GetCommandDisplayName() const
    {
        if (!m_player_procs.empty())
            return m_player_procs.front()->GetCommandDisplayName();
        return QString();
    }

  private:
    void AddPlayer(const QString &player, const QString &filename,
            const QString &plot, const QString &title, const QString &subtitle,
            const QString &director, int season, int episode, const QString &inetref,
            int length, const QString &year, const QString &id)
    {
        m_player_procs.push_back(VideoPlayHandleMedia::Create(player, filename,
                        plot, title, subtitle, director, season, episode, inetref,
                        length, year, id));
        m_player_procs.push_back(VideoPlayMythSystem::Create(player, filename));
    }

  private:
    typedef std::vector<VideoPlayProc *> player_list;
    player_list m_player_procs;
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

VideoPlayerCommand::VideoPlayerCommand() : m_d(0)
{
    m_d = new VideoPlayerCommandPrivate;
}

VideoPlayerCommand::~VideoPlayerCommand()
{
    delete m_d;
    m_d = 0;
}

VideoPlayerCommand::VideoPlayerCommand(const VideoPlayerCommand &other)
{
    m_d = new VideoPlayerCommandPrivate(*other.m_d);
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
    gCoreContext->WantingPlayback(NULL);
    GetMythMainWindow()->PauseIdleTimer(true);
    m_d->Play();
    gCoreContext->emitTVPlaybackStopped();
    GetMythMainWindow()->PauseIdleTimer(false);
    GetMythMainWindow()->raise();
    GetMythMainWindow()->activateWindow();
    if (GetMythMainWindow()->currentWidget())
        GetMythMainWindow()->currentWidget()->setFocus();
    if (lcd)
        lcd->setFunctionLEDs(FUNC_MOVIE, false);
}

QString VideoPlayerCommand::GetCommandDisplayName() const
{
    return m_d->GetCommandDisplayName();
}
