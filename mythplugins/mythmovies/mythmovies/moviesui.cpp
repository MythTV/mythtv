// POSIX headers
#include <unistd.h>

// C headers
#include <cstdlib>

// Qt headers
#include <QApplication>
#include <QKeyEvent>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>

// MythTV headers
#include <mythcontext.h>
#include <uitypes.h>
#include <compat.h>

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

        QString tempPurpose = purpose.isEmpty() ? "Command" : purpose;

        VERBOSE(VB_IMPORTANT, err);
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(),
            QString(tempPurpose + " failed"),
            QString(err + "\n\nCheck MythMovies Settings"));

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
            err = QString("\"%1\" failed: does not exist").arg(program);
            HandleError(err, purpose);
            return "#ERROR";
        }
        else if (!info.isExecutable())
        {
            err = QString("\"%1\" failed: not executable").arg(program);
            HandleError(err, purpose);
            return "#ERROR";
        }

        proc.start(program, args);

        if (!proc.waitForStarted())
        {
            err = QString("\"%1\" failed: Could not start process")
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
                    err = QString("\"%1\" failed: Process exited "
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

MoviesUI::MoviesUI(MythMainWindow *parent,
                   const QString  &windowName,
                   const QString  &themeFilename,
                   const char     *name)
    : MythThemedDialog(parent, windowName, themeFilename, name)
{
    query = new MSqlQuery(MSqlQuery::InitCon());
    subQuery = new MSqlQuery(MSqlQuery::InitCon());
    aboutPopup = NULL;
    menuPopup = NULL;
    //m_movieTree = new GenericTree("Theaters", 0, false);
    m_currentMode = "Undefined";
    setupTheme();
}

MoviesUI::~MoviesUI()
{
    delete query;
    delete subQuery;
}

void MoviesUI::setupTheme(void)
{
    m_movieTreeUI = getUIManagedTreeListType("movietreelist");
    m_currentNode = NULL;
    m_movieTreeUI->showWholeTree(true);
    m_movieTreeUI->colorSelectables(true);

    connect(m_movieTreeUI, SIGNAL(nodeSelected(int, IntVector*)),
            this, SLOT(handleTreeListSelection(int, IntVector*)));
    connect(m_movieTreeUI, SIGNAL(nodeEntered(int, IntVector*)),
            this, SLOT(handleTreeListEntry(int, IntVector*)));


    m_movieTitle = getUITextType("movietitle");
    if (!m_movieTitle)
        VERBOSE(VB_IMPORTANT, "moviesui.o: Couldn't find text area movietitle");

    m_movieRating = getUITextType("ratingvalue");
    if (!m_movieRating)
        VERBOSE(VB_IMPORTANT,
                "moviesui.o: Couldn't find text area ratingvalue");

    m_movieRunningTime = getUITextType("runningtimevalue");
    if (!m_movieRunningTime)
        VERBOSE(VB_IMPORTANT,
                "moviesui.o: Couldn't find text area runningtimevalue");

    m_movieShowTimes = getUITextType("showtimesvalue");
    if (!m_movieShowTimes)
        VERBOSE(VB_IMPORTANT,
                "moviesui.o: Couldn't find text area showtimesvalue");

    m_theaterName = getUITextType("theatername");
    if (!m_theaterName)
        VERBOSE(VB_IMPORTANT,
                "moviesui.o: Couldn't find text area theatername");
    gContext->ActivateSettingsCache(false);
    QString currentDate = QDate::currentDate().toString();
    QString lastDate = gContext->GetSetting("MythMovies.LastGrabDate");
    if (currentDate != lastDate)
    {
        VERBOSE(VB_IMPORTANT, "Movie Data Has Expired. Refreshing.");
        updateMovieTimes();
    }

    gContext->ActivateSettingsCache(true);

    updateDataTrees();
    drawDisplayTree();
    updateForeground();
}

void MoviesUI::updateMovieTimes()
{
    gContext->ActivateSettingsCache(false);
    QString currentDate = QDate::currentDate().toString();
    query->exec("truncate table movies_showtimes");
    query->exec("truncate table movies_movies");
    query->exec("truncate table movies_theaters");
    QString grabber = gContext->GetSetting("MythMovies.Grabber");
    grabber.replace("%z", gContext->GetSetting("MythMovies.ZipCode"));
    grabber.replace("%r", gContext->GetSetting("MythMovies.Radius"));
    QStringList args = grabber.split(' ');
    QString ret = "#ERROR";
    if (args.size())
    {
        QString program = args[0];
        args.pop_front();
        ret = ExecuteExternal(program, args, "MythMovies Data Grabber");
    }

    VERBOSE(VB_IMPORTANT, "Grabber Finished. Processing Data.");
    if (populateDatabaseFromGrabber(ret))
        gContext->SaveSetting("MythMovies.LastGrabDate", currentDate);
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                "Error", tr("Failed to process the grabber data!"));
        VERBOSE(VB_IMPORTANT, "Failed to process the grabber data!");
    }

    gContext->ActivateSettingsCache(true);
}

MovieVector MoviesUI::buildMovieDataTree()
{
    MovieVector ret;
    if (query->exec("select id, moviename, rating, runningtime from movies_movies order by moviename asc"))
    {
        while (query->next())
        {
            Movie m;
            m.name = query->value(1).toString();
            m.rating = query->value(2).toString();
            m.runningTime = query->value(3).toString();
            subQuery->prepare("select theatername, theateraddress, showtimes "
                    "from movies_showtimes left join movies_theaters "
                    "on movies_showtimes.theaterid = movies_theaters.id "
                    "where movies_showtimes.movieid = :MOVIEID");
            subQuery->bindValue(":MOVIEID", query->value(0).toString());

            if (subQuery->exec())
            {
                while (subQuery->next())
                {
                    Theater t;
                    t.name = subQuery->value(0).toString();
                    t.address = subQuery->value(1).toString();
                    t.showTimes = subQuery->value(2).toString();
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
    if (query->exec("select id, theatername, theateraddress from movies_theaters order by theatername asc"))
    {
        while (query->next())
        {
            Theater t;
            t.name = query->value(1).toString();
            t.address = query->value(2).toString();
            subQuery->prepare("select moviename, rating, runningtime, showtimes "
                    "from movies_showtimes left join movies_movies "
                    "on movies_showtimes.movieid = movies_movies.id "
                    "where movies_showtimes.theaterid = :THEATERID");
            subQuery->bindValue(":THEATERID", query->value(0).toString());

            if (subQuery->exec())
            {
                while (subQuery->next())
                {
                    Movie m;
                    m.name = subQuery->value(0).toString();
                    m.rating = subQuery->value(1).toString();
                    m.runningTime = subQuery->value(2).toString();
                    m.showTimes = subQuery->value(3).toString();
                    t.movies.push_back(m);
                }
            }

            ret.push_back(t);
        }
    }
    return ret;
}

void MoviesUI::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Movies", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        //cout << "Action: " << action << endl << flush;
        handled = true;
        if (action == "SELECT")
            m_movieTreeUI->select();
        else if (action == "MENU")
            showMenu();
        else if (action == "INFO")
            //todo: redirect info to an info screen via imdb.pl
            showAbout();
        else if (action == "UP")
            m_movieTreeUI->moveUp();
        else if (action == "DOWN")
            m_movieTreeUI->moveDown();
        else if (action == "LEFT")
            m_movieTreeUI->popUp();
        else if (action == "RIGHT")
            m_movieTreeUI->pushDown();
        else if (action == "PAGEUP")
            m_movieTreeUI->pageUp();
        else if (action == "PAGEDOWN")
            m_movieTreeUI->pageDown();
        else if (action == "INCSEARCH")
            m_movieTreeUI->incSearchStart();
        else if (action == "INCSEARCHNEXT")
            m_movieTreeUI->incSearchNext();
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MoviesUI::showMenu()
{
    if (menuPopup)
        return;
    menuPopup = new MythPopupBox(gContext->GetMainWindow(), "menuPopup");
    menuPopup->addLabel("MythMovies Menu");
    updateButton = menuPopup->addButton("Update Movie Times", this, SLOT(slotUpdateMovieTimes()));
    OKButton = menuPopup->addButton("Close Menu", this, SLOT(closeMenu()));
    OKButton->setFocus();
    menuPopup->ShowPopup(this, SLOT(closeMenu()));
}

void MoviesUI::slotUpdateMovieTimes()
{
    VERBOSE(VB_IMPORTANT, "Doing Manual Movie Times Update");

    menuPopup->hide();
    menuPopup->deleteLater();
    menuPopup = NULL;

    updateMovieTimes();
    updateDataTrees();
    drawDisplayTree();
}

void MoviesUI::closeMenu(void)
{
    if (menuPopup)
    {
        menuPopup->deleteLater();
        menuPopup = NULL;
    }
}

void MoviesUI::showAbout()
{
    if (aboutPopup)
        return;

    aboutPopup = new MythPopupBox(gContext->GetMainWindow(), "aboutPopup");
    aboutPopup->addLabel("MythMovies");
    aboutPopup->addLabel("Copyright (c) 2006 Josh Lefler.");
    aboutPopup->addLabel("Released under GNU GPL v2");
    aboutPopup->addLabel("Special Thanks to Ignyte.com for\nproviding the "
                         "listings data.\n and the #mythtv IRC channel for "
                         "assistance.");
    OKButton = aboutPopup->addButton(QString("Close"), this,
                                     SLOT(closeAboutPopup()));
    OKButton->setFocus();
    aboutPopup->ShowPopup(this,SLOT(closeAboutPopup()));
}

void MoviesUI::closeAboutPopup(void)
{
    if (aboutPopup)
    {
        aboutPopup->deleteLater();
        aboutPopup = NULL;
    }
}

void MoviesUI::handleTreeListEntry(int nodeInt, IntVector *)
{
    m_currentNode = m_movieTreeUI->getCurrentNode();
    if (nodeInt == 0)
    {
        m_currentMode = m_currentNode->getString();
        m_theaterName->SetText("");
        m_movieTitle->SetText("");
        m_movieRunningTime->SetText("");
    }
    else
    {
        if (m_currentMode == "By Theater")
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
        else if (m_currentMode == "By Movie")
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

void MoviesUI::handleTreeListSelection(int nodeInt, IntVector *)
{
    (void) nodeInt;
    //perhaps the same as info?
    //VERBOSE(VB_IMPORTANT, QString("In Selection with %1").arg(nodeInt));
}

GenericTree* MoviesUI::getDisplayTreeByTheater()
{
    TheaterVector *theaters;
    theaters = &m_dataTreeByTheater;
    int tbase = 0;
    GenericTree *parent = new GenericTree("By Theater", 0, false);
    for (int i = 0; i < theaters->size(); i++)
    {
        int mbase = 0;
        Theater x = theaters->at(i);
        GenericTree *node = new GenericTree(x.name, --tbase, false);
        for (int m =0; m < x.movies.size(); m++)
        {
            Movie y = x.movies.at(m);
            node->addNode(y.name, (tbase * -100) + ++mbase, true);
        }
        parent->addNode(node);
    }
    return parent;
}

GenericTree* MoviesUI::getDisplayTreeByMovie()
{
    MovieVector *movies;
    movies = &m_dataTreeByMovie;
    int mbase = 0;
    GenericTree *parent = new GenericTree("By Movie", 0, false);
    for (int i = 0; i < movies->size(); i++)
    {
        int tbase = 0;
        Movie x = movies->at(i);
        GenericTree *node = new GenericTree(x.name, --mbase, false);
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
    m_movieTree = new GenericTree("Theaters", 0, false);
    m_movieTree->addNode(getDisplayTreeByTheater());
    m_movieTree->addNode(getDisplayTreeByMovie());
    m_movieTreeUI->assignTreeData(m_movieTree);
    m_movieTreeUI->popUp();
    m_movieTreeUI->popUp();
    m_movieTreeUI->popUp();
    m_movieTreeUI->enter();
    m_currentMode = m_movieTreeUI->getCurrentNode()->getString();
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
        //list.push_back(t);
        n = n.nextSibling();
    }

    return true;
}

void MoviesUI::processTheatre(QDomNode &n)
{
    Theater t;
    //Movie m;
    QDomNode movieNode;
    const QDomElement theater = n.toElement();
    QDomNode child = theater.firstChild();
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
                query->prepare("INSERT INTO movies_theaters "
                        "(theatername, theateraddress)" 
                        "values (:NAME,:ADDRESS)");

                query->bindValue(":NAME", t.name);
                query->bindValue(":ADDRESS", t.address);
                if (!query->exec())
                {
                    VERBOSE(VB_IMPORTANT, "Failure to Insert Theater");
                }
                int lastid = query->lastInsertId().toInt();
                movieNode = child.firstChild();
                while (!movieNode.isNull())
                {
                    processMovie(movieNode, lastid);
                    //t.movies.push_back(m);
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
    
    query->prepare("SELECT id FROM movies_movies Where moviename = :NAME");
    query->bindValue(":NAME", m.name);
    if (query->exec() && query->next())
    {
        movieId = query->value(0).toInt();
    }
    else
    {
        query->prepare("INSERT INTO movies_movies ("
                "moviename, rating, runningtime) values ("
                ":NAME, :RATING, :RUNNINGTIME)");
        query->bindValue(":NAME", m.name);
        query->bindValue(":RATING", m.rating);
        query->bindValue(":RUNNINGTIME", m.runningTime);
        if (query->exec())
        {
            movieId = query->lastInsertId().toInt();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Failure to Insert Movie");
        }
    }
    query->prepare("INSERT INTO movies_showtimes ("
            "theaterid, movieid, showtimes) values ("
            ":THEATERID, :MOVIEID, :SHOWTIMES)");
    query->bindValue(":THEATERID", theaterId);
    query->bindValue(":MOVIEID", movieId);
    query->bindValue(":SHOWTIMES", m.showTimes);

    if (!query->exec())
    {
        VERBOSE(VB_IMPORTANT, "Failure to Link Movie to Theater");
    }
}


