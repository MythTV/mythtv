// C headers
#include <unistd.h>

// Qt headers
#include <q3network.h>
#include <Q3Process>
#include <QApplication>
#include <QDateTime>
#include <QDir>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/mythdirs.h>

#include "mythflixqueue.h"
#include "flixutil.h"

MythFlixQueue::MythFlixQueue(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name)
{
    q3InitNetworkProtocols ();

    // Setup cache directory

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythFlix";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    // Initialize variables
    m_zoom = QString("-z %1")
                   .arg(gContext->GetNumSetting("WebBrowserZoomLevel",200));
    m_browser = gContext->GetSetting("WebBrowserCommand",
                                   GetInstallPrefix() +
                                      "/bin/mythbrowser");
    m_articlesList = NULL;

    m_queueName = chooseQueue(this);
}

MythFlixQueue::~MythFlixQueue()
{
}

bool MythFlixQueue::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("netflix-ui.xml", "queue", this);

    if (!foundtheme)
        return false;

    m_articlesList = dynamic_cast<MythUIButtonList *>
                (GetChild("articleslist"));

    m_nameText = dynamic_cast<MythUIText *>
                (GetChild("queuename"));

    m_titleText = dynamic_cast<MythUIText *>
                (GetChild("title"));

    m_descText = dynamic_cast<MythUIText *>
                (GetChild("description"));

    m_boxshotImage = dynamic_cast<MythUIImage *>
                (GetChild("boxshot"));

    UpdateNameText();

    if (!m_articlesList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_articlesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(updateInfoView(MythUIButtonListItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_articlesList);

    if (!m_queueName.isEmpty())
        loadData();

    return true;
}

void MythFlixQueue::loadData()
{
    // Load sites from database

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT name, url, updated "
                      "FROM netflix "
                      "WHERE is_queue = :ISQUEUE "
                          "AND queue = :QUEUENAME "
                      "ORDER BY name");

    if (QString::compare("flixhistory",name())==0)
        query.bindValue(":ISQUEUE", 2);
    else if (QString::compare("flixqueue",name())==0)
        query.bindValue(":ISQUEUE", 1);
    else
        query.bindValue(":ISQUEUE", 1);

    if (m_queueName == "default")
        query.bindValue(":QUEUENAME", "");
    else
        query.bindValue(":QUEUENAME", m_queueName);

    if (!query.exec())
    {
        VERBOSE(VB_IMPORTANT,
                QString("MythFlixQueue: Error in loading queue from DB"));
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

    NewsSite* site = (NewsSite*) m_NewsSites.first();
    connect(site, SIGNAL(finished(NewsSite*)),
            this, SLOT(slotNewsRetrieved(NewsSite*)));

    slotRetrieveNews();

}

void MythFlixQueue::UpdateNameText()
{
    if (m_nameText)
    {
        QString queuename = m_queueName;
        if (queuename == "default")
            queuename = QObject::tr("Default");

        if (QString::compare("flixhistory",name())==0)
            m_nameText->SetText(tr("History for Queue: ") + queuename);
        else
            m_nameText->SetText(tr("Items in Queue: ") + queuename);
    }
}

QString MythFlixQueue::LoadPosterImage(const QString &location) const
{
    QString imageLoc = location;
    int length = imageLoc.length();
    int index = imageLoc.findRev("/");
    imageLoc = imageLoc.mid(index,length) + ".jpg";

    QString fileprefix = GetConfDir();

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

    return sFilename;
}

void MythFlixQueue::updateInfoView(MythUIButtonListItem* selected)
{
    NewsArticle *article = NULL;

    if (selected && !selected->GetData().isNull())
        article = qVariantValue<NewsArticle*>(selected->GetData());

    if (article)
    {

        if (m_titleText)
            m_titleText->SetText(article->title());

        if (m_descText)
            m_descText->SetText(article->description());

        if (m_boxshotImage)
        {
            QString posterImage = LoadPosterImage(article->articleURL());
            m_boxshotImage->SetFilename(posterImage);
            m_boxshotImage->Load();

            if (!m_boxshotImage->IsVisible())
                m_boxshotImage->Show();
        }

    }

}

bool MythFlixQueue::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("NetFlix", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "DELETE")
             slotRemoveFromQueue();
        else if (action == "MOVETOTOP")
             slotMoveToTop();
        else if((action == "SELECT") || (action == "MENU"))
            displayOptions();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythFlixQueue::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        site->retrieve();
    }
}

void MythFlixQueue::slotNewsRetrieved(NewsSite* site)
{
    processAndShowNews(site);
}

void MythFlixQueue::processAndShowNews(NewsSite* site)
{
    if (!site)
        return;

    site->process();

    if (site)
    {
        m_articlesList->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next())
        {
            MythUIButtonListItem* item =
                new MythUIButtonListItem(m_articlesList, article->title(),
                                         qVariantFromValue(article));
            QString posterImage = LoadPosterImage(article->articleURL());
            item->SetImage(posterImage);
            item->SetText(article->description(), "description");
        }
    }
}

void MythFlixQueue::slotMoveToTop()
{

    MythUIButtonListItem *articleListItem = m_articlesList->GetItemCurrent();

    if (articleListItem && !articleListItem->GetData().isNull())
    {
        NewsArticle *article =
                        qVariantValue<NewsArticle*>(articleListItem->GetData());
        if(article)
        {

            QStringList args(GetShareDir() +
                             "mythflix/scripts/netflix.pl");

            QString movieID(article->articleURL());
            int length = movieID.length();
            int index = movieID.findRev("/");
            movieID = movieID.mid(index+1,length);

            if (!m_queueName.isEmpty())
            {
                args += "-q";
                args += m_queueName;
            }

            args += "-1";
            args += movieID;

            // execute external command to obtain list of possible movie matches
            QString results = executeExternal(args, "Move To Top");

            slotRetrieveNews();
        }
    }

}

void MythFlixQueue::slotRemoveFromQueue()
{

    MythUIButtonListItem *articleListItem = m_articlesList->GetItemCurrent();

    if (articleListItem && !articleListItem->GetData().isNull())
    {
        NewsArticle *article =
                        qVariantValue<NewsArticle*>(articleListItem->GetData());
        if(article)
        {

            QStringList args(GetShareDir() +
                             "mythflix/scripts/netflix.pl");

            QString movieID(article->articleURL());
            int length = movieID.length();
            int index = movieID.findRev("/");
            movieID = movieID.mid(index+1,length);

            if (!m_queueName.isEmpty())
            {
                args += "-q";
                args += m_queueName;
            }

            args += "-R";
            args += movieID;

            // execute external command to obtain list of possible movie matches
            QString results = executeExternal(args, "Remove From Queue");

            slotRetrieveNews();

        }
    }

}

void MythFlixQueue::slotMoveToQueue()
{

    MythUIButtonListItem *articleListItem = m_articlesList->GetItemCurrent();

    if (articleListItem && !articleListItem->GetData().isNull())
    {
        NewsArticle *article =
                        qVariantValue<NewsArticle*>(articleListItem->GetData());
        if(article)
        {

            QString newQueue = chooseQueue(this, m_queueName);

            if (newQueue.isEmpty())
            {
//                 MythPopupBox::showOkPopup(
//                     gContext->GetMainWindow(), tr("Move Canceled"),
//                     tr("Item not moved."));
                return;
            }

            QStringList base(GetShareDir() +
                             "mythflix/scripts/netflix.pl");

            QString movieID(article->articleURL());
            int length = movieID.length();
            int index = movieID.findRev("/");
            movieID = movieID.mid(index+1,length);

            QStringList args = base;
            QString results;

            if (!newQueue.isEmpty())
            {
                args += "-q";
                args += newQueue;
            }

            args += "-A";
            args += movieID;

            results = executeExternal(args, "Add To Queue");

            args = base;

            if (!m_queueName.isEmpty())
            {
                args += "-q";
                args += m_queueName;
            }

            args += "-R";
            args += movieID;

            results = executeExternal(args, "Remove From Queue");

            slotRetrieveNews();
        }
    }
}

void MythFlixQueue::slotShowNetFlixPage()
{

    MythUIButtonListItem *articleListItem = m_articlesList->GetItemCurrent();
    if (articleListItem && !articleListItem->GetData().isNull())
    {
        NewsArticle *article =
                        qVariantValue<NewsArticle*>(articleListItem->GetData());
        if(article)
        {
            QString cmdUrl(article->articleURL());
            cmdUrl.replace('\'', "%27");

            QString cmd = QString("%1 %2 '%3'")
                                  .arg(m_browser).arg(m_zoom).arg(cmdUrl);
            VERBOSE(VB_GENERAL,
                  QString("MythFlixQueue: Opening Neflix site: (%1)").arg(cmd));
            myth_system(cmd);
        }
    }
}

void MythFlixQueue::displayOptions()
{

    QString label = tr("Manage Queue");

    MythScreenStack *mainStack =
                            GetMythMainWindow()->GetMainStack();

    m_menuPopup = new MythDialogBox(label, mainStack, "flixqueuepopup");

    if (m_menuPopup->Create())
        mainStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "manage");

    m_menuPopup->AddButton(tr("Top Of Queue"));
    m_menuPopup->AddButton(tr("Remove From Queue"));
    if (!m_queueName.isEmpty())
        m_menuPopup->AddButton(tr("Move To Another Queue"));
    m_menuPopup->AddButton(tr("Show NetFlix Page"));
    m_menuPopup->AddButton(tr("Cancel"));

}

// Execute an external command and return results in string
//   probably should make this routing async vs polling like this
//   but it would require a lot more code restructuring
QString MythFlixQueue::executeExternal(const QStringList& args, const QString& purpose)
{
    QString ret = "";
    QString err = "";

    VERBOSE(VB_GENERAL, QString("%1: Executing '%2'")
            .arg(purpose).arg(args.join(" ")));

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
//         MythPopupBox::showOkPopup(gContext->GetMainWindow(),
//         QObject::tr(tempPurpose + " failed"), QObject::tr(err + "\n\nCheck NetFlix Settings"));
        ret = "#ERROR";
    }

    VERBOSE(VB_IMPORTANT, ret);
    return ret;
}

void MythFlixQueue::customEvent(QEvent *event)
{

    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "manage")
        {

            if (buttonnum == 0)
                slotMoveToTop();
            else if (buttonnum == 1)
                slotRemoveFromQueue();
            else if (buttonnum == 2)
            {
                if (!m_queueName.isEmpty())
                    slotMoveToQueue();
                else
                    slotShowNetFlixPage();
            }
            else if (buttonnum == 3)
            {
                if (!m_queueName.isEmpty())
                    slotShowNetFlixPage();
            }

        }
        else if (resultid == "queues")
        {
            QString queueName = dce->GetResultText();
            if (!queueName.isEmpty())
            {
                m_queueName = queueName;
                UpdateNameText();
                loadData();
            }
        }

        m_menuPopup = NULL;
    }

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
