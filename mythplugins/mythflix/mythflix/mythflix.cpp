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

// C headers
#include <unistd.h>

// Qt headers
#include <qapplication.h>
#include <qdir.h>
#include <qregexp.h>
#include <q3process.h>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythFlix headers
#include "mythflix.h"
#include "flixutil.h"

/** \brief Creates a new MythFlix Browse Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythFlix::MythFlix(MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name)
{
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

    m_sitesList = m_articlesList = NULL;
    m_statusText = m_titleText = m_descText = NULL;
    m_boxshotImage = NULL;
    m_menuPopup = NULL;
}

MythFlix::~MythFlix()
{
}

bool MythFlix::Create()
{

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("netflix-ui.xml", "browse", this);

    if (!foundtheme)
        return false;

    m_sitesList = dynamic_cast<MythListButton *>
                (GetChild("siteslist"));

    connect(m_sitesList, SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT(slotSiteSelected(MythListButtonItem*)));

    m_articlesList = dynamic_cast<MythListButton *>
                (GetChild("articleslist"));

    m_statusText = dynamic_cast<MythUIText *>
                (GetChild("status"));

    m_titleText = dynamic_cast<MythUIText *>
                (GetChild("title"));

    m_descText = dynamic_cast<MythUIText *>
                (GetChild("description"));

    m_boxshotImage = dynamic_cast<MythUIImage *>
                (GetChild("boxshot"));

    if (!m_sitesList || !m_articlesList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_sitesList, SIGNAL(itemSelected( MythListButtonItem*)),
            this, SLOT(  updateInfoView(MythListButtonItem*)));
    connect(m_articlesList, SIGNAL(itemSelected( MythListButtonItem*)),
            this, SLOT(  updateInfoView(MythListButtonItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_sitesList);
    m_sitesList->SetActive(true);
    m_articlesList->SetActive(false);

    loadData();

    return true;
}

void MythFlix::loadData()
{

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
            name = query.value(0).toString();
            url  = query.value(1).toString();
            time.setTime_t(query.value(2).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        MythListButtonItem* item =
            new MythListButtonItem(m_sitesList, site->name());
        item->setData(site);
    }


    NewsSite* site = (NewsSite*) m_NewsSites.first();
    if (site)
    {
        connect(site, SIGNAL(finished(NewsSite*)),
            this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

    slotRetrieveNews();

}

void MythFlix::updateInfoView(MythListButtonItem* selected)
{
    if (!selected)
        return;

    if (GetFocusWidget() == m_articlesList)
    {

        NewsArticle *article = (NewsArticle*) selected->getData();

        if (article)
        {

            if (m_titleText)
                m_titleText->SetText(article->title());

            if (m_descText)
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
                m_descText->SetText(artText);
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

            VERBOSE(VB_FILE, QString("MythFlix: Boxshot File Prefix: %1")
                                    .arg(fileprefix));

            QString sFilename(fileprefix + "/" + imageLoc);

            bool exists = QFile::exists(sFilename);
            if (!exists)
            {
                VERBOSE(VB_NETWORK, QString("MythFlix: Copying boxshot file "
                                            "from server (%1)").arg(imageLoc));

                QString sURL = QString("http://cdn.nflximg.com/us/boxshots/"
                                       "large/%1").arg(imageLoc);

                if (!HttpComms::getHttpFile(sFilename, sURL, 20000))
                    VERBOSE(VB_NETWORK, QString("MythFlix: Failed to download "
                                                "image from: %1").arg(sURL));

                VERBOSE(VB_NETWORK, QString("MythFlix: Finished copying "
                                            "boxshot file from server "
                                            "(%1)").arg(imageLoc));
            }

            if (m_boxshotImage)
            {
                m_boxshotImage->SetFilename(sFilename);
                m_boxshotImage->Load();

                if (!m_boxshotImage->IsVisible())
                    m_boxshotImage->Show();
            }

        }
    }
    else {

        NewsSite *site = (NewsSite*) selected->getData();

        if (site)
        {

            if (m_titleText)
                m_titleText->SetText(site->name());

            if (m_descText)
                m_descText->SetText(site->description());

            if (m_boxshotImage && m_boxshotImage->IsVisible())
                m_boxshotImage->Hide();

        }
    }
}

bool MythFlix::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("NetFlix", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if((action == "SELECT") || (action == "MENU"))
            displayOptions();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
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

    MythListButtonItem *siteListItem = m_sitesList->GetItemCurrent();
    if (!siteListItem || !siteListItem->getData())
        return;

    if (site == (NewsSite*) siteListItem->getData()) {

        m_articlesList->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next()) {
            MythListButtonItem* item =
                new MythListButtonItem(m_articlesList, article->title());
            item->setData(article);
        }
    }
}

void MythFlix::slotShowNetFlixPage()
{

    MythListButtonItem *articleListItem = m_articlesList->GetItemCurrent();
    if (articleListItem && articleListItem->getData())
    {
        NewsArticle *article = (NewsArticle*) articleListItem->getData();
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

void MythFlix::slotSiteSelected(MythListButtonItem *item)
{
    if (!item || !item->getData())
        return;

    processAndShowNews((NewsSite*) item->getData());
}

void MythFlix::InsertMovieIntoQueue(QString queueName, bool atTop)
{
    MythListButtonItem *articleListItem = m_articlesList->GetItemCurrent();

    if (!articleListItem)
        return;

    NewsArticle *article = (NewsArticle*) articleListItem->getData();
    if(!article)
        return;

    QStringList args(gContext->GetShareDir() + "mythflix/scripts/netflix.pl");

    if (!queueName.isEmpty())
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
        args = QStringList(gContext->GetShareDir() +
                           "mythflix/scripts/netflix.pl");

        if (!queueName.isEmpty())
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
    QString label = tr("Browse Options");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "mythflixmenupopup");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "options");

    m_menuPopup->AddButton(tr("Add to Top of Queue"));
    m_menuPopup->AddButton(tr("Add to Bottom of Queue"));
    m_menuPopup->AddButton(tr("Show NetFlix Page"));
    m_menuPopup->AddButton(tr("Cancel"));

}

// Execute an external command and return results in string
//   probably should make this routing async vs polling like this
//   but it would require a lot more code restructuring
QString MythFlix::executeExternal(const QStringList& args, const QString& purpose)
{
    QString ret = "";
    QString err = "";

    VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose).
                      arg(args.join(" ")));
    Q3Process proc(args, this);

    QString cmd = args[0];
    QFileInfo info(cmd);

    if (!info.exists())
    {
       err = QString("\"%1\" failed: does not exist").arg(cmd);
    }
    else if (!info.isExecutable())
    {
       err = QString("\"%1\" failed: not executable").arg(cmd);
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
                    if (err.isEmpty())
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
                                  .arg(cmd);
                }

                break;
            }
        }
    }
    else
    {
        err = QString("\"%1\" failed: Could not start process")
                      .arg(cmd);
    }

    while (proc.canReadLineStdout() || proc.canReadLineStderr())
    {
        if (proc.canReadLineStdout())
        {
            ret += QString::fromLocal8Bit(proc.readLineStdout(),-1) + "\n";
        }

        if (proc.canReadLineStderr())
        {
            if (err.isEmpty())
            {
                err = cmd + ": ";
            }

            err += QString::fromLocal8Bit(proc.readLineStderr(), -1) + "\n";
        }
    }

    if (!err.isEmpty())
    {
        QString tempPurpose(purpose);

        if (tempPurpose.isEmpty())
            tempPurpose = "Command";

        VERBOSE(VB_IMPORTANT, QString("%1").arg(err));
//        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
//        QObject::tr(tempPurpose + " failed"), QObject::tr(err + "\n\nCheck NetFlix Settings"));
        ret = "#ERROR";
    }

    VERBOSE(VB_IMPORTANT, ret);
    return ret;
}

void MythFlix::customEvent(QEvent *event)
{

    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "options")
        {
            if (buttonnum == 0 || buttonnum == 1)
            {
                QString queueName = chooseQueue(this);
                if (!queueName.isEmpty())
                    InsertMovieIntoQueue(queueName, true);
            }
            else if (buttonnum == 2)
                slotShowNetFlixPage();
        }
        else if (resultid == "queues")
        {
            QString queueName = dce->GetResultText();
            if (!queueName.isEmpty())
                InsertMovieIntoQueue(queueName, true);
        }

        m_menuPopup = NULL;
    }

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
