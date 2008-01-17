/* ============================================================
 * File  : mythflix.cpp
 * Author: John Petrocik <john@petrocik.net>
 * Date  : 2005-10-28
 * Description :
 *
 * Copyright 2005 by John Petrocik

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <cstdlib>

#include <unistd.h>

#include <qnetwork.h>
#include <qapplication.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>
#include <qregexp.h>
#include <qprocess.h>

#include <qurl.h>
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include "mythtv/httpcomms.h"

#include "mythflix.h"
#include "flixutil.h"

MythFlix::MythFlix(MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    qInitNetworkProtocols ();

    // Setup cache directory

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythFlix";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    
    // Initialize variables
    zoom = QString("-z %1").arg(gContext->GetNumSetting("WebBrowserZoomLevel",200));
    browser = gContext->GetSetting("WebBrowserCommand",
                                   gContext->GetInstallPrefix() +
                                      "/bin/mythbrowser");
    expectingPopup = false;
    m_InColumn     = 0;
    m_UISites      = 0;
    m_UIArticles   = 0;

    setNoErase();
    loadTheme();

    updateBackground();

    // Load sites from database

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT name, url, updated FROM netflix WHERE is_queue=0 ORDER BY name");

    if (!query.isActive()) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Error in loading sites from DB"));
    }
    else {
        QString name;
        QString url;
        QDateTime time;
        while ( query.next() ) {
            name = QString::fromUtf8(query.value(0).toString());
            url  = QString::fromUtf8(query.value(1).toString());
            time.setTime_t(query.value(2).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UISites, site->name());
        item->setData(site);
    }

    
    NewsSite* site = (NewsSite*) m_NewsSites.first();
    connect(site, SIGNAL(finished(NewsSite*)),
            this, SLOT(slotNewsRetrieved(NewsSite*)));
    
    slotRetrieveNews();
}

MythFlix::~MythFlix()
{
    delete m_Theme;
}

void MythFlix::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "browse", "netflix-");

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        
        QDomElement e = child.toElement();
        if (!e.isNull()) {

            if (e.tagName() == "font") {
                m_Theme->parseFont(e);
            }
            else if (e.tagName() == "container") {
                QRect area;
                QString name;
                int context;
                m_Theme->parseContainer(e, name, context, area);

                if (name.lower() == "sites")
                    m_SitesRect = area;
                else if (name.lower() == "articles")
                    m_ArticlesRect = area;
                else if (name.lower() == "info")
                    m_InfoRect = area;
            }
            else {
                VERBOSE(VB_IMPORTANT, QString("MythFlix: Unknown element: %1").arg(e.tagName()));
                exit(-1);
            }
        }
    }

    LayerSet *container = m_Theme->GetSet("sites");
    if (!container) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get sites container."));
        exit(-1);
    }
        
    m_UISites = (UIListBtnType*)container->GetType("siteslist");
    if (!m_UISites) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get list area."));
        exit(-1);
    }
        
    connect(m_UISites, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotSiteSelected(UIListBtnTypeItem*)));

    container = m_Theme->GetSet("articles");
    if (!container) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get articles container."));
        exit(-1);
    }

    m_UIArticles = (UIListBtnType*)container->GetType("articleslist");
    if (!m_UIArticles) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get articles list area."));
        exit(-1);
    }
    
    connect(m_UIArticles, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotArticleSelected(UIListBtnTypeItem*)));
    
    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);
}


void MythFlix::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_SitesRect))
        updateSitesView();
    if (r.intersects(m_ArticlesRect))
        updateArticlesView();
    if (r.intersects(m_InfoRect))
        updateInfoView();
}

void MythFlix::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = m_Theme->GetSet("background");
    if (container)
    {
        container->Draw(&tmp, 0, 0);
    }

    tmp.end();
    m_background = bground;

    setPaletteBackgroundPixmap(m_background);
}

void MythFlix::updateSitesView()
{
    QPixmap pix(m_SitesRect.size());
    pix.fill(this, m_SitesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("sites");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_SitesRect.left(), m_SitesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythFlix::updateArticlesView()
{
    QPixmap pix(m_ArticlesRect.size());
    pix.fill(this, m_ArticlesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("articles");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_ArticlesRect.left(), m_ArticlesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythFlix::updateInfoView()
{
    QPixmap pix(m_InfoRect.size());
    pix.fill(this, m_InfoRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("info");
    if (container)
    {
        NewsSite    *site     = 0;
        NewsArticle *article  = 0;

        UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
        if (siteUIItem && siteUIItem->getData()) 
            site = (NewsSite*) siteUIItem->getData();
        
        UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();
        if (articleUIItem && articleUIItem->getData())
            article = (NewsArticle*) articleUIItem->getData();
        
        if (m_InColumn == 1) {

            if (article)
            {

                UITextType *ttype =
                    (UITextType *)container->GetType("status");
//                if (ttype)
//                    ttype->SetText("");

                ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(article->title());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(article->description());

                // removes html tags 
                { 
                    QString artText = article->description(); 
                    // Replace paragraph and break HTML with newlines 
                    if( artText.find(QRegExp("</(p|P)>")) ) 
                    { 
                        artText.replace( QRegExp("<(p|P)>"), ""); 
                        artText.replace( QRegExp("</(p|P)>"), "\n\n"); 
                    } 
                    else 
                    { 
                        artText.replace( QRegExp("<(p|P)>"), "\n\n"); 
                        artText.replace( QRegExp("</(p|P)>"), ""); 
                    }                         
                    artText.replace( QRegExp("<(br|BR|)/>"), "\n"); 
                    artText.replace( QRegExp("<(br|BR|)>"), "\n"); 
                    // These are done instead of simplifyWhitespace 
                    // because that function also strips out newlines 
                    // Replace tab characters with nothing 
                    artText.replace( QRegExp("\t"), ""); 
                    // Replace double space with single 
                    artText.replace( QRegExp("  "), ""); 
                    // Replace whitespace at beginning of lines with newline 
                    artText.replace( QRegExp("\n "), "\n"); 
                    // Remove any remaining HTML tags 
                    QRegExp removeHTML(QRegExp("</?.+>")); 
                    removeHTML.setMinimal(true); 
                    artText.remove((const QRegExp&) removeHTML); 
                    artText = artText.stripWhiteSpace(); 
                    ttype->SetText(artText); 
                }  

                QString imageLoc = article->articleURL();
                int length = imageLoc.length();
                int index = imageLoc.findRev("/");
                imageLoc = imageLoc.mid(index,length) + ".jpg";

                QString fileprefix = MythContext::GetConfDir();
                
                QDir dir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);
            
                fileprefix += "/MythFlix";
            
                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);
            
                VERBOSE(VB_FILE, QString("MythFlix: Boxshot File Prefix: %1").arg(fileprefix));

                QString sFilename(fileprefix + "/" + imageLoc);
            
                bool exists = QFile::exists(sFilename);
                if (!exists) 
                {
                    VERBOSE(VB_NETWORK, QString("MythFlix: Copying boxshot file from server (%1)").arg(imageLoc));
                    
                    QString sURL("http://cdn.nflximg.com/us/boxshots/large/" + imageLoc);
                
                    if (!HttpComms::getHttpFile(sFilename, sURL, 20000))
                        VERBOSE(VB_NETWORK, QString("MythFlix: Failed to download image from: %1").arg(sURL));
                
                    VERBOSE(VB_NETWORK, QString("MythFlix: Finished copying boxshot file from server (%1)").arg(imageLoc));
                }

                UIImageType *itype = (UIImageType *)container->GetType("boxshot");
                if (itype)
                {
                   itype->SetImage(sFilename);
                   itype->LoadImage();
        
                   if (itype->isHidden())
                       itype->show();   
                }

            }
        }
        else {

            if (site)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("status");
//                if (ttype)
//                    ttype->SetText("");

                ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(site->name());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(site->description());

                UIImageType *itype = (UIImageType *)container->GetType("boxshot");
                if (itype)
                {
                    itype->hide();   
                }

            }
        }

        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }

    p.end();


    bitBlt(this, m_InfoRect.left(), m_InfoRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythFlix::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("NetFlix", e, actions);
   
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            cursorUp();
        else if (action == "PAGEUP")
             cursorUp(true);
        else if (action == "DOWN")
            cursorDown();
        else if (action == "PAGEDOWN")
             cursorDown(true);
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if((action == "SELECT") || (action == "MENU"))
            displayOptions();
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythFlix::cursorUp(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveUp(unit);
    }
    else {
        m_UIArticles->MoveUp(unit);
    }
}

void MythFlix::cursorDown(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveDown(unit);
    }
    else {
        m_UIArticles->MoveDown(unit);
    }
}

void MythFlix::cursorRight()
{
    if (m_InColumn == 1)
    {
        slotViewArticle();
        return;
    }

    m_InColumn++;

    m_UISites->SetActive(false);
    m_UIArticles->SetActive(true);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythFlix::cursorLeft()
{
    if (m_InColumn == 0)
    {
        accept();
        return;
    }

    m_InColumn--;

    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythFlix::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        site->retrieve();
    }

}

void MythFlix::slotNewsRetrieved(NewsSite* site)
{
    processAndShowNews(site);
}

void MythFlix::processAndShowNews(NewsSite* site)
{
    if (!site)
        return;

    site->process();

    UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
    if (!siteUIItem || !siteUIItem->getData())
        return;
    
    if (site == (NewsSite*) siteUIItem->getData()) {

        m_UIArticles->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next()) {
            UIListBtnTypeItem* item =
                new UIListBtnTypeItem(m_UIArticles, article->title());
            item->setData(article);
        }

        update(m_ArticlesRect);
        update(m_InfoRect);
    } 
}

void MythFlix::slotShowNetFlixPage()
{
    if (expectingPopup)
        slotCancelPopup();
    
    UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();
    if (articleUIItem && articleUIItem->getData())
    {
        NewsArticle *article = (NewsArticle*) articleUIItem->getData();
        if(article)
        {
            QString cmdUrl(article->articleURL());
            cmdUrl.replace('\'', "%27");

            QString cmd = QString("%1 %2 '%3'")
                 .arg(browser)
                 .arg(zoom)
                 .arg(cmdUrl);
            VERBOSE(VB_GENERAL, QString("MythFlixBrowse: Opening Neflix site: (%1)").arg(cmd));
            myth_system(cmd);
        }
    }
}

void MythFlix::slotSiteSelected(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;
    
    processAndShowNews((NewsSite*) item->getData());
}

void MythFlix::slotArticleSelected(UIListBtnTypeItem*)
{
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythFlix::slotViewArticle()
{
    if (expectingPopup)
        slotCancelPopup();

    QString queueName = chooseQueue();
    if (queueName != "__NONE__")
        InsertMovieIntoQueue(queueName, false);
}

void MythFlix::slotViewArticleTop()
{
    if (expectingPopup)
        slotCancelPopup();

    QString queueName = chooseQueue();
    if (queueName != "__NONE__")
        InsertMovieIntoQueue(queueName, true);
}

void MythFlix::InsertMovieIntoQueue(QString queueName, bool atTop)
{
    UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();

    if (!articleUIItem)
        return;

    NewsArticle *article = (NewsArticle*) articleUIItem->getData();
    if(!article)
        return;

    QStringList args = gContext->GetShareDir() + "mythflix/scripts/netflix.pl";

    if (queueName != "")
    {
        args += "-q";
        args += queueName;
    }

    QString movieID(article->articleURL());
    int length = movieID.length();
    int index = movieID.findRev("/");
    movieID = movieID.mid(index+1,length);

    args += "-A";
    args += movieID;

    QString results = executeExternal(args, "Add Movie");

    if (atTop)
    {
        // Move to top of queue as well
        args = gContext->GetShareDir() + "mythflix/scripts/netflix.pl";

        if (queueName != "")
        {
            args += "-q";
            args += queueName;
        }

        args += "-1";
        args += movieID;

        results = executeExternal(args, "Move To Top");
    } 
}

void MythFlix::displayOptions()
{

    popup = new MythPopupBox(gContext->GetMainWindow(), "menu popup");

    QLabel *label = popup->addLabel(tr("Browse Options"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *topButton = popup->addButton(tr("Add to Top of Queue"), this,
                     SLOT(slotViewArticleTop()));
    popup->addButton(tr("Add to Bottom of Queue"), this, SLOT(slotViewArticle()));
                     
    popup->addButton(tr("Show NetFlix Page"), this, SLOT(slotShowNetFlixPage()));

    popup->addButton(tr("Cancel"), this, SLOT(slotCancelPopup()));

    popup->ShowPopup(this, SLOT(slotCancelPopup()));

    topButton->setFocus();

    expectingPopup = true;

}

void MythFlix::slotCancelPopup(void)
{
    popup->hide();
    expectingPopup = false;

    popup->deleteLater();
    popup = NULL;

    setActiveWindow();
}


// Execute an external command and return results in string
//   probably should make this routing async vs polling like this
//   but it would require a lot more code restructuring
QString MythFlix::executeExternal(const QStringList& args, const QString& purpose) 
{
    QString ret = "";
    QString err = "";

    VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose).
                      arg(args.join(" ")).local8Bit() );
    QProcess proc(args, this);

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
                    ret += QString::fromLocal8Bit(proc.readLineStdout(),-1) + "\n";
                } 
              
                if (proc.canReadLineStderr()) 
                {
                    if (err == "") 
                    {
                        err = cmd + ": ";
                    }                    
                 
                    err += QString::fromLocal8Bit(proc.readLineStderr(),-1) + "\n";
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
                    err = QString("\"%1\" failed: Process exited abnormally")
                                  .arg(cmd.local8Bit());
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
            ret += QString::fromLocal8Bit(proc.readLineStdout(),-1) + "\n";
        }
        
        if (proc.canReadLineStderr()) 
        {
            if (err == "") 
            {
                err = cmd + ": ";
            }                
           
            err += QString::fromLocal8Bit(proc.readLineStderr(), -1) + "\n";
        }
    }

    if (err != "")
    {
        QString tempPurpose(purpose);
        
        if (tempPurpose == "")
            tempPurpose = "Command";

        cerr << err << endl;
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
        QObject::tr(tempPurpose + " failed"), QObject::tr(err + "\n\nCheck NetFlix Settings"));
        ret = "#ERROR";
    }
    
    VERBOSE(VB_IMPORTANT, ret); 
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
