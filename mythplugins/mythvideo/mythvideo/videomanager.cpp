#include <qlayout.h>
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
#include <qurloperator.h>

using namespace std;

#include "metadata.h"
#include "videomanager.h"
#include "editmetadata.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

VideoManager::VideoManager(QSqlDatabase *ldb,
                           MythMainWindow *parent, const char *name)
            : MythDialog(parent, name)
{
    db = ldb;
    updateML = false;

    RefreshMovieList();

    fullRect = QRect(0, 0, (int)(800*wmult), (int)(600*hmult));

    noUpdate = false;
    curIMDBNum = "";
    ratingCountry = "USA";
    curitem = NULL;
    curitemMovie = "";
    can_do_page_down = false;
    can_do_page_down_movie = false;

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

    m_state = SHOWING_MAINWINDOW;
    httpGrabber = NULL;

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

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    setNoErase();
}

VideoManager::~VideoManager(void)
{
    if (httpGrabber)
    {
        httpGrabber->stop();
        delete httpGrabber;
    }
    delete urlTimer;

    delete theme;
    delete bgTransBackup;

    if (curitem)
        delete curitem;
}

void VideoManager::keyPressEvent(QKeyEvent *e)
{
    if (allowselect)
    {
        switch (e->key())
        {
            case Key_Space: 
            case Key_Enter: 
            case Key_Return: 
                selected(); 
                return;
                break;

            case Key_0:
            case Key_1:
            case Key_2:
            case Key_3:
            case Key_4:
            case Key_5:
            case Key_6:
            case Key_7:
            case Key_8:
            case Key_9:
                if(m_state == SHOWING_MAINWINDOW)
                {
                    editMetadata();
                }
                break;
            default: break;
        }
    }

    switch (e->key())
    {
        case Key_Down: cursorDown(); break;
        case Key_Up: cursorUp(); break;
        case Key_Left: cursorLeft(); break;
        case Key_Right: cursorRight(); break;
        case Key_PageUp: pageUp(); break;
        case Key_PageDown: pageDown(); break;
        case Key_Escape: exitWin(); break;
        case Key_M: case Key_I: videoMenu(); break;

        case Key_0: 
        case Key_1: 
        case Key_2: 
        case Key_3: 
        case Key_4: 
        case Key_5: 
        case Key_6: 
        case Key_7: 
        case Key_8: 
        case Key_9: 
        
            if(m_state != SHOWING_MAINWINDOW)
            {
                num(e); 
            }
            break;

        default: MythDialog::keyPressEvent(e);
    }
}

void VideoManager::num(QKeyEvent *e)
{
    if (m_state == SHOWING_IMDBMANUAL && curIMDBNum.length() != 7)
    {
        curIMDBNum = curIMDBNum + e->text();
        update(imdbEnterRect);
        if (curIMDBNum.length() == 7)
        {
            movieNumber = curIMDBNum;
            GetMovieData(curIMDBNum);
            backup.begin(this);
            backup.drawPixmap(0, 0, myBackground);
            backup.end();
            m_state = SHOWING_MAINWINDOW;
            noUpdate = false;
            update(fullRect);
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

           myData = new Metadata();
           myData->setID(idnum);
           myData->fillDataFromID(db);
           m_list.append(*myData);

           delete myData;
        }
    }
    updateML = false;
}

// Replace the numeric character references
// See http://www.w3.org/TR/html4/charset.html#h-5.3.1
static void replaceNumCharRefs(QString &str)
{
    QString &ret = str;
    int pos = 0;
    QRegExp re("&#(\\d+|(x|X)[0-9a-fA-F]+);");

    while ((pos = re.search(ret, pos)) != -1) 
    {
        int len = re.matchedLength();
        QString numStr = re.cap(1);
        bool ok = false;
        int num = -1;

        if (numStr[0] == 'x' || numStr[0] == 'X') 
        {
            QString hexStr = numStr.right(numStr.length() - 1);
            num = hexStr.toInt(&ok, 16);
        } 
        else 
            num = numStr.toInt(&ok, 10);

        QChar rep('X');

        if (ok)
           rep = QChar(num);

        ret.replace(pos, len, rep);

        pos += 1;
    }
}

QString VideoManager::parseData(QString data, QString beg, QString end)
{
    bool debug = false;
    QString ret;

    if (debug == true)
    {
        cout << "MythVideo: Parse HTML : Looking for: " << beg << ", ending with: " << end << endl;
    }
    int start = data.find(beg, 0, false) + beg.length();
    int endint = data.find(end, start + 1, false);
    if (start != ((int)beg.length() - 1) && endint != -1)
    {
        ret = data.mid(start, endint - start);

        replaceNumCharRefs(ret);

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

QString VideoManager::parseDataAnchorEnd(QString data, QString beg, QString end)
{
    bool debug = false;
    QString ret;

    if (debug == true)
    {
        cout << "MythVideo: Parse (Anchor End) HTML : Looking for: " << beg << ", ending with: " << end << endl;
    }
    int endint = data.find(end);
    int start = data.findRev(beg, endint + 1);
    if (start != - 1 && endint != -1)
    {
	start = start + beg.length();
        ret = data.mid(start, endint - start);

        replaceNumCharRefs(ret);

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
    QString beg = "<A HREF=\"/Title";
    QString end = "</A>";
    QString ret = "";

    QString movieNumber = "";
    QString movieTitle = "";
 
    int count = 0;
 
    if (data.find("Sorry there were no matches for the title") > 0)
    {
        listing["ERROR"] = tr("Sorry there were no matches for the title");
        return listing;
    }

    int start = data.find(beg, 0, false) + beg.length();
    int endint = data.find(end, start + 1, false);

    while (start != ((int)beg.length() - 1))
    {
        ret = data.mid(start, endint - start);
  
	int fnd = ret.find("title/tt") + 4; 
        movieNumber = ret.mid(fnd, ret.find("/\">") - fnd);
	
        movieTitle = ret.right(ret.length() - ret.find("\">") - 2);

        listing[movieNumber] = movieTitle;
  
        data = data.right(data.length() - endint);
        start = data.find(beg, 0, false) + beg.length();
        endint = data.find(end, start + 1, false);
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
    QString movieFile = curitem->Filename();
    QStringList images = QImage::inputFormatList();

    movieFile = movieFile.left(movieFile.findRev("."));

    QStringList::Iterator an_image_item = images.begin();
    for (; an_image_item != images.end(); ++an_image_item)
    {
        QString an_image = *an_image_item;
        QString coverFile = movieFile + "." + an_image.lower();

        QFile checkFile(coverFile);

        if (checkFile.exists())
        {
            return coverFile;
        }
    }

    if (movieNum == "Local")
        return(QString("<NULL>"));

    QString host = "www.imdb.com";
    QString path = "";

    QUrl url("http://" + host + "/title/tt" + movieNum + "/posters");

    //cout << "Grabbing Poster HTML From: " << url.toString() << endl;

    if (httpGrabber)
    {
        httpGrabber->stop();
        delete httpGrabber;
    }

    httpGrabber = new HttpComms(url);

    while (!httpGrabber->isDone())
    {
        qApp->processEvents();
        usleep(10000);
    }

    QString res;
    res = httpGrabber->getData();


    QString beg, end, filename = "<NULL>";

    // Check for posters on impawards.com first, since their posters
    // are usually much better quality

    beg = "Posters on other sites</h2></p>\n<ul>\n<li><a href=\"";
    end = "\">http://www.impawards.com";
    QString impsite = parseDataAnchorEnd(res, beg, end);
    if (impsite != "<NULL>")
    {

	//cout << "Retreiving poster from " << impsite << endl; 
	    
        QUrl impurl(impsite);

        //cout << "Grabbing Poster HTML From: " << url.toString() << endl;

        if (httpGrabber)
        {
            httpGrabber->stop();
            delete httpGrabber;
        }

        httpGrabber = new HttpComms(impurl);

        while (!httpGrabber->isDone())
        {
            qApp->processEvents();
            usleep(10000);
        }

	QString impres;
	
        impres = httpGrabber->getData();
        beg = "<img SRC=\"posters/";
	end = "\" ALT";

	filename = parseData(impres, beg, end);
	//cout << "Imp found: " << filename << endl;

	host = parseData(impsite, "//", "/");
	path = impsite.replace(QRegExp("http://" + host), QString(""));
	path = path.left(impsite.findRev("/") + 1) + "posters/";
    }

    // If the impawards site failed or wasn't available
    // just grab the poster from imdb
    if (filename == "<NULL>")
    {
        host = "posters.imdb.com";
	path = "/posters/";

	//cout << "Retreiving poster from imdb.com" << endl;
        beg = "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" "
                  "background=\"http://posters.imdb.com/posters/";
        end = "\"><td><td><a href=\"";

	filename = parseData(res, beg, end);
    }
    
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

    //cout << "Copying (" << filename << ")...";
    QUrlOperator *op = new QUrlOperator();
    op->copy(QString("http://" + host + path + filename),
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

    QUrl url("http://" + host + "/title/tt" + movieNum + "/");

    //cout << "Grabbing Data From: " << url.toString() << endl;

    if (httpGrabber)
    {
        httpGrabber->stop();
        delete httpGrabber;
    }

    httpGrabber = new HttpComms(url);

    while (!httpGrabber->isDone())
    {
        qApp->processEvents();
        usleep(10000);
    }

    QString res;
    res = httpGrabber->getData();

    //cout << "Outputting Movie Data Page\n" << res << endl;

    ParseMovieData(res);
}

int VideoManager::GetMovieListing(QString movieName)
{
    int ret = -1;
    QString host = "us.imdb.com";
    theMovieName = movieName;

    QUrl url("http://" + host + "/Tsearch?title=" + movieName + 
             "&type=fuzzy&from_year=1890" +
             "&to_year=2010&sort=smart&tv=off&x=12&y=14");

    //cout << "Grabbing Listing From: " << url.toString() << endl;

    if (httpGrabber)
    {
        httpGrabber->stop();
        delete httpGrabber;
    }

    httpGrabber = new HttpComms(url);

    urlTimer->stop();
    urlTimer->start(10000);

    stopProcessing = false;
    while (!httpGrabber->isDone())
    {
        qApp->processEvents();
        if (stopProcessing)
            return 1;
        usleep(10000);
    }

    urlTimer->stop();

    QString res;
    res = httpGrabber->getData();

    QString movies = parseData(res, "<A NAME=\"mov\">Movies</A></H2>", "</TABLE>");

    movieList.clear();

    if (movies != "<NULL>")
    {
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
    }

    movieList["manual"] = tr("Manually Enter IMDB #");
    movieList["reset"] = tr("Reset Entry");
    movieList["cancel"] = tr("Cancel");
    ret = 2;

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
 
    movieDirector = parseData(data, ">Directed by</b><br>\n<a href=\"/name/nm", "</a><br>");
    if (movieDirector != "<NULL>")
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

void VideoManager::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), 
                     Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), 
                       (int)(600*hmult));
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

    int pastSkip = (int)inData;
    can_do_page_down = false;
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
                   can_do_page_down = true;
            }
        }

        ltype->SetDownArrow(can_do_page_down);
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
    can_do_page_down_movie = false;
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
                   can_do_page_down_movie = true;
            }
        }

        ltype->SetDownArrow(can_do_page_down_movie);
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
       QString length = QString("%1").arg(curitem->Length()) + " " + 
                        tr("minutes");
       QString level = QString("%1").arg(curitem->ShowLevel());
       QString browseable = "";
       if(curitem->Browse())
       {
         browseable = tr("Yes");
       }
       else
       {
         browseable = tr("No");
       }

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
  
           type = (UITextType *)container->GetType("browseable");
           if (type)
               type->SetText(browseable);
  
           container->Draw(&tmp, 1, 0); 
           container->Draw(&tmp, 2, 0);
           container->Draw(&tmp, 3, 0);
           container->Draw(&tmp, 4, 0);  
           container->Draw(&tmp, 5, 0);
           container->Draw(&tmp, 6, 0); 
           container->Draw(&tmp, 7, 0);
           container->Draw(&tmp, 8, 0);
       }
       allowselect = true;
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

       allowselect = false;
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
 
void VideoManager::exitWin()
{
    if (m_state != SHOWING_MAINWINDOW)
    {
        m_state = SHOWING_MAINWINDOW;
        backup.begin(this);
        backup.drawPixmap(0, 0, myBackground);
        backup.end();
        update(fullRect);
        noUpdate = false;
        urlTimer->stop();
    }
    else
        emit accept();
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

void VideoManager::cursorDown()
{
  
  switch (m_state)
  {
      
    case SHOWING_MAINWINDOW:
    {

      // if we're beyond halfway through the list, scroll the window
      if (inList > (int)((int)(listsize / 2) - 1)
          && ((int)(inData + listsize) <= (int)(dataCount - 1))
          && can_do_page_down == true)
      {
          inData++;
          inList = (int)(listsize / 2);
      }
      
      // otherwise just move the selection bar down one
      else
      {
          inList++;

          if (inList >= listCount)
              inList = listCount - 1;
      }

    } break;


    case SHOWING_IMDBLIST:
    {
      if (inListMovie > (int)((int)(listsizeMovie / 2) - 1)
          && ((int)(inDataMovie + listsizeMovie) <= (int)(dataCountMovie - 1))
          && can_do_page_down_movie == true)
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
    } break;

  } // switch

      
  validateUp();

}

void VideoManager::pageDown()
{

  switch (m_state)
  {
    
    case SHOWING_MAINWINDOW:
    {

      // if there's more data to show, go a page down
      if (can_do_page_down) {

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

      // otherwise just go to the end of the list
      else
        inList = listsize - 1;

      
    } break;
    
    case SHOWING_IMDBLIST:
    {

      // if there's more data to show, go a page down
      if (can_do_page_down_movie)
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
      
      // otherwise just go to the end of the list
      else
        inListMovie = listsizeMovie - 1;
      
    } break;
    
   } // switch


  validateUp();

}

void VideoManager::validateUp() {

  switch (m_state)
  {
    
    case SHOWING_MAINWINDOW:
    {

      if ((int)(inData + inList) >= (int)(dataCount))
      {
        inData = dataCount - listsize;
        inList = listsize - 1;
      }
      else if ((int)(inData + listsize) >= (int)(dataCount))
      {
        inData = dataCount - listsize;
      }
      if (inList >= listCount)
        inList = listCount - 1;

    } break;


    case SHOWING_IMDBLIST:
    {

      if ((int)(inDataMovie + inListMovie) >= (int)(dataCountMovie))
      {
        inDataMovie = dataCountMovie - listsizeMovie;
        inListMovie = listsizeMovie - 1;
      }
      else if ((int)(inDataMovie + listsizeMovie) >= (int)(dataCountMovie))
      {
        inDataMovie = dataCountMovie - listsizeMovie;
      }
      if (inListMovie >= listCountMovie)
        inListMovie = listCountMovie - 1;


    } break;

  }

  update(fullRect);

}  


void VideoManager::cursorUp()
{
  
  //cout << "CursorUp\n";
  
  switch (m_state)
  {

    case SHOWING_MAINWINDOW:
    {

    //  cout << "CursorUp - SHOWING_MAINWINDOW\n";


      // if we're beyond halfway through the list, scroll the window
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

      // otherwise just move the selection bar up one
      else
        inList--;

      if (inList > -1)
        update(fullRect);
      else
        inList = 0;

    } break;
      
    case SHOWING_IMDBLIST:
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
        inListMovie--;
        
      if (inListMovie > -1)
        update(movieListRect);
      else
        inListMovie = 0;

    } break;
  
  } // switch

}

void VideoManager::pageUp()
{

  switch (m_state)
  {
    
    case SHOWING_MAINWINDOW:
    {
      
      if (inData > 0)
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
        
        update(fullRect);
      }

      else {
        inData = 0;
        inList = 0;
      }      

    } break;
    
    case SHOWING_IMDBLIST:
    {

      if (inDataMovie > 0)
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
        
        update(movieListRect);

      }
      
      else {
        inDataMovie = 0;
        inListMovie = 0;
      }

    } break;    
    
  } // switch
}


void VideoManager::videoMenu()
{
    QPainter p(this);
    if (m_state == SHOWING_MAINWINDOW || m_state == SHOWING_EDITWINDOW)
    {
       backup.flush();
       backup.begin(this);
       grayOut(&backup);
       backup.end();

       movieList.clear();
       movieList["manual"] = tr("Manually Enter IMDB #");
       movieList["reset"] = tr("Reset Entry");
       movieList["cancel"] = tr("Cancel");
       inListMovie = 0;
       inDataMovie = 0;
       listCountMovie = 0;
       dataCountMovie = 0;
       m_state = SHOWING_IMDBLIST;
       update(movieListRect);
    }
}


void VideoManager::editMetadata()
{
    EditMetadataDialog *md_editor = new EditMetadataDialog(db,
                                                           curitem,
                                                           gContext->GetMainWindow(),
                                                           "edit_metadata",
                                                           "video-",
                                                           "edit metadata dialog");
    md_editor->exec();
    delete md_editor;
    curitem->fillDataFromID(db);
    RefreshMovieList();
    update(infoRect);
}

void VideoManager::selected()
{

// Do IMDB Connections and Stuff
    QPainter p(this);
    if (m_state == SHOWING_MAINWINDOW || m_state == SHOWING_EDITWINDOW)
    {
       QString movieTitle = curitem->Title();
       movieTitle.replace(QRegExp(" "), "+");
       movieTitle.replace(QRegExp("_"), "+");
       m_state = SHOWING_EDITWINDOW;
 
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
          m_state = SHOWING_IMDBLIST;
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
              m_state = SHOWING_MAINWINDOW;
              update(fullRect);
              return;
          }
          //cout << "GETTING MOVIE #" << movieNumber << endl;
          GetMovieData(movieNumber);
          backup.begin(this);
          backup.drawPixmap(0, 0, myBackground);
          backup.end();
          m_state = SHOWING_MAINWINDOW;
          update(infoRect);
          update(listRect);
      }
      else if (ret == -1)
      {
          //cout << "Error, movie not found.\n";
          backup.begin(this);
          backup.drawPixmap(0, 0, myBackground);
          backup.end();
          m_state = SHOWING_MAINWINDOW;
          update(infoRect);
          update(listRect);
      }
   }
   else if (m_state == SHOWING_IMDBLIST)
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
           QString movieCoverFile = GetMoviePoster(QString("Local"));
           if (movieCoverFile != "<NULL>")
           {
               curitem->setCoverFile(movieCoverFile);
               curitem->updateDatabase(db);
               RefreshMovieList();
           }
 
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           m_state = SHOWING_MAINWINDOW;
           update(fullRect);
           movieNumber = "";
           return;
       }
       else if (movieNumber == "manual")
       {
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           curIMDBNum = "";
           m_state = SHOWING_IMDBMANUAL;
           update(fullRect);
           movieNumber = "";
           return;
       } 
       else if (movieNumber == "reset")
       {
           ResetCurrentItem();

           QString movieCoverFile = GetMoviePoster(QString("Local"));
           if (movieCoverFile != "<NULL>")
           {
               curitem->setCoverFile(movieCoverFile);
               curitem->updateDatabase(db);
               RefreshMovieList();
           }
 
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
 
           m_state = SHOWING_MAINWINDOW;
           update(fullRect);
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
               update(fullRect);
               return;
           }
           //cout << "GETTING MOVIE #" << movieNumber << endl;
           GetMovieData(movieNumber);
           backup.begin(this);
           backup.drawPixmap(0, 0, myBackground);
           backup.end();
           m_state = SHOWING_MAINWINDOW;
           update(infoRect);
           update(listRect);
           update(fullRect);
           movieNumber = "";
       }
   }
}

void VideoManager::ResetCurrentItem()
{
    QString coverFile = tr("No Cover");

    curitem->guessTitle();
    curitem->setCoverFile(coverFile);
    curitem->setYear(1895);
    curitem->setInetRef("00000000");
    curitem->setDirector(tr("Unknown"));
    curitem->setPlot(tr("None"));
    curitem->setUserRating(0.0);
    curitem->setRating(tr("NR"));
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

