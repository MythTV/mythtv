#include <qlayout.h>
#include <qaccel.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
#include <qnetwork.h>
#include <qurl.h>
#include <qdir.h>

using namespace std;

#include "metadata.h"
#include "videomanager.h"
#include <mythtv/mythcontext.h>

#ifdef ENABLE_LIRC
#include "lirc_client.h"
extern struct lirc_config *config;
#endif

VideoManager::VideoManager(QSqlDatabase *ldb,
                         QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    qInitNetworkProtocols();
    db = ldb;
    updateML = false;

    RefreshMovieList();

    noUpdate = false;
    curIMDBNum = "";
    ratingCountry = "USA";
    curitem = NULL;
    curitemMovie = "";
    pageDowner = false;
    pageDownerMovie = false;

    inList = 0;
    inData = 0;
    listCount = 0;
    dataCount = 0;

    inListMovie = 0;
    inDataMovie = 0;
    listCountMovie = 0;
    dataCountMovie = 0;

    GetMovieListingTimeoutCounter = 0;
    stopProcessing = false;

    m_state = 0;
    InetGrabber = NULL;

    accel = new QAccel(this);

    space_itemid = accel->insertItem(Key_Space);
    enter_itemid = accel->insertItem(Key_Enter);
    return_itemid = accel->insertItem(Key_Return);

    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(space_itemid, this, SLOT(selected()));
    accel->connectItem(enter_itemid, this, SLOT(selected()));
    accel->connectItem(return_itemid, this, SLOT(selected()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(exitWin()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(videoMenu()));
    accel->connectItem(accel->insertItem(Key_0), this, SLOT(num0()));
    accel->connectItem(accel->insertItem(Key_1), this, SLOT(num1()));
    accel->connectItem(accel->insertItem(Key_2), this, SLOT(num2()));
    accel->connectItem(accel->insertItem(Key_3), this, SLOT(num3()));
    accel->connectItem(accel->insertItem(Key_4), this, SLOT(num4()));
    accel->connectItem(accel->insertItem(Key_5), this, SLOT(num5()));
    accel->connectItem(accel->insertItem(Key_6), this, SLOT(num6()));
    accel->connectItem(accel->insertItem(Key_7), this, SLOT(num7()));
    accel->connectItem(accel->insertItem(Key_8), this, SLOT(num8()));
    accel->connectItem(accel->insertItem(Key_9), this, SLOT(num9()));

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    urlTimer = new QTimer(this);
    connect(urlTimer, SIGNAL(timeout()), SLOT(GetMovieListingTimeOut()));

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "manager", "video-");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythVideo: VideoManager : Failed to get selector object.\n";
        exit(0);
    }

    container = theme->GetSet("moviesel");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            listsizeMovie = ltype->GetItems();
        }
    }

    bgTransBackup = new QPixmap();
    resizeImage(bgTransBackup, "trans-backup.png");

    updateBackground();

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);

    showFullScreen();
    setActiveWindow();
}

VideoManager::~VideoManager()
{
    if (InetGrabber)
    {
        InetGrabber->stop();
        delete InetGrabber;
    }

    delete accel;
    delete theme;
}

void VideoManager::num0()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "0";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num1()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "1";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num2()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "2";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num3()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "3";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num4()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "4";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num5()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "5";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num6()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "6";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num7()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "7";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num8()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "8";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::num9()
{
    if (m_state == 3 && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + "9";
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = 0;
            noUpdate = false;
            update(fullRect());
        }
    }
}

void VideoManager::RefreshMovieList()
{
    if (updateML == true)
        return;
    updateML = true;
    m_list.clear();

    QSqlQuery query("SELECT intid FROM videometadata ORDER BY title;", db);
    Metadata *myData;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
           unsigned int idnum = query.value(0).toUInt();

           //POSSIBLE MEMORY LEAK
           myData = new Metadata();
           myData->setID(idnum);
           myData->fillDataFromID(db);
           m_list.append(*myData);
        }
    }
    updateML = false;
}

QString VideoManager::parseData(QString data, QString beg, QString end)
{
    bool debug = false;
    QString ret;

    if (debug == true)
    {
        cout << "MythVideo: Parse HTML : Looking for: " << beg << ", ending with: " << end << endl;
    }
    int start = data.find(beg) + beg.length();
    int endint = data.find(end, start + 1);
    if (start != ((int)beg.length() - 1) && endint != -1)
    {
        ret = data.mid(start, endint - start);

        ret.replace(QRegExp("&#38;"), "&");
        ret.replace(QRegExp("&#34;"), "\"");

        if (debug == true)
            cout << "MythVideo: Parse HTML : Returning : " << ret << endl;
        return ret;
    }
    else
    {
        if (debug == true)
            cout << "MythVideo: Parse HTML : Parse Failed...returning <NULL>\n";
        ret = "<NULL>";
        return ret;
    }
}

QMap<QString, QString> VideoManager::parseMovieList(QString data)
{
    QMap<QString, QString> listing;
    QString beg = "<A HREF=\"/Title?";
    QString end = "</A>";
    QString ret = "";

    QString movieNumber = "";
    QString movieTitle = "";
 
    int count = 0;
 
    if (data.find("Sorry there were no matches for the title") > 0)
    {
        listing["ERROR"] = "Sorry there were no matches for the title";
        return listing;
    }

    int start = data.find(beg) + beg.length();
    int endint = data.find(end, start + 1);

    while (start != ((int)beg.length() - 1))
    {
        ret = data.mid(start, endint - start);
  
        movieNumber = ret.left(ret.find("\">"));
        movieTitle = ret.right(ret.length() - ret.find("\">") - 2);

        listing[movieNumber] = movieTitle;
  
        data = data.right(data.length() - endint);
        start = data.find(beg) + beg.length();
        endint = data.find(end, start + 1);
        count++;
        if (count == 10)
		break;
    }
    return listing;

}

void VideoManager::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

QString VideoManager::GetMoviePoster(QString movieNum)
{
    QString host = "www.imdb.com";

    QUrl url("http://" + host + "/Posters?" + movieNum
           + " HTTP/1.1\nHost: " + host + "\nUser-Agent: Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en)"
           + " Gecko/25250101 Netscape/5.432b1\n");

    //cout << "Grabbing Poster HTML From: " << url.toString() << endl;

    if (InetGrabber)
    {
        InetGrabber->stop();
        delete InetGrabber;
    }

    InetGrabber = new INETComms(url);

    while (!InetGrabber->isDone())
    {
        qApp->processEvents();
    }

    QString res;
    res = InetGrabber->getData();

    QString beg = "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" "
                  "background=\"http://posters.imdb.com/posters/";
    QString end = "\"><td><td><a href=\"";
    QString filename = "";

    filename = parseData(res, beg, end);
    if (filename == "<NULL>")
    {
        cout << "MyhVideo: Error parsing poster filename.\n";
        return filename;
    }

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythVideo";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    host = "posters.imdb.com";

    //cout << "Copying (" << filename << ")...";
    QUrlOperator *op = new QUrlOperator();
    op->copy(QString("http://" + host + "/posters/" + filename),
             "file:" + fileprefix);
    //cout << "Done.\n";

    filename = filename.right(filename.length() - filename.findRev("/") - 1);
    fileprefix = fileprefix + "/" + filename;

    return fileprefix;

}

void VideoManager::GetMovieData(QString movieNum)
{
    movieNumber = movieNum;
    QString host = "www.imdb.com";

    QUrl url("http://" + host + "/Title?" + movieNum
           + " HTTP/1.1\nHost: " + host + "\nUser-Agent: Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en)"
           + " Gecko/25250101 Netscape/5.432b1\n");

    //cout << "Grabbing Data From: " << url.toString() << endl;

    if (InetGrabber)
    {
        InetGrabber->stop();
        delete InetGrabber;
    }

    InetGrabber = new INETComms(url);

    while (!InetGrabber->isDone())
    {
        qApp->processEvents();
    }

    QString res;
    res = InetGrabber->getData();

    ParseMovieData(res);

}

int VideoManager::GetMovieListing(QString movieName)
{
    int ret = -1;
    QString host = "us.imdb.com";
    theMovieName = movieName;

    QUrl url("http://" + host + "/Tsearch?title=" + movieName + "&type=fuzzy&from_year=1890"
           + "&to_year=2010&sort=smart&tv=off&x=12&y=14"
	   + " HTTP/1.1\nHost: us.imdb.com\nUser-Agent: Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en)"
	   + " Gecko/25250101 Netscape/5.432b1\n");

    //cout << "Grabbing Listing From: " << url.toString() << endl;

    if (InetGrabber)
    {
        InetGrabber->stop();
        delete InetGrabber;
    }

    InetGrabber = new INETComms(url);

    urlTimer->stop();
    urlTimer->start(5000);

    stopProcessing = false;
    while (!InetGrabber->isDone())
    {
        qApp->processEvents();
	if (stopProcessing)
		return 1;
    }

    urlTimer->stop();

    QString res;
    res = InetGrabber->getData();

    QString movies = parseData(res, "<A NAME=\"mov\">Movies</A></H2>", "</TABLE>");
    if (movies == "<NULL>")
    {
        // Individual Movie Page or Title Lookup Failure, either way cancel!
        ret = 1;
    }
    else
    {
        movieList.clear();
        movieList = parseMovieList(movies);

        QMap<QString, QString>::Iterator it;

        //cout << endl << endl;
        for (it = movieList.begin(); it != movieList.end(); ++it)
        {
            if (movieList.count() == 1)
            {
                movieNumber = it.key();
                if (movieNumber == "ERROR")
                    ret = -1;
                else 
                    ret = 1;
                return ret;
            }
            //cout << it.data() << endl;
        }
        movieList["manual"] = "Manually Enter IMDB #";
        movieList["reset"] = "Reset Entry";
        movieList["cancel"] = "Cancel";
        ret = 2;
    }

    return ret;
}

void VideoManager::ParseMovieData(QString data)
{
    movieTitle = parseData(data, "<title>", "(");
    QString mYear = parseData(data, "(", ")");
    if (mYear.find("/") > 0)
        movieYear = mYear.left(mYear.find("/")).toInt();
    else 
        movieYear = mYear.toInt();
 
    movieDirector = parseData(data, ">Directed by</b><br>\n<a href=\"/Name?", "</a><br>");
    movieDirector = movieDirector.right(movieDirector.length() - movieDirector.find("\">") - 2);
    moviePlot = parseData(data, "<b class=\"ch\">Plot Outline:</b> ", "<a href=\"");
    if (moviePlot == "<NULL>")
        moviePlot = parseData(data, "<b class=\"ch\">Plot Summary:</b> ", "<a href=\"");

    QString rating = parseData(data, "<b class=\"ch\">User Rating:</b>", " (");
    rating = parseData(rating, "<b>", "/");
    movieUserRating = rating.toFloat();

    movieRating = parseData(data, "<b class=\"ch\"><a href=\"/mpaa\">MPAA</a>:</b> ", "<br>");
    if (movieRating == "<NULL>")
    {
        movieRating = parseData(data, "<b class=\"ch\">Certification:</b>", "<br>");
        movieRating = parseData(movieRating, "<a href=\"/List?certificates=" + ratingCountry, "/a>");
        movieRating = parseData(movieRating, "\">", "<");
    }
    movieRuntime = parseData(data, "<b class=\"ch\">Runtime:</b>", " min").toInt();
    QString movieCoverFile = "";
    movieCoverFile = GetMoviePoster(movieNumber);

    /*
    cout << "      Title:\t" << movieTitle << endl;
    cout << "       Year:\t" << movieYear << endl;
    cout << "   Director:\t" << movieDirector << endl;
    cout << "       Plot:\t" << moviePlot << endl;
    cout << "User Rating:\t" << movieUserRating << endl;
    cout << "     Rating:\t" << movieRating << endl;
    cout << "    Runtime:\t" << movieRuntime << endl;
    cout << " Cover File:\t" << movieCoverFile << endl;
    */

    if (movieTitle == "<NULL>")
        ResetCurrentItem();
    else 
    {
       curitem->setTitle(movieTitle);
       curitem->setYear(movieYear);
       curitem->setDirector(movieDirector);
       curitem->setPlot(moviePlot);
       curitem->setUserRating(movieUserRating);
       curitem->setRating(movieRating);
       curitem->setLength(movieRuntime);
       curitem->setInetRef(movieNumber);
       curitem->setCoverFile(movieCoverFile);
    }
    curitem->updateDatabase(db);
    RefreshMovieList();
}

void VideoManager::resizeImage(QPixmap *dst, QString file)
{
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
         file = themeDir + file;
    else
         file = baseDir + file;

    if (hmult == 1 && wmult == 1)
    {
         dst->load(file);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            scalerImg = sourceImg->smoothScale((int)(sourceImg->width() * wmult),
                                               (int)(sourceImg->height() * hmult));
            dst->convertFromImage(scalerImg);
        }
        delete sourceImg;
    }
}

void VideoManager::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), (int)(600*hmult));
}

void VideoManager::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (m_state == 0 || m_state == 3)
    {
       if (r.intersects(listRect) && noUpdate == false)
       {
           updateList(&p);
       }
       if (r.intersects(infoRect) && noUpdate == false)
       {
           updateInfo(&p);
       }
       if (r.intersects(imdbEnterRect) && m_state == 3)
       {
           noUpdate = true;
           updateIMDBEnter(&p);
       }
    }
    if (m_state == 2)
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

    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    QString filename = "";
    QString title = "";

    ValueMetadata::Iterator it;
    ValueMetadata::Iterator start;
    ValueMetadata::Iterator end;

    start = m_list.begin();
    end = m_list.end();

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (it = start; it != end; ++it)
            {
               if (cnt < listsize)
               {
                  if (pastSkip <= 0)
                  {
                      filename =(*it).Filename();
                      if (0 == (*it).Title().compare("title"))
                      {
                          title = filename.section('/', -1);
                          if (!gContext->GetNumSetting("ShowFileExtensions"))
                          title = title.section('.',0,-2);
                      }
                      else
                          title = (*it).Title();

                      if (cnt == inList)
                      {
                          if (curitem)
                              delete curitem;
                          curitem = new Metadata(*(it));
                          ltype->SetItemCurrent(cnt);
                      }

                      ltype->SetItemText(cnt, 1, title);
                      ltype->SetItemText(cnt, 2, (*it).Director());
                      QString year = QString("%1").arg((*it).Year());
                      if (year == "1895")
                          year = "?";
                      ltype->SetItemText(cnt, 3, year);

                      cnt++;
                      listCount++;
                  }
                  pastSkip--;
               }
               else
                   pageDowner = true;
            }
        }

        ltype->SetDownArrow(pageDowner);
        if (inData > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
    }

    dataCount = m_list.count();

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
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

    int pastSkip = (int)inDataMovie;
    pageDownerMovie = false;
    listCountMovie = 0;

    QString title = "";

    QMap<QString, QString>::Iterator it;

    LayerSet *container = NULL;
    container = theme->GetSet("moviesel");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (it = movieList.begin(); it != movieList.end(); ++it)
            {
               if (cnt < listsizeMovie)
               {
                  if (pastSkip <= 0)
                  {
                      if (cnt == inListMovie)
                      {
                          curitemMovie = (*it).data();
                          ltype->SetItemCurrent(cnt);
                      }

                      ltype->SetItemText(cnt, 1, (*it).data());

                      cnt++;
                      listCountMovie++;
                  }
                  pastSkip--;
               }
               else
                   pageDownerMovie = true;
            }
        }

        ltype->SetDownArrow(pageDownerMovie);
        if (inDataMovie > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
    }

    dataCountMovie = m_list.count();

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
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
   
    LayerSet *container = NULL;
    container = theme->GetSet("enterimdb");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("numhold");
        if (type)
            type->SetText(curIMDBNum);
    }

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
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

    if (m_list.count() > 0 && curitem)
    {
       QString title = curitem->Title();
       QString filename = curitem->Filename();
       QString director = curitem->Director();
       QString year = QString("%1").arg(curitem->Year());
       if (year == "1895")
           year = "?";
       QString coverfile = curitem->CoverFile();
       QString inetref = curitem->InetRef();
       QString plot = curitem->Plot();
       QString userrating = QString("%1").arg(curitem->UserRating());
       QString rating = curitem->Rating();
       QString length = QString("%1").arg(curitem->Length()) + " minutes";
       QString level = QString("%1").arg(curitem->ShowLevel());

       LayerSet *container = NULL;
       container = theme->GetSet("info");
       if (container)
       {
           UITextType *type = (UITextType *)container->GetType("title");
           if (type)
               type->SetText(title);

           type = (UITextType *)container->GetType("filename");
           if (type)
               type->SetText(filename);

           type = (UITextType *)container->GetType("director");
           if (type)
               type->SetText(director);
 
           type = (UITextType *)container->GetType("year");
           if (type)
               type->SetText(year);

           type = (UITextType *)container->GetType("coverfile");
           if (type)
               type->SetText(coverfile);

           type = (UITextType *)container->GetType("inetref");
           if (type)
               type->SetText(inetref);

           type = (UITextType *)container->GetType("plot");
           if (type)
               type->SetText(plot);
 
           type = (UITextType *)container->GetType("userrating");
           if (type)
               type->SetText(userrating);

           type = (UITextType *)container->GetType("rating");
           if (type)
               type->SetText(rating);

           type = (UITextType *)container->GetType("length");
           if (type)
               type->SetText(length);

           type = (UITextType *)container->GetType("level");
           if (type)
               type->SetText(level);
  
           container->Draw(&tmp, 1, 0); 
           container->Draw(&tmp, 2, 0);
           container->Draw(&tmp, 3, 0);
           container->Draw(&tmp, 4, 0);  
           container->Draw(&tmp, 5, 0);
           container->Draw(&tmp, 6, 0); 
           container->Draw(&tmp, 7, 0);
           container->Draw(&tmp, 8, 0);
       }
    }
    else
    {
       LayerSet *norec = theme->GetSet("novideos_info");
       if (norec)
       {
           norec->Draw(&tmp, 4, 0);
           norec->Draw(&tmp, 5, 0);
           norec->Draw(&tmp, 6, 0);
           norec->Draw(&tmp, 7, 0);
           norec->Draw(&tmp, 8, 0);
       }

       accel->setItemEnabled(space_itemid, false);
       accel->setItemEnabled(enter_itemid, false);
       accel->setItemEnabled(return_itemid, false);

    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

}

void VideoManager::LoadWindow(QDomElement &element)
{

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void VideoManager::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "info")
        infoRect = area;
    if (name.lower() == "moviesel")
        movieListRect = area;
    if (name.lower() == "enterimdb")
        imdbEnterRect = area;
}
 



#ifdef ENABLE_LIRC

void VideoManager::dataReceived()
{
  //  printf("get lirc data\n");
    char *code;
    char *c;
    int ret;
    char buffer[200];
    size_t l;

    while (ret=lirc_nextcode(&code)==0 && code !=NULL)
    {
        while ((ret=lirc_code2char(config,code,&c))==0 && c!=NULL)
        {
            if (code==NULL) 
                continue;
            QString str = QString(c);
            if (str == "Up")
	    { 
                listview->setCurrentItem(listview->currentItem()->itemAbove());
                listview->setSelected(listview->currentItem(),TRUE);
   	        listview->ensureItemVisible(listview->currentItem());
	    }
            else if (str == "Down")
   	    {
                listview->setCurrentItem(listview->currentItem()->itemBelow());
                listview->setSelected(listview->currentItem(),TRUE);
	        listview->ensureItemVisible(listview->currentItem());
	    }
            else if(str == "Enter")
	    {
	        doSelected(listview->currentItem());
	    }
        }
   
        free(code);
        if(ret==-1) break;
    }
}
#endif

void VideoManager::exitWin()
{
    if (m_state != 0)
    {
        m_state = 0;
        backup.begin(this);
        backup.drawPixmap(0, 0, myBackground);
        backup.end();
        update(fullRect());
        noUpdate = false;
	urlTimer->stop();
    }
    else
        emit killTheApp();
}

void VideoManager::cursorLeft()
{
    int curshowlevel = curitem->ShowLevel();

    curshowlevel--;
    if (curshowlevel > -1)
    {
        curitem->setShowLevel(curshowlevel);
        curitem->updateDatabase(db);
        RefreshMovieList();
        update(infoRect);
    }
}

void VideoManager::cursorRight()
{
    int curshowlevel = curitem->ShowLevel();

    curshowlevel++;
    if (curshowlevel < 5)
    {
        curitem->setShowLevel(curshowlevel);
        curitem->updateDatabase(db);
        RefreshMovieList();
        update(infoRect);
    }
}

void VideoManager::cursorDown(bool page)
{
    if (page == false)
    {
      if (m_state == 0)
      {
        if (inList > (int)((int)(listsize / 2) - 1)
            && ((int)(inData + listsize) <= (int)(dataCount - 1))
            && pageDowner == true)
        {
            inData++;
            inList = (int)(listsize / 2);
        }
        else
        {
            inList++;

            if (inList >= listCount)
                inList = listCount - 1;
        }
      }
      else if (m_state == 2)
      {
        if (inListMovie > (int)((int)(listsizeMovie / 2) - 1)
            && ((int)(inDataMovie + listsizeMovie) <= (int)(dataCountMovie - 1))
            && pageDownerMovie == true)
        {
            inDataMovie++;
            inListMovie = (int)(listsizeMovie / 2);
        }
        else
        {
            inListMovie++;

            if (inListMovie >= listCountMovie)
                inListMovie = listCountMovie - 1;
        }
      }
    }
    else if (page == true && pageDowner == true && m_state == 0)
    {
        if (inList >= (int)(listsize / 2) || inData != 0)
        {
            inData = inData + listsize;
        }
        else if (inList < (int)(listsize / 2) && inData == 0)
        {
            inData = (int)(listsize / 2) + inList;
            inList = (int)(listsize / 2);
        }
    }
    else if (page == true && pageDownerMovie == true && m_state == 2)
    {
        if (inListMovie >= (int)(listsizeMovie / 2) || inDataMovie != 0)
        {
            inDataMovie = inDataMovie + listsizeMovie;
        }
        else if (inListMovie < (int)(listsizeMovie / 2) && inDataMovie == 0)
        {
            inDataMovie = (int)(listsizeMovie / 2) + inListMovie;
            inListMovie = (int)(listsizeMovie / 2);
        }
    }
    else if (page == true && pageDowner == false && m_state == 0)
    {
        inList = listsize - 1;
    }
    else if (page == true && pageDownerMovie == false && m_state == 2)
    {
        inListMovie = listsizeMovie - 1;
    }

    if ((int)(inData + inList) >= (int)(dataCount) && m_state == 0)
    {
        inData = dataCount - listsize;
        inList = listsize - 1;
    }
    else if ((int)(inData + listsize) >= (int)(dataCount) && m_state == 0)
    {
        inData = dataCount - listsize;
    }
    if (inList >= listCount && m_state == 0)
        inList = listCount - 1;

    if ((int)(inDataMovie + inListMovie) >= (int)(dataCountMovie) && m_state == 2)
    {
        inDataMovie = dataCountMovie - listsizeMovie;
        inListMovie = listsizeMovie - 1;
    }
    else if ((int)(inDataMovie + listsizeMovie) >= (int)(dataCountMovie) && m_state == 2)
    {
        inDataMovie = dataCountMovie - listsizeMovie;
    }
    if (inListMovie >= listCountMovie && m_state == 2)
        inListMovie = listCountMovie - 1;

    update(fullRect());
}

void VideoManager::cursorUp(bool page)
{

    if (page == false)
    {
      if (m_state == 0)
      {
         if (inList < ((int)(listsize / 2) + 1) && inData > 0)
         {
            inList = (int)(listsize / 2);
            inData--;
            if (inData < 0)
            {
                inData = 0;
                inList--;
            }
         }
         else
         {
             inList--;
         }
      }
      if (m_state == 2)
      {
         if (inListMovie < ((int)(listsizeMovie / 2) + 1) && inDataMovie > 0)
         {
            inListMovie = (int)(listsizeMovie / 2);
            inDataMovie--;
            if (inDataMovie < 0)
            {
                inDataMovie = 0;
                inListMovie--;
            }
         }
         else
         {
             inListMovie--;
         }
      }
    }
    else if (page == true && inData > 0 && m_state == 0)
    {
        inData = inData - listsize;
        if (inData < 0)
        {
            inList = inList + inData;
            inData = 0;
            if (inList < 0)
                inList = 0;
         }

         if (inList > (int)(listsize / 2))
         {
              inList = (int)(listsize / 2);
              inData = inData + (int)(listsize / 2) - 1;
         }
    }
    else if (page == true && inDataMovie > 0 && m_state == 2)
    {
        inDataMovie = inDataMovie - listsizeMovie;
        if (inDataMovie < 0)
        {
            inListMovie = inListMovie + inDataMovie;
            inDataMovie = 0;
            if (inListMovie < 0)
                inListMovie = 0;
         }

         if (inListMovie > (int)(listsizeMovie / 2))
         {
              inListMovie = (int)(listsizeMovie / 2);
              inDataMovie = inDataMovie + (int)(listsizeMovie / 2) - 1;
         }
    }
    else if (page == true)
    {
       if (m_state == 0)
       {
          inData = 0;
          inList = 0;
       } 
       else if (m_state == 2)
       {
          inDataMovie = 0;
          inListMovie = 0;
       }
    }

    if (inList > -1 && m_state == 0)
    {
        update(fullRect());
    }
    else if (m_state == 0)
        inList = 0;

    if (inListMovie > -1 && m_state == 2)
    {
        update(movieListRect);
    }
    else if (m_state == 2)
        inListMovie = 0;
}

void VideoManager::videoMenu()
{
    QPainter p(this);
    if (m_state == 0 || m_state == 1)
    {
       backup.flush();
       backup.begin(this);
       grayOut(&backup);
       backup.end();

       movieList.clear();
       movieList["manual"] = "Manually Enter IMDB #";
       movieList["reset"] = "Reset Entry";
       movieList["cancel"] = "Cancel";
       inListMovie = 0;
       inDataMovie = 0;
       listCountMovie = 0;
       dataCountMovie = 0;
       m_state = 2;
       update(movieListRect);
    }
}


void VideoManager::selected()
{
// Do IMDB Connections and Stuff
    QPainter p(this);
    if (m_state == 0 || m_state == 1)
    {
       QString movieTitle = curitem->Title();
       movieTitle.replace(QRegExp(" "), "+");
       m_state = 1;
 
       backup.flush();
       backup.begin(this);
       grayOut(&backup);
       backup.end();

       LayerSet *container = NULL;
       container = theme->GetSet("inetwait");
       if (container)
       {
          UITextType *type = (UITextType *)container->GetType("title");
          if (type)
              type->SetText(curitem->Title());
 
          container->Draw(&p, 0, 0);
          container->Draw(&p, 1, 0);
          container->Draw(&p, 2, 0);
          container->Draw(&p, 3, 0);
       }

      int ret = GetMovieListing(movieTitle);
      if (ret == 2)
      {
          inListMovie = 0;
          inDataMovie = 0;
          listCountMovie = 0;
          dataCountMovie = 0;
          m_state = 2;
          update(movieListRect);
      }
      else if (ret == 1)
      {
          if (movieNumber.isNull() || movieNumber.length() == 0)
          {
              ResetCurrentItem();
              backup.begin(this);
              backup.drawPixmap(0, 0, myBackground);
              backup.end();
              m_state = 0;
              update(fullRect());
              return;
          }
          //cout << "GETTING MOVIE #" << movieNumber << endl;
          GetMovieData(movieNumber);
          backup.begin(this);
          backup.drawPixmap(0, 0, myBackground);
          backup.end();
          m_state = 0;
          update(infoRect);
          update(listRect);
      }
      else if (ret == -1)
      {
          //cout << "Error, movie not found.\n";
          backup.begin(this);
          backup.drawPixmap(0, 0, myBackground);
          backup.end();
          m_state = 0;
          update(infoRect);
          update(listRect);
      }
   }
   else if (m_state == 2)
   {
       QMap<QString, QString>::Iterator it;

       for (it = movieList.begin(); it != movieList.end(); ++it)
       {
           if (curitemMovie == it.data())
           {
               movieNumber = it.key();
               break;
           }
       }

       if (movieNumber == "cancel")
       {
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           m_state = 0;
           update(fullRect());
           update(fullRect());
           movieNumber = "";
           return;
       }
       else if (movieNumber == "manual")
       {
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           curIMDBNum = "";
           m_state = 3;
           update(fullRect());
           update(fullRect());
           movieNumber = "";
           return;
       } 
       else if (movieNumber == "reset")
       {
           ResetCurrentItem();
 
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
 
           m_state = 0;
           update(fullRect());
           update(fullRect());
           movieNumber = "";
           return;
       }
       else
       {
           if (movieNumber.isNull() || movieNumber.length() == 0)
           {
               ResetCurrentItem();
               backup.begin(this);
               backup.drawPixmap(0, 0, myBackground);
               backup.end();
               update(fullRect());
               return;
           }
           //cout << "GETTING MOVIE #" << movieNumber << endl;
           GetMovieData(movieNumber);
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           m_state = 0;
           update(infoRect);
           update(listRect);
           update(fullRect());
           movieNumber = "";
       }
   }
}

void VideoManager::ResetCurrentItem()
{
    QString name = curitem->Filename();
    QString title = name.right(name.length() - name.findRev("/") - 1);
    title = title.left(title.find("."));
    title = title.left(title.find("["));
    title = title.left(title.find("("));

    QString coverFile = "No Cover";

    curitem->setTitle(title);
    curitem->setCoverFile(coverFile);
    curitem->setYear(1895);
    curitem->setInetRef("00000000");
    curitem->setDirector("Uknown");
    curitem->setPlot("None");
    curitem->setUserRating(0.0);
    curitem->setRating("NR");
    curitem->setLength(0);
    curitem->setShowLevel(1);

    curitem->updateDatabase(db);
    RefreshMovieList();
}

void VideoManager::GetMovieListingTimeOut()
{
    //Increment the counter and check were not over the limit
    if(++GetMovieListingTimeoutCounter != 3)
    {
        //Try again
        GetMovieListing(theMovieName);
    }
    else
    {
        GetMovieListingTimeoutCounter = 0;
        cerr << "Failed to contact  server" << endl;

        //Set the stopProcessing var so the other thread knows what to do
        stopProcessing = true;

        //Let the exitWin method take care of closing the dialog screen
        exitWin();
    }

    return;
}

QRect VideoManager::fullRect() const
{
    QRect r(0, 0, (int)(800*wmult), (int)(600*hmult));
    return r;
}
