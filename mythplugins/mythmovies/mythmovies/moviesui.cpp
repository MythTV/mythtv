// POSIX headers
#include <unistd.h>

// C headers
#include <cstdlib>

// Qt headers
#include <QApplication>
#include <QKeyEvent>
#include <QProcess>
#include <QFileInfo>
#include <QDomDocument>

// MythTV headers
#include <mythcontext.h>
#include <compat.h>
#include <mythuitext.h>
#include <mythuibuttontree.h>
#include <mythgenerictree.h>
#include <mythdialogbox.h>
#include <mythmainwindow.h>
#include <mythdb.h>

// MythMovies headers
#include "moviesui.h"

namespace
{
    struct sAscendingMovieOrder
    {
        bool operator()(const Movie& start,const Movie& end)
        {
            return start.name < end.name;
        }
    };

    bool HandleError(const QString &err, const QString &purpose)
    {
        if (err.isEmpty())
            return false;

        QString tempPurpose = purpose.isEmpty() ? QObject::tr("Command") : purpose;

        VERBOSE(VB_IMPORTANT, err);
        ShowOkPopup(QObject::tr("%1 failed\n%2\n\nCheck MythMovies Settings").arg(tempPurpose).arg(err));

        return true;
    }

    //Taken from MythVideo
    // Execute an external command and return results in string
    // probably should make this routing async vs polling like this
    // but it would require a lot more code restructuring
    QString ExecuteExternal(const QString     &program,
                            const QStringList &args,
                            const QString     &purpose)
    {
        QString ret = "";
        QString err = "";

        VERBOSE(VB_GENERAL, QString("%1: Executing '%2 %3'")
                .arg(purpose).arg(program).arg(args.join(" ")));

        QProcess proc;

        QFileInfo info(program);

        if (!info.exists())
        {
            err = QObject::tr("\"%1\" failed: does not exist").arg(program);
            HandleError(err, purpose);
            return "#ERROR";
        }
        else if (!info.isExecutable())
        {
            err = QObject::tr("\"%1\" failed: not executable").arg(program);
            HandleError(err, purpose);
            return "#ERROR";
        }

        proc.start(program, args);

        if (!proc.waitForStarted())
        {
            err = QObject::tr("\"%1\" failed: Could not start process")
                .arg(program);
            HandleError(err, purpose);
            return "#ERROR";
        }


        while (true)
        {
            proc.setReadChannel(QProcess::StandardError);
            while (proc.canReadLine())
            {
                if (err.isEmpty())
                    err = program + ": ";

                err += QString::fromUtf8(proc.readLine(), -1) + "\n";
            }

            proc.setReadChannel(QProcess::StandardOutput);
            while (proc.canReadLine())
                ret += QString::fromUtf8(proc.readLine(), -1) + "\n";

            if (QProcess::Running == proc.state())
            {
                qApp->processEvents();
                usleep(10000);
            }
            else
            {
                if (proc.exitCode())
                {
                    err = QObject::tr("\"%1\" failed: Process exited "
                                      "abnormally").arg(program);
                }

                break;
            }
        }

        ret += QString::fromUtf8(proc.readAllStandardOutput(),-1);
        QString tmp = QString::fromUtf8(proc.readAllStandardError(),-1);
        if (!tmp.isEmpty())
        {
            if (err.isEmpty())
                err =  program + ": ";
            err += tmp;
        }

        if (HandleError(err, purpose))
            return "#ERROR";

        return ret;
    }
}

MoviesUI::MoviesUI(MythScreenStack *parent)
        : MythScreenType(parent, "MoviesUI")
{
    m_currentMode = "Undefined";
}

MoviesUI::~MoviesUI()
{
}

bool MoviesUI::Create()
{
    if (!LoadWindowFromXML("movies-ui.xml", "moviesui", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_movieTreeUI, "movietreelist", &err);
    UIUtilE::Assign(this, m_movieTitle, "movietitle", &err);
    UIUtilE::Assign(this, m_movieRating, "ratingvalue", &err);
    UIUtilE::Assign(this, m_movieRunningTime, "runningtimevalue", &err);
    UIUtilE::Assign(this, m_movieShowTimes, "showtimesvalue", &err);
    UIUtilE::Assign(this, m_theaterName, "theatername", &err);


    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'moviesui'");
        return false;
    }

    m_currentNode = NULL;

    connect(m_movieTreeUI, SIGNAL(nodeChanged(MythGenericTree*)),
            this, SLOT(nodeChanged(MythGenericTree*)));

    BuildFocusList();
    LoadInBackground();

    return true;
}

void MoviesUI::Load()
{
    gCoreContext->ActivateSettingsCache(false);
    QString currentDate = QDate::currentDate().toString();
    QString lastDate = gCoreContext->GetSetting("MythMovies.LastGrabDate");
    if (currentDate != lastDate)
    {
        VERBOSE(VB_IMPORTANT, "Movie Data Has Expired. Refreshing.");
        updateMovieTimes();
    }

    gCoreContext->ActivateSettingsCache(true);

    updateDataTrees();
}

void MoviesUI::Init()
{
    drawDisplayTree();
}

void MoviesUI::updateMovieTimes()
{
    gCoreContext->ActivateSettingsCache(false);

    QString currentDate = QDate::currentDate().toString();

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec("truncate table movies_showtimes"))
        MythDB::DBError("truncating movies_showtimes", query);
    if (!query.exec("truncate table movies_movies"))
        MythDB::DBError("truncating movies_movies", query);
    if (!query.exec("truncate table movies_theaters"))
        MythDB::DBError("truncating movies_theaters", query);

    QString grabber = gCoreContext->GetSetting("MythMovies.Grabber");
    grabber.replace("%z", gCoreContext->GetSetting("MythMovies.ZipCode"));
    grabber.replace("%r", gCoreContext->GetSetting("MythMovies.Radius"));
    QStringList args = grabber.split(' ');
    QString ret = "#ERROR";
    if (args.size())
    {
        QString program = args[0];
        args.pop_front();
        ret = ExecuteExternal(program, args, QObject::tr("MythMovies Data Grabber"));
    }

    VERBOSE(VB_IMPORTANT, "Grabber Finished. Processing Data.");
    if (populateDatabaseFromGrabber(ret))
        gCoreContext->SaveSetting("MythMovies.LastGrabDate", currentDate);
    else
    {
        ShowOkPopup(QObject::tr("Failed to process the grabber data!"));
        VERBOSE(VB_IMPORTANT, "Failed to process the grabber data!");
    }

    gCoreContext->ActivateSettingsCache(true);
}

MovieVector MoviesUI::buildMovieDataTree()
{
    MovieVector ret;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery subQuery(MSqlQuery::InitCon());

    if (query.exec("select id, moviename, rating, runningtime from movies_movies order by moviename asc"))
    {
        while (query.next())
        {
            Movie m;
            m.name = query.value(1).toString();
            m.rating = query.value(2).toString();
            m.runningTime = query.value(3).toString();
            subQuery.prepare("select theatername, theateraddress, showtimes "
                    "from movies_showtimes left join movies_theaters "
                    "on movies_showtimes.theaterid = movies_theaters.id "
                    "where movies_showtimes.movieid = :MOVIEID");
            subQuery.bindValue(":MOVIEID", query.value(0).toString());

            if (subQuery.exec())
            {
                while (subQuery.next())
                {
                    Theater t;
                    t.name = subQuery.value(0).toString();
                    t.address = subQuery.value(1).toString();
                    t.showTimes = subQuery.value(2).toString();
                    m.theaters.push_back(t);
                }
            }
            ret.push_back(m);
        }
    }
    return ret;
}

TheaterVector MoviesUI::buildTheaterDataTree()
{
    TheaterVector ret;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery subQuery(MSqlQuery::InitCon());

    if (query.exec("select id, theatername, theateraddress from movies_theaters order by theatername asc"))
    {
        while (query.next())
        {
            Theater t;
            t.name = query.value(1).toString();
            t.address = query.value(2).toString();
            subQuery.prepare("select moviename, rating, runningtime, showtimes "
                    "from movies_showtimes left join movies_movies "
                    "on movies_showtimes.movieid = movies_movies.id "
                    "where movies_showtimes.theaterid = :THEATERID");
            subQuery.bindValue(":THEATERID", query.value(0).toString());

            if (subQuery.exec())
            {
                while (subQuery.next())
                {
                    Movie m;
                    m.name = subQuery.value(0).toString();
                    m.rating = subQuery.value(1).toString();
                    m.runningTime = subQuery.value(2).toString();
                    m.showTimes = subQuery.value(3).toString();
                    t.movies.push_back(m);
                }
            }

            ret.push_back(t);
        }
    }
    return ret;
}

bool MoviesUI::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Movies", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MoviesUI::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(QObject::tr("Update Movie Times"), SLOT(slotUpdateMovieTimes()));
    menuPopup->AddButton(QObject::tr("Cancel"));
}

void MoviesUI::slotUpdateMovieTimes()
{
    VERBOSE(VB_IMPORTANT, "Doing Manual Movie Times Update");

    updateMovieTimes();
    updateDataTrees();
    drawDisplayTree();
}

void MoviesUI::nodeChanged(MythGenericTree* node)
{
    m_currentNode = node;
    int nodeInt = node->getInt();

    if (nodeInt == 0)
    {
        m_currentMode = m_currentNode->getString();
        m_theaterName->SetText("");
        m_movieTitle->SetText("");
        m_movieRunningTime->SetText("");
    }
    else
    {
        if (m_currentMode == QObject::tr("By Theater"))
        {
            if (nodeInt < 0)
            {
                int theaterInt = -nodeInt;
                m_currentTheater = m_dataTreeByTheater.at(theaterInt - 1);
                m_theaterName->SetText(m_currentTheater.name + " - " +
                                       m_currentTheater.address);
                m_movieTitle->SetText("");
                m_movieRating->SetText("");
                m_movieShowTimes->SetText("");
                m_movieRunningTime->SetText("");
            }
            else
            {
                int theaterInt = nodeInt / 100;
                int movieInt = nodeInt - (theaterInt * 100);
                Theater t = m_dataTreeByTheater.at(theaterInt - 1);
                Movie m = t.movies.at(movieInt - 1);
                m_movieTitle->SetText(m.name);
                m_movieRating->SetText(m.rating);
                m_movieRunningTime->SetText(m.runningTime);
                QStringList st = m.showTimes.split("|");
                QString buf;
                int i = 0;
                for (QStringList::Iterator it = st.begin(); it != st.end();
                     ++it)
                {
                    buf += (*it).trimmed() + " ";
                    i++;
                }
                m_movieShowTimes->SetText(buf);
            }
        }
        else if (m_currentMode == QObject::tr("By Movie"))
        {
            if (nodeInt < 0)
            {
                int movieInt = -nodeInt;
                m_currentMovie = m_dataTreeByMovie.at(movieInt - 1);
                m_movieTitle->SetText(m_currentMovie.name);
                m_movieRating->SetText(m_currentMovie.rating);
                m_movieRunningTime->SetText(m_currentMovie.runningTime);
                m_movieShowTimes->SetText("");
                m_theaterName->SetText("");
            }
            else
            {
                int movieInt =  nodeInt / 100;
                int theaterInt = nodeInt - (movieInt * 100);
                Movie m = m_dataTreeByMovie.at(movieInt - 1);
                Theater t = m.theaters.at(theaterInt - 1);
                QStringList st = t.showTimes.split("|");
                QString buf;
                int i = 0;
                for (QStringList::Iterator it = st.begin(); it != st.end();
                     ++it)
                {
                    if (i % 4 == 0 && i != 0)
                        buf+= "\n";
                    buf += (*it).trimmed() + " ";
                    i++;
                }
                m_movieShowTimes->SetText(buf);
                m_theaterName->SetText(t.name + " - " + t.address);
            }
        }
        else
        {
            //cerr << "Entry was called with an undefined mode." << endl << flush;
        }
    }
}

MythGenericTree* MoviesUI::getDisplayTreeByTheater()
{
    TheaterVector *theaters;
    theaters = &m_dataTreeByTheater;
    int tbase = 0;
    MythGenericTree *parent = new MythGenericTree(QObject::tr("By Theater"), 0, false);
    for (int i = 0; i < theaters->size(); i++)
    {
        int mbase = 0;
        Theater x = theaters->at(i);
        MythGenericTree *node = new MythGenericTree(x.name, --tbase, false);
        for (int m =0; m < x.movies.size(); m++)
        {
            Movie y = x.movies.at(m);
            node->addNode(y.name, (tbase * -100) + ++mbase, true);
        }
        parent->addNode(node);
    }
    return parent;
}

MythGenericTree* MoviesUI::getDisplayTreeByMovie()
{
    MovieVector *movies;
    movies = &m_dataTreeByMovie;
    int mbase = 0;
    MythGenericTree *parent = new MythGenericTree(QObject::tr("By Movie"), 0, false);
    for (int i = 0; i < movies->size(); i++)
    {
        int tbase = 0;
        Movie x = movies->at(i);
        MythGenericTree *node = new MythGenericTree(x.name, --mbase, false);
        for (int m = 0; m < x.theaters.size(); m++)
        {
            Theater y = x.theaters.at(m);
            node->addNode(y.name, (mbase * -100) + ++tbase, true);
        }
        parent->addNode(node);
    }
    return parent;
}

void MoviesUI::updateDataTrees()
{
    m_dataTreeByTheater = buildTheaterDataTree();
    m_dataTreeByMovie = buildMovieDataTree();
}

void MoviesUI::drawDisplayTree()
{
    m_movieTree = new MythGenericTree(QObject::tr("Theaters"), 0, false);
    m_movieTree->addNode(getDisplayTreeByTheater());
    m_movieTree->addNode(getDisplayTreeByMovie());
    m_movieTreeUI->AssignTree(m_movieTree);
    m_currentMode = m_movieTreeUI->GetItemCurrent()->GetText();
}

bool MoviesUI::populateDatabaseFromGrabber(QString ret)
{
     //stores error returns
    QString error;
    int errorLine;
    int errorColumn;
    QDomDocument doc;
    QDomNode n;
    if (!doc.setContent(ret, false, &error, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("Error parsing data from grabber: "
                "Error: %1 Location Line: %2 Column %3")
                .arg(error) .arg(errorLine) .arg(errorColumn));
        return false;
    }
    QDomElement root = doc.documentElement();
    n = root.firstChild();
    //loop through each theater
    while (!n.isNull())
    {
        processTheatre(n);
        n = n.nextSibling();
    }

    return true;
}

void MoviesUI::processTheatre(QDomNode &n)
{
    Theater t;
    QDomNode movieNode;
    const QDomElement theater = n.toElement();
    QDomNode child = theater.firstChild();
    MSqlQuery query(MSqlQuery::InitCon());

    while (!child.isNull())
    {
        if (!child.isNull())
        {
            if (child.toElement().tagName() == "Name")
            {
                t.name = child.firstChild().toText().data();
                if (t.name.isNull())
                    t.name = "";
            }

            if (child.toElement().tagName() == "Address")
            {
                t.address = child.firstChild().toText().data();
                if (t.address.isNull())
                    t.address = "";
            }
            if (child.toElement().tagName() == "Movies")
            {
                query.prepare("INSERT INTO movies_theaters "
                        "(theatername, theateraddress)" 
                        "values (:NAME,:ADDRESS)");

                query.bindValue(":NAME", t.name);
                query.bindValue(":ADDRESS", t.address);
                if (!query.exec())
                {
                    VERBOSE(VB_IMPORTANT, "Failure to Insert Theater");
                }
                int lastid = query.lastInsertId().toInt();
                movieNode = child.firstChild();
                while (!movieNode.isNull())
                {
                    processMovie(movieNode, lastid);
                    movieNode = movieNode.nextSibling();
                }
            }

            child = child.nextSibling();
        }
    }
}

void MoviesUI::processMovie(QDomNode &n, int theaterId)
{
    Movie m;
    QDomNode mi = n.firstChild();
    int movieId = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    while (!mi.isNull())
    {
        if (mi.toElement().tagName() == "Name")
        {
            m.name = mi.firstChild().toText().data();
            if (m.name.isNull())
                m.name = "";
        }
        if (mi.toElement().tagName() == "Rating")
        {
            m.rating = mi.firstChild().toText().data();
            if (m.rating.isNull())
                m.rating = "";
        }
        if (mi.toElement().tagName() == "ShowTimes")
        {
            m.showTimes = mi.firstChild().toText().data();
            if (m.showTimes.isNull())
                m.showTimes = "";
        }
        if (mi.toElement().tagName() == "RunningTime")
        {
            m.runningTime = mi.firstChild().toText().data();
            if (m.runningTime.isNull())
                m.runningTime = "";
        }
        mi = mi.nextSibling();
    }

    query.prepare("SELECT id FROM movies_movies Where moviename = :NAME");
    query.bindValue(":NAME", m.name);
    if (query.exec() && query.next())
    {
        movieId = query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO movies_movies ("
                "moviename, rating, runningtime) values ("
                ":NAME, :RATING, :RUNNINGTIME)");
        query.bindValue(":NAME", m.name);
        query.bindValue(":RATING", m.rating);
        query.bindValue(":RUNNINGTIME", m.runningTime);
        if (query.exec())
        {
            movieId = query.lastInsertId().toInt();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Failure to Insert Movie");
        }
    }
    query.prepare("INSERT INTO movies_showtimes ("
            "theaterid, movieid, showtimes) values ("
            ":THEATERID, :MOVIEID, :SHOWTIMES)");
    query.bindValue(":THEATERID", theaterId);
    query.bindValue(":MOVIEID", movieId);
    query.bindValue(":SHOWTIMES", m.showTimes);

    if (!query.exec())
    {
        VERBOSE(VB_IMPORTANT, "Failure to Link Movie to Theater");
    }
}
