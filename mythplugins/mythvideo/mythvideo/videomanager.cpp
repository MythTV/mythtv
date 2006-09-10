#include <qapplication.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qurloperator.h>
#include <qprocess.h>
#include <qpainter.h>

#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/xmlparse.h>

#include "globals.h"
#include "videomanager.h"
#include "videolist.h"

#include "videofilter.h"
#include "metadata.h"
#include "editmetadata.h"
#include "videoutils.h"

namespace
{
    class ListBehaviorManager
    {
      public:
        enum ListBehavior
        {
            lbNone = 0x0,
            lbScrollCenter = 0x1,
            lbWrapList = 0x2
        };

        struct lb_data
        {
            unsigned int begin;
            unsigned int end; // one past the end
            unsigned int index;
            unsigned int item_index;
            bool data_above_window; // true if items "before" window
            bool data_below_window; // true if items "past" window
        };

      public:
        ListBehaviorManager(unsigned int window_size = 0, int behavior = lbNone,
                            unsigned int item_count = 0) :
            m_window_size(window_size), m_item_count(item_count), m_index(0),
            m_skip_index(SKIP_MAX)
        {
            m_scroll_center = behavior & lbScrollCenter;
            m_wrap_list = behavior & lbWrapList;
        }

        void setWindowSize(unsigned int window_size)
        {
            m_window_size = window_size;
        }

        unsigned int getWindowSize()
        {
            return m_window_size;
        }

        void setItemCount(unsigned int item_count)
        {
            m_item_count = item_count;
        }

        unsigned int getItemCount()
        {
            return m_item_count;
        }

        const lb_data &setIndex(unsigned int index)
        {
            m_index = index;
            return update_data();
        }

        unsigned int getIndex()
        {
            return m_index;
        }

        void setSkipIndex(unsigned int skip = SKIP_MAX)
        {
            m_skip_index = skip;
        }

        const lb_data &move_up()
        {
            if (m_index)
                --m_index;
            else if (m_wrap_list)
                m_index = m_item_count - 1;
            return update_data(mdUp);
        }

        const lb_data &move_down()
        {
            ++m_index;
            return update_data(mdDown);
        }

        const lb_data &getData()
        {
            return update_data();
        }

        const lb_data &page_up()
        {
            if (m_index == 0)
                m_index = m_item_count - 1;
            else if (m_index < m_window_size - 1)
                m_index = 0;
            else
                m_index -= m_window_size;
            return update_data(mdUp);
        }

        const lb_data &page_down()
        {
            if (m_index == m_item_count - 1)
                m_index = 0;
            else if (m_index + m_window_size > m_item_count - 1)
                m_index = m_item_count - 1;
            else
                m_index += m_window_size;
            return update_data(mdDown);
        }

      private:
        enum movement_direction
        {
            mdNone,
            mdUp,
            mdDown
        };

        const lb_data &update_data(movement_direction direction = mdNone)
        {
            if (m_item_count == 1)
            {
                m_index = 0;
                m_data.begin = 0;
                m_data.end = 1;
                m_data.index = 0;
                m_data.item_index = 0;
                m_data.data_below_window = false;
            }
            else if (m_item_count)
            {
                const unsigned int last_item_index = m_item_count - 1;

                if (m_skip_index != SKIP_MAX && m_index == m_skip_index)
                {
                    if (direction == mdDown)
                        ++m_index;
                    else if (direction == mdUp)
                        --m_index;
                }

                if (m_wrap_list)
                {
                    if (m_index > last_item_index)
                    {
                        m_index = 0;
                    }
                }
                else
                {
                    if (m_index > last_item_index)
                    {
                        m_index = last_item_index;
                    }
                }

                const unsigned int max_window_size = m_window_size - 1;

                if (m_scroll_center && m_item_count > max_window_size)
                {
                    unsigned int center_buffer = m_window_size / 2;
                    if (m_index < center_buffer)
                    {
                        m_data.begin = 0;
                        m_data.end = std::min(last_item_index, max_window_size);
                    }
                    else if (m_index > m_item_count - center_buffer)
                    {
                        m_data.begin = last_item_index - max_window_size;
                        m_data.end = last_item_index;
                    }
                    else
                    {
                        m_data.begin = m_index - center_buffer;
                        m_data.end = m_index + center_buffer - 1;
                    }
                }
                else
                {
                    if (m_index <= max_window_size)
                    {
                        m_data.begin = 0;
                        m_data.end = std::min(last_item_index, max_window_size);
                    }
                    else
                    {
                        m_data.begin = m_index - max_window_size;
                        m_data.end = m_index;
                    }
                }
                m_data.index = m_index - m_data.begin;

                m_data.data_below_window = m_data.end < m_item_count - 1;
                m_data.data_above_window = m_data.begin != 0;
                m_data.item_index = m_index;
                if (m_data.end != 0) ++m_data.end;
            }
            else
            {
                m_index = 0;
                m_data.begin = 0;
                m_data.end = 0;
                m_data.index = 0;
                m_data.item_index = 0;
                m_data.data_below_window = false;
            }

            return m_data;
        }

      private:
        unsigned int m_window_size;
        unsigned int m_item_count;
        unsigned int m_index;
        unsigned int m_skip_index;

        static const unsigned int SKIP_MAX = -1;

        bool m_scroll_center;
        bool m_wrap_list;
        lb_data m_data;
    };

    const unsigned int ListBehaviorManager::SKIP_MAX;

    QString executeExternal(const QStringList &args, const QString &purpose);
}

VideoManager::VideoManager(MythMainWindow *lparent,  const QString &lname,
                           VideoList *video_list) :
    MythDialog(lparent, lname), updateML(false), noUpdate(false),
    m_video_list(video_list), curIMDBNum(""), curitem(NULL), curitemMovie(""),
    m_state(SHOWING_MAINWINDOW), popup(NULL), expectingPopup(false),
    isbusy(false)
{
    videoDir = gContext->GetSetting("VideoStartupDir");
    artDir = gContext->GetSetting("VideoArtworkDir");

    VideoFilterSettings video_filter(true, "VideoManager");
    m_video_list->setCurrentVideoFilter(video_filter);

    m_list_behave.reset(new ListBehaviorManager(0,
                    ListBehaviorManager::lbScrollCenter |
                    ListBehaviorManager::lbWrapList));
    m_movie_list_behave.reset(new ListBehaviorManager);

    backup.reset(new QPainter);
    RefreshMovieList(false);

    fullRect = QRect(0, 0, size().width(), size().height());

    m_theme.reset(new XMLParse());
    m_theme->SetWMult(wmult);
    m_theme->SetHMult(hmult);
    QDomElement xmldata;
    m_theme->LoadTheme(xmldata, "manager", "video-");
    LoadWindow(xmldata);

    LayerSet *container = m_theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            m_list_behave->setWindowSize(ltype->GetItems());
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("MythVideo: VideoManager : Failed to "
                                      "get selector object."));
        exit(0);
    }

    container = m_theme->GetSet("moviesel");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            m_movie_list_behave->setWindowSize(ltype->GetItems());
        }
    }

    bgTransBackup.reset(gContext->LoadScalePixmap("trans-backup.png"));
    if (!bgTransBackup.get())
        bgTransBackup.reset(new QPixmap());

    updateBackground();

    setNoErase();
}

VideoManager::~VideoManager()
{
}

void VideoManager::keyPressEvent(QKeyEvent *e)
{
    if (isbusy) // ignore keypresses while doing http query
        return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
        if (action == "SELECT" && allowselect)
        {
            if (m_state == SHOWING_IMDBLIST)
                handleIMDBList();
            else if (m_state == SHOWING_IMDBMANUAL)
                handleIMDBManual();
            else
               slotEditMeta();
            return;
        }
        else if ((action == "0" || action == "1" || action == "2" ||
                 action == "3" || action == "4" || action == "5" ||
                 action == "6" || action == "7" || action == "8" ||
                 action == "9") && (m_state == SHOWING_IMDBMANUAL))
        {
            num(action);
            return;
        }
        else if (action == "DELETE")
            slotRemoveVideo();
        else if (action == "BROWSE" && (m_state == SHOWING_MAINWINDOW))
            slotToggleBrowseable();
        else if (action == "INCPARENT")
            doParental(1);
        else if (action == "DECPARENT")
            doParental(-1);
        else if (action == "UP")
            cursorUp();
        else if (action == "DOWN")
            cursorDown();
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "PAGEUP")
            pageUp();
        else if (action == "PAGEDOWN")
            pageDown();
        else if (action == "ESCAPE")
            exitWin();
        else if ((action == "INFO") || action == "MENU")
             videoMenu();
        else if (action == "FILTER" && (m_state == SHOWING_MAINWINDOW))
            slotDoFilter();
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void VideoManager::num(const QString &text)
{
    if (m_state == SHOWING_IMDBMANUAL)
    {
        curIMDBNum = curIMDBNum + text;
        update(imdbEnterRect);
    }
}

void VideoManager::RefreshMovieList(bool resort_only)
{
    if (updateML == true)
        return;
    updateML = true;
    if (resort_only)
    {
        m_video_list->resortList(true);
    }
    else
    {
        m_video_list->refreshList(false, 0, true);
        m_list_behave->setItemCount(m_video_list->count());
    }
    curitem = m_video_list->getVideoListMetadata(m_list_behave->getIndex());
    updateML = false;
}

void VideoManager::updateBackground()
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = m_theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

// Copy movie poster to appropriate directory and return full pathname to it
QString VideoManager::GetMoviePoster(const QString &movieNum)
{
    QString movieFile = curitem->Filename().section('.', 0, -2);
    QStringList images = QImage::inputFormatList();

    for (QStringList::iterator an_image_item = images.begin();
            an_image_item != images.end(); ++an_image_item)
    {
        QString an_image = *an_image_item;
        QString coverFile = movieFile + "." + an_image.lower();

        if (QFile::exists(coverFile))
        {
            return coverFile;
        }
    }

    if (movieNum == "Local")
        return(QString("<NULL>"));

    // Obtain movie poster
    QStringList args = QStringList::split(' ',
              gContext->GetSetting("MoviePosterCommandLine",
              gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -P"));
    args += movieNum;

    // execute external command to obtain url of movie poster
    QStringList lines = QStringList::split('\n',
                                    executeExternal(args, "Poster Query"));
    QString uri = "";
    for (QStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
        if ( (*it).at(0) == '#')  // treat lines beg w/ # as a comment
            continue;
        uri = *it;
        break;  // just take the first result
    }

    if (uri == "") {
       return "";
    }

    QString fileprefix = gContext->GetSetting("VideoArtworkDir");
    QDir dir;

    // If the video artwork setting hasn't been set default to
    // using ~/.mythtv/MythVideo
    if (fileprefix.length() == 0)
    {
        fileprefix = MythContext::GetConfDir();

        dir = QDir(fileprefix);
        if (!dir.exists())
            dir.mkdir(fileprefix);

        fileprefix += "/MythVideo";
    }

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    VERBOSE(VB_IMPORTANT,
            QString("Copying '%1' -> '%2'...").arg(uri).arg(fileprefix));
    QUrlOperator *op = new QUrlOperator();
    connect(op, SIGNAL(finished(QNetworkOperation*)),
          this, SLOT(copyFinished(QNetworkOperation*)) );
    iscopycomplete = false;
    iscopysuccess = false;

    op->copy(uri, "file:" + fileprefix);

    int nTimeout = gContext->GetNumSetting("PosterDownloadTimeout", 30) * 100;

    // wait for completion
    for (int i = 0; i < nTimeout; i++)
    {
        if (!iscopycomplete)
        {
            qApp->processEvents();
            usleep(10000);
        }
        else
        {
            break;
        }
    }

    QString localfile = "";
    if (iscopycomplete)
    {
        if (iscopysuccess)
        {
            localfile = fileprefix + "/" + uri.section('/', -1);
            QString extension = uri.right(uri.length()-uri.findRev('.'));
            if (dir.rename (localfile, fileprefix +"/" + movieNum + extension))
                localfile = fileprefix +"/" + movieNum + extension;
        }
    }
    else
    {
       op->stop();

       VERBOSE(VB_IMPORTANT, QString("Copying of '%1' timed out").arg(uri));

       MythPopupBox::showOkPopup(gContext->GetMainWindow(),
               QObject::tr("Could not retrieve poster"),
               QObject::tr("A movie poster exists for this movie but "
                           "Myth could not retrieve it within a reasonable "
                           "amount of time.\n"));
    }
    delete op;

    return localfile;
}

void VideoManager::copyFinished(QNetworkOperation* op)
{
    QString state, operation;
    switch(op->operation())
    {
        case QNetworkProtocol::OpMkDir:
            operation = "MkDir";
            break;
        case QNetworkProtocol::OpRemove:
            operation = "Remove";
            break;
        case QNetworkProtocol::OpRename:
            operation = "Rename";
            break;
        case QNetworkProtocol::OpGet:
            operation = "Get";
            break;
        case QNetworkProtocol::OpPut:
            operation = "Put";
            break;
        default:
            operation = "Uknown";
            break;
    }

    switch(op->state())
    {
        case QNetworkProtocol::StWaiting:
            state = "The operation is in the QNetworkProtocol's queue waiting "
                    "to be prcessed.";
            break;
        case QNetworkProtocol::StInProgress:
            state = "The operation is being processed.";
            break;
        case QNetworkProtocol::StDone:
            state = "The operation has been processed succesfully.";
            iscopycomplete = true;
            iscopysuccess = true;
            break;
        case QNetworkProtocol::StFailed:
            state = "The operation has been processed but an error occurred.";
            iscopycomplete = true;
            break;
        case QNetworkProtocol::StStopped:
            state = "The operation has been processed but has been stopped "
                    "before it finished, and is waiting to be processed.";
            break;
        default:
            state = "Unknown";
            break;
   }

   VERBOSE(VB_IMPORTANT, QString("%1: %2: %3")
           .arg(operation)
           .arg(state)
           .arg(op->protocolDetail()) );
}

void VideoManager::GetMovieData(const QString &movieNum)
{
    QStringList args = QStringList::split(' ',
              gContext->GetSetting("MovieDataCommandLine",
              gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -D"));
    args += movieNum;

    // execute external command to obtain list of possible movie matches
    QString results = executeExternal(args, "Movie Data Query");

    // parse results
    QMap<QString,QString> data;
    QStringList lines = QStringList::split('\n', results);
    if (lines.size() > 0)
    {
        for (QStringList::Iterator it = lines.begin(); it != lines.end(); ++it)
        {
            if ( (*it).at(0) == '#')  // treat lines beg w/ # as a comment
                continue;

            QString data_name = (*it).section(':', 0, 0);
            QString data_value = (*it).section(':', 1);
            data[data_name] = data_value;
        }

        // set known values
        curitem->setTitle(data["Title"]);
        curitem->setYear(data["Year"].toInt());
        curitem->setDirector(data["Director"]);
        curitem->setPlot(data["Plot"]);
        curitem->setUserRating(data["UserRating"].toFloat());
        curitem->setRating(data["MovieRating"]);
        curitem->setLength(data["Runtime"].toInt());

        //movieGenres
        Metadata::genre_list movie_genres;
        QStringList genres = QStringList::split(",", data["Genres"]);

        for (QStringList::iterator p = genres.begin(); p != genres.end(); ++p)
        {
            QString genre_name = (*p).stripWhiteSpace();
            if (genre_name.length())
            {
                movie_genres.push_back(
                        Metadata::genre_list::value_type(-1, genre_name));
            }
        }

        curitem->setGenres(movie_genres);

        // movieCountries
        Metadata::country_list movie_countries;
        QStringList countries = QStringList::split(",", data["Countries"]);
        for (QStringList::iterator p = countries.begin();
             p != countries.end(); ++p)
        {
            QString country_name = (*p).stripWhiteSpace();
            if (country_name.length())
            {
                movie_countries.push_back(
                        Metadata::country_list::value_type(-1, country_name));
            }
        }

        curitem->setCountries(movie_countries);

        curitem->setInetRef(movieNumber);
        QString movieCoverFile = GetMoviePoster(movieNumber);
        curitem->setCoverFile(movieCoverFile);
    }
    else
    {
        ResetCurrentItem();
    }

    curitem->updateDatabase();
    RefreshMovieList(true);
}

namespace
{
    // Execute an external command and return results in string
    // probably should make this routing async vs polling like this
    // but it would require a lot more code restructuring
    QString executeExternal(const QStringList &args, const QString &purpose)
    {
        QString ret = "";
        QString err = "";

        VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose).
                arg(args.join(" ")).local8Bit() );
        QProcess proc(args);

        QString cmd = args[0];
        QFileInfo info(cmd);

        if (!info.exists())
        {
            err = QString("\"%1\" failed: does not exist").arg(cmd.local8Bit());
        }
        else if (!info.isExecutable())
        {
            err = QString("\"%1\" failed: not executable").arg(cmd.local8Bit());
        }
        else if (proc.start())
        {
            while (true)
            {
                while (proc.canReadLineStdout() || proc.canReadLineStderr())
                {
                    if (proc.canReadLineStdout())
                    {
                        ret +=
                            QString::fromUtf8(proc.readLineStdout(), -1) + "\n";
                    }

                    if (proc.canReadLineStderr())
                    {
                        if (err == "")
                        {
                            err = cmd + ": ";
                        }

                        err +=
                            QString::fromUtf8(proc.readLineStderr(), -1) + "\n";
                    }
                }

                if (proc.isRunning())
                {
                    qApp->processEvents();
                    usleep(10000);
                }
                else
                {
                    if (!proc.normalExit())
                    {
                        err = QString("\"%1\" failed: Process exited "
                                "abnormally").arg(cmd.local8Bit());
                    }

                    break;
                }
            }
        }
        else
        {
            err = QString("\"%1\" failed: Could not start process")
                    .arg(cmd.local8Bit());
        }

        while (proc.canReadLineStdout() || proc.canReadLineStderr())
        {
            if (proc.canReadLineStdout())
            {
                ret += QString::fromUtf8(proc.readLineStdout(),-1) + "\n";
            }

            if (proc.canReadLineStderr())
            {
                if (err == "")
                {
                    err = cmd + ": ";
                }

                err += QString::fromUtf8(proc.readLineStderr(), -1) + "\n";
            }
        }

        if (err != "")
        {
            QString tempPurpose(purpose);

            if (tempPurpose == "")
                tempPurpose = "Command";

            VERBOSE(VB_IMPORTANT, err);
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                    QObject::tr(tempPurpose + " failed"),
                    QObject::tr(err + "\n\nCheck VideoManager Settings"));
            ret = "#ERROR";
        }

        VERBOSE(VB_IMPORTANT, ret);
        return ret;
    }
}

// Obtain a movie listing via popular website(s) and populates movieList
//   returns: number of possible matches returned, -1 for error
int VideoManager::GetMovieListing(const QString& movieName)
{
    QStringList args = QStringList::split(' ',
            gContext->GetSetting("MovieListCommandLine",
                    gContext->GetShareDir() +
                    "mythvideo/scripts/imdb.pl -M tv=no;video=no"));

    args += movieName;

    // execute external command to obtain list of possible movie matches
    QString results = executeExternal(args, "Movie Search");

    // parse results
    movieList.clear();

    int count = 0;
    QStringList lines = QStringList::split('\n', results);

    for (QStringList::Iterator it = lines.begin(); it != lines.end(); ++it)
    {
        if ((*it).at(0) == '#')  // treat lines beg w/ # as a comment
            continue;
        movieList.push_back(*it);
        count++;
    }

    // if only a single match, assume this is it
    if (count == 1)
    {
        movieNumber = movieList[0].section(':', 0, 0);
    }

    if (count > 0)
    {
        movieList.push_back(""); // visual separator - can't get selected
    }

    movieList.push_back("manual:Manually Enter IMDB #");
    movieList.push_back("reset:Reset Entry");
    movieList.push_back("cancel:Cancel");

    m_movie_list_behave->setItemCount(movieList.size());
    if (count > 0)
        m_movie_list_behave->setSkipIndex(movieList.size() - 4);
    else
        m_movie_list_behave->setSkipIndex();

    return count;
}

void VideoManager::grayOut(QPainter *tmp)
{
    tmp->fillRect(QRect(QPoint(0, 0), size()),
                  QBrush(QColor(10, 10, 10), Dense4Pattern));
}

void VideoManager::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (m_state == SHOWING_MAINWINDOW || m_state == SHOWING_IMDBMANUAL)
    {
       if (r.intersects(listRect) && noUpdate == false)
       {
           updateList(&p);
       }
       if (r.intersects(infoRect) && noUpdate == false)
       {
           updateInfo(&p);
       }
       if (r.intersects(imdbEnterRect) && m_state == SHOWING_IMDBMANUAL)
       {
           noUpdate = true;
           updateIMDBEnter(&p);
       }
    }

    if (m_state == SHOWING_IMDBLIST)
    {
       if (r.intersects(movieListRect))
       {
           updateMovieList(&p);
       }
    }
}

void VideoManager::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = m_theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            ltype->ResetList();
            ltype->SetActive(true);

            const ListBehaviorManager::lb_data &lbd = m_list_behave->getData();
            for (unsigned int i = lbd.begin; i < lbd.end; ++i)
            {
                Metadata *meta = m_video_list->getVideoListMetadata(i);

                QString title = meta->Title();
                QString filename = meta->Filename();
                if (0 == title.compare("title"))
                {
                    title = filename.section('/', -1);
                    if (!gContext->GetNumSetting("ShowFileExtensions"))
                        title = title.section('.',0,-2);
                }

                ltype->SetItemText(i - lbd.begin, 1, title);
                ltype->SetItemText(i - lbd.begin, 2, meta->Director());
                ltype->SetItemText(i - lbd.begin, 3,
                                   getDisplayYear(meta->Year()));

                if (i == lbd.item_index)
                {
                    curitem = meta;
                }
            }

            ltype->SetItemCurrent(lbd.index);

            ltype->SetDownArrow(lbd.data_below_window);
            ltype->SetUpArrow(lbd.data_above_window);
        }

        for (int i = 0; i < 9; ++i)
        {
            container->Draw(&tmp, i, 0);
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoManager::updateMovieList(QPainter *p)
{
    QRect pr = movieListRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    QString title = "";

    LayerSet *container = m_theme->GetSet("moviesel");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            ltype->ResetList();
            ltype->SetActive(true);

            const ListBehaviorManager::lb_data &lbd =
                    m_movie_list_behave->getData();
            for (unsigned int i = lbd.begin; i < lbd.end; ++i)
            {
               QString data = movieList[i].data();
               QString moviename = data.section(':', 1);
               ltype->SetItemText(i, 1, moviename);

                if (i == lbd.item_index)
                {
                    curitemMovie = moviename;
                }
            }

            ltype->SetItemCurrent(lbd.index);
            ltype->SetDownArrow(lbd.data_below_window);
            ltype->SetUpArrow(lbd.data_above_window);
        }

        for (int i = 0; i < 9; ++i)
        {
            container->Draw(&tmp, i, 0);
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoManager::updateIMDBEnter(QPainter *p)
{
    QRect pr = imdbEnterRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = m_theme->GetSet("enterimdb");
    if (container)
    {
        checkedSetText((UITextType *)container->GetType("numhold"), curIMDBNum);

        for (int i = 0; i < 9; ++i)
        {
            container->Draw(&tmp, i, 0);
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoManager::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (m_video_list->count() > 0 && curitem)
    {
       LayerSet *container = m_theme->GetSet("info");
       if (container)
       {
           checkedSetText((UITextType *)container->GetType("title"),
                          curitem->Title());
           checkedSetText((UITextType *)container->GetType("filename"),
                          curitem->getFilenameNoPrefix());
           checkedSetText((UITextType *)container->GetType("video_player"),
                          Metadata::getPlayer(curitem));
           checkedSetText((UITextType *)container->GetType("director"),
                          curitem->Director());
           checkedSetText((UITextType *)container->GetType("plot"),
                          curitem->Plot());
           checkedSetText((UITextType *)container->GetType("rating"),
                          curitem->Rating());
           checkedSetText((UITextType *)container->GetType("inetref"),
                          curitem->InetRef());
           checkedSetText((UITextType *)container->GetType("year"),
                          getDisplayYear(curitem->Year()));
           checkedSetText((UITextType *)container->GetType("userrating"),
                          getDisplayUserRating(curitem->UserRating()));
           checkedSetText((UITextType *)container->GetType("length"),
                          getDisplayLength(curitem->Length()));

           QString coverfile = curitem->CoverFile();
           coverfile.remove(artDir + "/");
           checkedSetText((UITextType *)container->GetType("coverfile"),
                          coverfile);

           checkedSetText((UITextType *)container->GetType("child_id"),
                          QString::number(curitem->ChildID()));
           checkedSetText((UITextType *)container->GetType("browseable"),
                          getDisplayBrowse(curitem->Browse()));
           checkedSetText((UITextType *)container->GetType("category"),
                          curitem->Category());
           checkedSetText((UITextType *)container->GetType("level"),
                          QString::number(curitem->ShowLevel()));

           for (int i = 1; i <= 8; ++i)
           {
               container->Draw(&tmp, i, 0);
           }
       }

       allowselect = true;
    }
    else
    {
       LayerSet *norec = m_theme->GetSet("novideos_info");
       if (norec)
       {
           for (int i = 4; i <= 8; ++i)
           {
               norec->Draw(&tmp, i, 0);
           }
       }

       allowselect = false;
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoManager::LoadWindow(QDomElement &element)
{
    for (QDomNode chld = element.firstChild(); !chld.isNull();
         chld = chld.nextSibling())
    {
        QDomElement e = chld.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                m_theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Unknown element: ").arg(e.tagName()));
                exit(0);
            }
        }
    }
}

void VideoManager::parseContainer(QDomElement &element)
{
    QRect area;
    QString container_name;
    int context;
    m_theme->parseContainer(element, container_name, context, area);

    container_name = container_name.lower();
    if (container_name == "selector")
        listRect = area;
    if (container_name == "info")
        infoRect = area;
    if (container_name == "moviesel")
        movieListRect = area;
    if (container_name == "enterimdb")
        imdbEnterRect = area;
}

void VideoManager::exitWin()
{
    if (m_state != SHOWING_MAINWINDOW)
    {
        m_state = SHOWING_MAINWINDOW;
        backup->begin(this);
        backup->drawPixmap(0, 0, myBackground);
        backup->end();
        update(fullRect);
        noUpdate = false;
    }
    else
        emit accept();
}

void VideoManager::doParental(int amount)
{
    if (!curitem)
        return;

    int curshowlevel = curitem->ShowLevel();

    curshowlevel += amount;

    if ( (curshowlevel > -1) && (curshowlevel < 5))
    {
        curitem->setShowLevel(curshowlevel);
        curitem->updateDatabase();
        RefreshMovieList(true);
        update(infoRect);
    }
}

void VideoManager::cursorLeft()
{
    if (expectingPopup)
        cancelPopup();
    else
        exitWin();
}

void VideoManager::cursorRight()
{
    videoMenu();
}

void VideoManager::cursorDown()
{
    switch (m_state)
    {
        case SHOWING_MAINWINDOW:
        {
            m_list_behave->move_down();
            break;
        }
        case SHOWING_IMDBLIST:
        {
            m_movie_list_behave->move_down();
            break;
        }
        // for warning
        case SHOWING_EDITWINDOW:
        case SHOWING_IMDBMANUAL:
        break;
    }

    update(fullRect);
}

void VideoManager::pageDown()
{
    switch (m_state)
    {
        case SHOWING_MAINWINDOW:
        {
            m_list_behave->page_down();
            break;
        }
        case SHOWING_IMDBLIST:
        {
            m_movie_list_behave->page_down();
            break;
        }
        // for warning
        case SHOWING_EDITWINDOW:
        case SHOWING_IMDBMANUAL:
        break;
    }

    update(fullRect);
}

void VideoManager::cursorUp()
{
    switch (m_state)
    {
        case SHOWING_MAINWINDOW:
        {
            m_list_behave->move_up();
            update(fullRect);
            break;
        }

        case SHOWING_IMDBLIST:
        {
            m_movie_list_behave->move_up();
            update(movieListRect);
            break;
        }
        // for warning
        case SHOWING_EDITWINDOW:
        case SHOWING_IMDBMANUAL:
        break;
    }
}

void VideoManager::pageUp()
{
    switch (m_state)
    {
        case SHOWING_MAINWINDOW:
        {
            m_list_behave->page_up();
            update(fullRect);
            break;
        }
        case SHOWING_IMDBLIST:
        {
            m_movie_list_behave->page_up();
            update(movieListRect);
            break;
        }
        // for warning
        case SHOWING_EDITWINDOW:
        case SHOWING_IMDBMANUAL:
        break;
    }
}

void VideoManager::videoMenu()
{
    if (!curitem)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), "video popup");

    expectingPopup = true;

    popup->addLabel(tr("Select action:"));
    popup->addLabel("");

    QButton *editButton = NULL;
    if (curitem)
    {
        editButton = popup->addButton(tr("Edit Metadata"), this,
                                      SLOT(slotEditMeta()));

        popup->addButton(tr("Search IMDB"), this, SLOT(slotAutoIMDB()));
        popup->addButton(tr("Manually Enter IMDB #"), this,
                         SLOT(slotManualIMDB()));
        popup->addButton(tr("Reset Metadata"), this, SLOT(slotResetMeta()));
        popup->addButton(tr("Toggle Browseable"), this,
                         SLOT(slotToggleBrowseable()));
        popup->addButton(tr("Remove Video"), this, SLOT(slotRemoveVideo()));
    }

    QButton *filterButton = popup->addButton(tr("Filter Display"), this,
                                             SLOT(slotDoFilter()));
    popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));

    popup->ShowPopup(this, SLOT(slotDoCancel()));

    if (editButton)
        editButton->setFocus();
    else
        filterButton->setFocus();
}

void VideoManager::doWaitBackground(QPainter &p, const QString &titleText)
{
    // set the title for the wait background
    LayerSet *container = m_theme->GetSet("inetwait");
    if (container)
    {
        checkedSetText((UITextType *)container->GetType("title"), titleText);

        for (int i = 0; i < 4; ++i)
            container->Draw(&p, i, 0);
    }
}

void VideoManager::ResetCurrentItem()
{
    curitem->setTitle(Metadata::FilenameToTitle(curitem->Filename()));
    curitem->setCoverFile(VIDEO_COVERFILE_DEFAULT);
    curitem->setYear(VIDEO_YEAR_DEFAULT);
    curitem->setInetRef(VIDEO_INETREF_DEFAULT);
    curitem->setDirector(VIDEO_DIRECTOR_DEFAULT);
    curitem->setPlot(VIDEO_PLOT_DEFAULT);
    curitem->setUserRating(0.0);
    curitem->setRating(VIDEO_RATING_DEFAULT);
    curitem->setLength(0);
    curitem->setShowLevel(1);
    curitem->setGenres(Metadata::genre_list());
    curitem->setCountries(Metadata::country_list());
    curitem->updateDatabase();

    RefreshMovieList(false);
}

void VideoManager::slotManualIMDB()
{
    cancelPopup();

    backup->begin(this);
    backup->drawPixmap(0, 0, myBackground);
    backup->end();
    curIMDBNum = "";
    m_state = SHOWING_IMDBMANUAL;
    update(fullRect);
    movieNumber = "";
}

void VideoManager::handleIMDBManual()
{
    QPainter p(this);
    movieNumber = curIMDBNum;

    backup->begin(this);
    grayOut(backup.get());
    doWaitBackground(p, curIMDBNum);
    backup->end();

    qApp->processEvents();

    GetMovieData(curIMDBNum);

    backup->begin(this);
    backup->drawPixmap(0, 0, myBackground);
    backup->end();
    qApp->processEvents(); // Without this we get drawing errors.

    m_state = SHOWING_MAINWINDOW;
    noUpdate = false;
    update(infoRect);
    update(listRect);
    update(fullRect);
}

void VideoManager::handleIMDBList()
{
    QPainter p(this);

    for (QStringList::Iterator it = movieList.begin();
         it != movieList.end(); ++it)
    {
        QString data = (*it).data();
        QString movie = data.section(':', 1);
        if (curitemMovie == movie)
        {
            movieNumber = data.section(':', 0, 0);
            break;
        }
    }

    if (movieNumber == "cancel")
    {
        QString movieCoverFile = GetMoviePoster(QString("Local"));
        if (movieCoverFile != "<NULL>")
        {
            curitem->setCoverFile(movieCoverFile);
            curitem->updateDatabase();
            RefreshMovieList(true);
        }

        backup->begin(this);
        backup->drawPixmap(0, 0, myBackground);
        backup->end();
        m_state = SHOWING_MAINWINDOW;
        update(fullRect);
        movieNumber = "";
        return;
    }
    else if (movieNumber == "manual")
         slotManualIMDB();
    else if (movieNumber == "reset")
         slotResetMeta();
    else if (movieNumber == "")
    {
        return;
    }
    else
    {
        if (movieNumber.isNull() || movieNumber.length() == 0)
        {
            ResetCurrentItem();
            backup->begin(this);
            backup->drawPixmap(0, 0, myBackground);
            backup->end();
            update(fullRect);
            return;
        }
        //cout << "GETTING MOVIE #" << movieNumber << endl;
        backup->begin(this);
        grayOut(backup.get());
        doWaitBackground(p, movieNumber);
        backup->end();
        qApp->processEvents();

        GetMovieData(movieNumber);

        backup->begin(this);
        backup->drawPixmap(0, 0, myBackground);
        backup->end();
        qApp->processEvents(); // Without this we get drawing errors.

        m_state = SHOWING_MAINWINDOW;
        update(infoRect);
        update(listRect);
        update(fullRect);
        movieNumber = "";
    }
}

void VideoManager::slotAutoIMDB()
{
    cancelPopup();

    // Do queries
    QPainter p(this);
    if (m_state == SHOWING_MAINWINDOW || m_state == SHOWING_EDITWINDOW)
    {
        m_state = SHOWING_EDITWINDOW;

        backup->flush();
        backup->begin(this);
        grayOut(backup.get());
        backup->end();

        // set the title for the wait background
        doWaitBackground(p, curitem->Title());
        backup->flush();

        int ret;

        if (curitem->InetRef() == VIDEO_INETREF_DEFAULT)
        {
            ret = GetMovieListing(curitem->Title());
        }
        else
        {
            movieNumber = curitem->InetRef();
            ret = 1;
        }

        VERBOSE(VB_IMPORTANT,
                QString("GetMovieList returned %1 possible matches").arg(ret));
        if (ret == 1)
        {
            if (movieNumber.isNull() || movieNumber.length() == 0)
            {
                ResetCurrentItem();
                backup->begin(this);
                backup->drawPixmap(0, 0, myBackground);
                backup->end();
                m_state = SHOWING_MAINWINDOW;
                update(fullRect);
                return;
            }
            //cout << "GETTING MOVIE #" << movieNumber << endl;
            GetMovieData(movieNumber);
            backup->begin(this);
            backup->drawPixmap(0, 0, myBackground);
            backup->end();
            m_state = SHOWING_MAINWINDOW;
            update(infoRect);
            update(listRect);
        }
        else if (ret >= 0)
        {
            m_movie_list_behave->setIndex(0);
            m_state = SHOWING_IMDBLIST;
            update(movieListRect);
        }
        else
        {
            //cout << "Error, movie not found.\n";
            backup->begin(this);
            backup->drawPixmap(0, 0, myBackground);
            backup->end();
            m_state = SHOWING_MAINWINDOW;
            update(infoRect);
            update(listRect);
        }
    }
}

void VideoManager::slotEditMeta()
{
    if (!curitem)
        return;

    EditMetadataDialog *md_editor = new EditMetadataDialog(
            curitem, m_video_list->getListCache(), gContext->GetMainWindow(),
            "edit_metadata", "video-", "edit metadata dialog");

    md_editor->exec();
    delete md_editor;
    cancelPopup();

    RefreshMovieList(false);

    update(infoRect);
}

void VideoManager::slotRemoveVideo()
{
    cancelPopup();

    if (curitem && m_state == SHOWING_MAINWINDOW)
    {
        bool okcancel;
        MythPopupBox *ConfirmationDialog =
                new MythPopupBox(gContext->GetMainWindow());
        okcancel = ConfirmationDialog->showOkCancelPopup(
                gContext->GetMainWindow(), "", tr("Delete this file?"), false);

        if (okcancel)
        {
            if (m_video_list->Delete(curitem->ID()))
                RefreshMovieList(false);
            else
                ConfirmationDialog->showOkPopup(gContext->GetMainWindow(), "",
                                                tr("delete failed"));
        }

        delete ConfirmationDialog;
    }
}

void VideoManager::slotDoCancel()
{
    if (!expectingPopup)
        return;

    cancelPopup();
}

void VideoManager::cancelPopup(void)
{
    expectingPopup = false;

    if (popup)
    {
        popup->hide();
        delete popup;

        popup = NULL;

        update(fullRect);
        qApp->processEvents();
        setActiveWindow();
    }
}

void VideoManager::slotResetMeta()
{
    cancelPopup();

    ResetCurrentItem();
    QString movieCoverFile = GetMoviePoster(QString("Local"));
    if (movieCoverFile != "<NULL>")
    {
        curitem->setCoverFile(movieCoverFile);
        curitem->updateDatabase();
        RefreshMovieList(true);
    }

    backup->begin(this);
    backup->drawPixmap(0, 0, myBackground);
    backup->end();

    m_state = SHOWING_MAINWINDOW;
    update(fullRect);
    movieNumber = "";
}

void VideoManager::slotDoFilter()
{
    cancelPopup();

    m_video_list->getFilterChangedState(); // clear any state sitting around
    BasicFilterSettingsProxy<VideoList> sp(*m_video_list);
    VideoFilterDialog *vfd =
            new VideoFilterDialog(&sp, gContext->GetMainWindow(),
                                  "filter", "video-", *m_video_list,
                                  "Video Filter Dialog");
    vfd->exec();
    delete vfd;

    unsigned int filter_state = m_video_list->getFilterChangedState();
    if (filter_state & VideoFilterSettings::FILTER_MASK)
    {
        RefreshMovieList(false);
    }
    else if (filter_state & VideoFilterSettings::kSortOrderChanged)
    {
        RefreshMovieList(true);
    }
    update(fullRect);
}

void VideoManager::slotToggleBrowseable()
{
    if (!curitem)
        return;

    cancelPopup();

    if (curitem)
    {
        curitem->setBrowse(!curitem->Browse());
        curitem->updateDatabase();
    }

    RefreshMovieList(false);
    update(infoRect);
}
