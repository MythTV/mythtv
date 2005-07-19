#include <qlayout.h>
#include <qapplication.h>
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
#include <qprocess.h>
#include <qfiledialog.h>

using namespace std;

#include "metadata.h"
#include "videomanager.h"
#include "editmetadata.h"
#include "videofilter.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>


VideoManager::VideoManager(MythMainWindow *parent, const char *name)
            : MythDialog(parent, name)
{
    updateML = false;
    debug = 0;
    isbusy = false;    // ignores keys when true (set when doing http request)

    expectingPopup = false;
    popup = NULL;
    videoDir = gContext->GetSetting("VideoStartupDir");    
    artDir = gContext->GetSetting("VideoArtworkDir");
    currentVideoFilter = new VideoFilterSettings(true, "VideoManager");
    RefreshMovieList();
    
    fullRect = QRect(0, 0, size().width(), size().height());
    

    noUpdate = false;
    curIMDBNum = "";
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
    
    m_state = SHOWING_MAINWINDOW;

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
    delete theme;
    delete bgTransBackup;

    if (curitem)
        delete curitem;
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
            else if(m_state == SHOWING_IMDBMANUAL)
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
    if(m_state == SHOWING_IMDBMANUAL)
    {
        curIMDBNum = curIMDBNum + text;
        update(imdbEnterRect);
    }
}

void VideoManager::RefreshMovieList()
{
    if (updateML == true)
        return;
    updateML = true;
    m_list.clear();

    QString thequery = QString("SELECT intid FROM %1 %2 %3")
                               .arg(currentVideoFilter->BuildClauseFrom())
                               .arg(currentVideoFilter->BuildClauseWhere(0))
                               .arg(currentVideoFilter->BuildClauseOrderBy());
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    Metadata *myData;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
           unsigned int idnum = query.value(0).toUInt();

           myData = new Metadata();
           myData->setID(idnum);
           myData->fillDataFromID();
           m_list.append(*myData);

           delete myData;
        }
    }
    updateML = false;
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

// Copy movie poster to appropriate directory and return full pathname to it
QString VideoManager::GetMoviePoster(QString movieNum)
{
    QString movieFile = curitem->Filename().section('.', 0, -2);
    QStringList images = QImage::inputFormatList();

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

    // Obtain movie poster
    QStringList args = QStringList::split(' ', 
              gContext->GetSetting("MoviePosterCommandLine", 
              gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -P"));
    args += movieNum;

    // execute external command to obtain url of movie poster
    QStringList lines = QStringList::split('\n', 
                                    executeExternal(args, "Poster Query"));
    QString uri = "";
    for (QStringList::Iterator it = lines.begin();it != lines.end(); ++it) {
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
    if( fileprefix.length() == 0 )
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

    VERBOSE(VB_ALL, QString("Copying '%1' -> '%2'...").arg(uri).arg(fileprefix));
    QUrlOperator *op = new QUrlOperator();
    connect(op, SIGNAL(finished(QNetworkOperation*)), 
          this, SLOT(copyFinished(QNetworkOperation*)) );
    iscopycomplete = false;
    iscopysuccess = false;

    op->copy(uri, "file:" + fileprefix);

    int nTimeout = gContext->GetNumSetting("PosterDownloadTimeout", 30) * 100;

    // wait for completion
    for (int i = 0; i < nTimeout; i++) {
       if (!iscopycomplete) {
          qApp->processEvents();
          usleep(10000);
       } else {
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

       QString err = QString("Copying of '%1' timed out").arg(uri);
       cerr << err << endl;
       VERBOSE(VB_ALL, err);

       MythPopupBox::showOkPopup( gContext->GetMainWindow(),
                                  QObject::tr("Could not retrieve poster"),
                                  QObject::tr("A movie poster exists for this movie but "
                                              "Myth could not retrieve it within a reasonable "
                                              "amount of time.\n"));
    }
    delete op;
    return localfile;
}

void VideoManager::copyFinished(QNetworkOperation* op) {
   QString state, operation;
   switch(op->operation()) {
      case QNetworkProtocol::OpMkDir: operation = "MkDir"; break;
      case QNetworkProtocol::OpRemove: operation = "Remove"; break; 
      case QNetworkProtocol::OpRename: operation = "Rename"; break; 
      case QNetworkProtocol::OpGet: operation = "Get"; break;
      case QNetworkProtocol::OpPut: operation = "Put"; break; 
      default: operation = "Uknown"; break;
   }
   switch(op->state()) {
      case QNetworkProtocol::StWaiting:
         state = "The operation is in the QNetworkProtocol's queue waiting to be prcessed.";
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
         state = "The operation has been processed but has been stopped before it finished, and is waiting to be processed.";
         break;
      default: state = "Unknown"; break;
   }
   VERBOSE(VB_ALL, QString("%1: %2: %3")
           .arg(operation)
           .arg(state)
           .arg(op->protocolDetail()) );
}

void VideoManager::GetMovieData(QString movieNum)
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
    if (lines.size() > 0) {
        for (QStringList::Iterator it = lines.begin();it != lines.end(); ++it) {
            if ( (*it).at(0) == '#')  // treat lines beg w/ # as a comment
                continue; 
            QString name = (*it).section(':', 0, 0);
            QString vale = (*it).section(':', 1);
            data[name] = vale;
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
    movieGenres.clear();
    QString genres = data["Genres"];
    int indice ;
    QString genre;
    while (genres!= ""){
        indice = genres.find(",");
        if (indice == -1){
            genre = genres;
            genres = "";
        } else {
            genre = genres.left(indice);
            genres = genres.right(genres.length()-indice -1);
        }
        movieGenres.append(genre.stripWhiteSpace());
    }
    curitem->setGenres(movieGenres);
    //movieCountries
    movieCountries.clear();
    QString countries = data["Countries"];
    QString country;
    while (countries!= ""){
        indice = countries.find(",");
        if (indice == -1){
            country = countries;
            countries = "";
        } else {
            country = countries.left(indice);
            countries = countries.right(countries.length()-indice -1);
        }
        country.stripWhiteSpace();

        movieCountries.append(country.stripWhiteSpace());
    }
    curitem->setCountries(movieCountries);
    
        curitem->setInetRef(movieNumber);
        QString movieCoverFile = "";
        movieCoverFile = GetMoviePoster(movieNumber);
        curitem->setCoverFile(movieCoverFile);
    } else {
        ResetCurrentItem();
    }

    curitem->updateDatabase();
    RefreshMovieList();
}

// Execute an external command and return results in string
//   probably should make this routing async vs polling like this
//   but it would require a lot more code restructuring
QString VideoManager::executeExternal(QStringList args, QString purpose) 
{
    QString ret = "";
    QString err = "";

    VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose).
                      arg(args.join(" ")).local8Bit() );
    QProcess proc(args, this);

    QString cmd = args[0];
    QFileInfo info(cmd);
    if (!info.exists()) {
       err = QString("\"%1\" failed: does not exist").arg(cmd.local8Bit());
    } else if (!info.isExecutable()) {
       err = QString("\"%1\" failed: not executable").arg(cmd.local8Bit());
    } else if (proc.start()) {
        while (true) {
           while (proc.canReadLineStdout() || proc.canReadLineStderr()) {
              if (proc.canReadLineStdout()) {
                ret += QString::fromLocal8Bit(proc.readLineStdout(),-1) + "\n";
              } 
              if (proc.canReadLineStderr()) {
                 if (err == "") err = cmd + ": ";
                 err += QString::fromLocal8Bit(proc.readLineStderr(),-1) + "\n";
              }
           }
           if (proc.isRunning()) {
              qApp->processEvents();
              usleep(10000);
           } else {
              if (!proc.normalExit()) {
                 err = QString("\"%1\" failed: Process exited abnormally")
                     .arg(cmd.local8Bit());
              } 
              break;
           }
       }
    } else {
       err = QString("\"%1\" failed: Could not start process")
        .arg(cmd.local8Bit());
    }

    while (proc.canReadLineStdout() || proc.canReadLineStderr()) {
        if (proc.canReadLineStdout()) {
            ret += QString::fromLocal8Bit(proc.readLineStdout(),-1) + "\n";
        }
        if (proc.canReadLineStderr()) {
           if (err == "") err = cmd + ": ";
           err += QString::fromLocal8Bit(proc.readLineStderr(), -1) + "\n";
        }
    }

    if (err != "")
    {
        if (purpose == "")
            purpose = "Command";

        cerr << err << endl;
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
        QObject::tr(purpose + " failed"), QObject::tr(err + "\n\nCheck VideoManager Settings"));
        ret = "#ERROR";
    }
    VERBOSE(VB_ALL, ret); 
    return ret;
}

// Obtain a movie listing via popular website(s) and populates movieList 
//   returns: number of possible matches returned, -1 for error
int VideoManager::GetMovieListing(QString movieName)
{
    QStringList args = QStringList::split(' ', 
              gContext->GetSetting("MovieListCommandLine", 
              gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -M tv=no;video=no"));
    args += movieName;

    // execute external command to obtain list of possible movie matches 
    QString results = executeExternal(args, "Movie Search");
/* let error parse as 0 hits, so user gets chance to enter one in manually 
    if (results == "#ERROR") {
        return -1;
    }
*/

    // parse results
    movieList.clear();
    int count = 0;
    QStringList lines = QStringList::split('\n', results);
    for (QStringList::Iterator it = lines.begin();it != lines.end(); ++it) {
        if ( (*it).at(0) == '#')  // treat lines beg w/ # as a comment
            continue; 
        movieList.push_back(*it);
        count++;
    }

    // if only a single match, assume this is it
    if (count == 1) {
        movieNumber = movieList[0].section(':', 0, 0);
    }

    if (count > 0) {
        movieList.push_back(""); // visual separator - can't get selected
    }
    movieList.push_back("manual:Manually Enter IMDB #");
    movieList.push_back("reset:Reset Entry");
    movieList.push_back("cancel:Cancel");

    return count;
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

            for (QStringList::Iterator it = movieList.begin(); 
                 it != movieList.end(); ++it)
            {
               QString data = (*it).data();
               QString moviename = data.section(':', 1);
               if (cnt < listsizeMovie)
               {
                  if (pastSkip <= 0)
                  {
                      if (cnt == inListMovie)
                      {
                          curitemMovie = moviename;
                          ltype->SetItemCurrent(cnt);
                      }

                      ltype->SetItemText(cnt, 1, moviename);

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
           {
                filename.remove(videoDir + "/");
                type->SetText(filename);
           }

           type = (UITextType *)container->GetType("director");
           if (type)
               type->SetText(director);
 
           type = (UITextType *)container->GetType("year");
           if (type)
               type->SetText(year);

           type = (UITextType *)container->GetType("coverfile");
           if (type)
           {
               coverfile.remove(artDir + "/");
               type->SetText(coverfile);
           }
           
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
        RefreshMovieList();
        update(infoRect);
    }


}

void VideoManager::cursorLeft()
{
    if(expectingPopup)
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

      
      if(inList == (listCount - 1))
      {
          inList = 0;
          inData = 0;
      }
      else if (inList > (int)((int)(listsize / 2) - 1)
          && ((int)(inData + listsize) <= (int)(dataCount - 1))
          && can_do_page_down == true)
      {
          // if we're beyond halfway through the list, scroll the window
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
      if (can_do_page_down) 
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
      if(inList == 0)
      {
          inData = dataCount - listsize;
          inList = listsize - 1;
      }
      else if (inList < ((int)(listsize / 2) + 1) && inData > 0)
      {
           // if we're beyond halfway through the list, scroll the window
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
      else 
      {
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
    if (!curitem)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), "video popup");

    expectingPopup = true;

    popup->addLabel(tr("Select action:"));
    popup->addLabel("");

    QButton *editButton = NULL;
    if (curitem)
    {
        editButton = popup->addButton(tr("Edit Metadata"), this, SLOT(slotEditMeta()));

        popup->addButton(tr("Search IMDB"), this, SLOT(slotAutoIMDB()));    
        popup->addButton(tr("Manually Enter IMDB #"), this, SLOT(slotManualIMDB()));
        popup->addButton(tr("Reset Metadata"), this, SLOT(slotResetMeta()));
        popup->addButton(tr("Toggle Browseable"), this, SLOT(slotToggleBrowseable()));
        popup->addButton(tr("Remove Video"), this, SLOT(slotRemoveVideo()));
    }

    QButton *filterButton = popup->addButton(tr("Filter Display"), this, SLOT(slotDoFilter()));
    popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));

    popup->ShowPopup(this, SLOT(slotDoCancel()));
    
    if (editButton)
        editButton->setFocus();    
    else 
        filterButton->setFocus();
}


void VideoManager::editMetadata()
{
}

void VideoManager::doWaitBackground(QPainter& p,const QString& titleText)
{
    // set the title for the wait background
    LayerSet *container = NULL;
    container = theme->GetSet("inetwait");
    cout << "Wait background activated" << endl;
    if (container)
    {
       UITextType *type = (UITextType *)container->GetType("title");
       if (type)
           type->SetText(titleText);

       container->Draw(&p, 0, 0);
       container->Draw(&p, 1, 0);
       container->Draw(&p, 2, 0);
       container->Draw(&p, 3, 0);
    }


}

void VideoManager::selected()
{
}

void VideoManager::RemoveCurrentItem()
{
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
    movieGenres.clear();
    curitem->setGenres(movieGenres);
    movieCountries.clear();
    curitem->setCountries(movieCountries);
    curitem->updateDatabase();
    RefreshMovieList();

}


void VideoManager::slotManualIMDB()
{
    cancelPopup();
    
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();
    curIMDBNum = "";
    m_state = SHOWING_IMDBMANUAL;
    update(fullRect);
    movieNumber = "";
}

void VideoManager::handleIMDBManual()
{
    QPainter p(this);
    movieNumber = curIMDBNum;

    backup.begin(this);
    grayOut(&backup);
    doWaitBackground(p, curIMDBNum);
    backup.end();

    qApp->processEvents();

    GetMovieData(curIMDBNum);

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();
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
         slotManualIMDB();
    else if (movieNumber == "reset")
         slotResetMeta();
    else if (movieNumber == "") {
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
        backup.begin(this);
        grayOut(&backup);
        doWaitBackground(p, movieNumber);
        backup.end();
        qApp->processEvents();

        GetMovieData(movieNumber);

        backup.begin(this);
        backup.drawPixmap(0, 0, myBackground);
        backup.end();
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

       backup.flush();
       backup.begin(this);
       grayOut(&backup);
       backup.end();

       // set the title for the wait background
       doWaitBackground(p, curitem->Title());
       backup.flush();

       int ret;

       if (curitem->InetRef() == "00000000") {
         ret = GetMovieListing(curitem->Title());
       } else {
         movieNumber = curitem->InetRef();
         ret = 1;
       }

      VERBOSE(VB_ALL,
              QString("GetMovieList returned %1 possible matches").arg(ret));
      if (ret == 1) {
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
      } else if (ret >= 0) {
          inListMovie = 0;
          inDataMovie = 0;
          listCountMovie = 0;
          dataCountMovie = 0;
          m_state = SHOWING_IMDBLIST;
          update(movieListRect);
      } else {
          //cout << "Error, movie not found.\n";
          backup.begin(this);
          backup.drawPixmap(0, 0, myBackground);
          backup.end();
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
        
    EditMetadataDialog* md_editor = new EditMetadataDialog(curitem, gContext->GetMainWindow(),
                                                           "edit_metadata", "video-",
                                                           "edit metadata dialog");
    
    
    md_editor->exec();
    delete md_editor;
    cancelPopup();
    curitem->fillDataFromID();

    RefreshMovieList();
    update(infoRect);
}

void VideoManager::slotRemoveVideo()
{
    cancelPopup();
    if (curitem && m_state == SHOWING_MAINWINDOW)
    {
        bool okcancel;
        MythPopupBox * ConfirmationDialog = new MythPopupBox (gContext->GetMainWindow());
        okcancel = ConfirmationDialog->showOkCancelPopup( gContext->GetMainWindow(), "",
                                                          tr("Delete this file?"), false);

        if (okcancel)
        {
            if (curitem->Remove())
                RefreshMovieList();
            else
                ConfirmationDialog->showOkPopup(gContext->GetMainWindow(), "", tr("delete failed"));
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

    if(popup)
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


void VideoManager::slotDoFilter()
{
    cancelPopup();
    
    VideoFilterDialog *vfd = new VideoFilterDialog(currentVideoFilter,
                                                   gContext->GetMainWindow(),
                                                   "filter", "video-", "Video Filter Dialog");
    vfd->exec();
    delete vfd;
    RefreshMovieList();
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

    RefreshMovieList();
    update(infoRect);
}
