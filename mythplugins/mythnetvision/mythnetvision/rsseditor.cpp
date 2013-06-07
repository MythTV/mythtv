#include <QDomDocument>
#include <QDateTime>
#include <QImageReader>

// MythTV headers
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuitextedit.h>
#include <mythuicheckbox.h>
#include <mythuifilebrowser.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythdate.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <httpcomms.h>
#include <mythdirs.h>
#include <netutils.h>
#include <rssparse.h>

// RSS headers
#include "rsseditor.h"

#define LOC      QString("RSSEditor: ")
#define LOC_WARN QString("RSSEditor, Warning: ")
#define LOC_ERR  QString("RSSEditor, Error: ")

namespace
{
    const QString CEID_NEWIMAGE = "image";

    QStringList GetSupportedImageExtensionFilter()
    {
        QStringList ret;

        QList<QByteArray> exts = QImageReader::supportedImageFormats();
        for (QList<QByteArray>::iterator p = exts.begin(); p != exts.end(); ++p)
        {
            ret.append(QString("*.").append(*p));
        }

        return ret;
    }
}

/** \brief Creates a new RSS Edit Popup
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
RSSEditPopup::RSSEditPopup(
    const QString &url, bool edit,
    MythScreenStack *parent, const QString &name) :
    MythScreenType(parent, name),
    m_site(NULL),
    m_urlText(url),        m_editing(edit),
    m_thumbImage(NULL),    m_thumbButton(NULL),
    m_urlEdit(NULL),       m_titleEdit(NULL),
    m_descEdit(NULL),      m_authorEdit(NULL),
    m_okButton(NULL),      m_cancelButton(NULL),
    m_download(NULL),      m_manager(NULL),
    m_reply(NULL)
{
}

RSSEditPopup::~RSSEditPopup()
{
    if (m_manager)
    {
        m_manager->disconnect();
        m_manager->deleteLater();
        m_manager = NULL;
    }
}

bool RSSEditPopup::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("netvision-ui.xml", "rsseditpopup", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_urlEdit, "url", &err);
    UIUtilE::Assign(this, m_titleEdit, "title", &err);
    UIUtilE::Assign(this, m_descEdit, "description", &err);
    UIUtilE::Assign(this, m_authorEdit, "author", &err);
    UIUtilE::Assign(this, m_download, "download", &err);
    UIUtilE::Assign(this, m_okButton, "ok", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);
    UIUtilE::Assign(this, m_thumbButton, "preview_browse", &err);
    UIUtilE::Assign(this, m_thumbImage, "preview", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'rsseditpopup'");
        return false;
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(parseAndSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_thumbButton, SIGNAL(Clicked()), this, SLOT(doFileBrowser()));

    m_urlEdit->SetMaxLength(0);
    m_titleEdit->SetMaxLength(255);
    m_descEdit->SetMaxLength(0);
    m_authorEdit->SetMaxLength(255);

    if (m_editing)
    {
        m_site = findByURL(m_urlText, VIDEO_PODCAST);

        m_urlEdit->SetText(m_urlText);
        m_titleEdit->SetText(m_site->GetTitle());
        m_descEdit->SetText(m_site->GetDescription());
        m_authorEdit->SetText(m_site->GetAuthor());

        QString thumb = m_site->GetImage();
        if (!thumb.isEmpty())
        {
            m_thumbImage->SetFilename(thumb);
            m_thumbImage->Load();
        }

        if (m_site->GetDownload() == 1)
           m_download->SetCheckState(MythUIStateType::Full);
    }

    BuildFocusList();

    return true;
}

bool RSSEditPopup::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Internet Video", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RSSEditPopup::parseAndSave(void)
{
    if (m_editing)
    {
        QString title = m_titleEdit->GetText();
        QString desc = m_descEdit->GetText();
        QString author = m_authorEdit->GetText();
        QString link = m_urlEdit->GetText();
        QString filename = m_thumbImage->GetFilename();

        bool download;
        if (m_download->GetCheckState() == MythUIStateType::Full)
            download = true;
        else
            download = false;

        removeFromDB(m_urlText, VIDEO_PODCAST);

        if (insertInDB(new RSSSite(title, filename, VIDEO_PODCAST,
                desc, link, author, download, MythDate::current())))
            emit saving();
        Close();
    }
    else
    {
        m_manager = new QNetworkAccessManager();
        QUrl qurl(m_urlEdit->GetText());

        m_reply = m_manager->get(QNetworkRequest(qurl));

        connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                           SLOT(slotCheckRedirect(QNetworkReply*)));
    }
}

QUrl RSSEditPopup::redirectUrl(const QUrl& possibleRedirectUrl,
                               const QUrl& oldRedirectUrl) const
{
    QUrl redirectUrl;
    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;
    return redirectUrl;
}

void RSSEditPopup::slotCheckRedirect(QNetworkReply* reply)
{
    QVariant possibleRedirectUrl =
         reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    QUrl urlRedirectedTo;
    urlRedirectedTo = redirectUrl(
        possibleRedirectUrl.toUrl(), urlRedirectedTo);

    if(!urlRedirectedTo.isEmpty())
    {
        m_urlEdit->SetText(urlRedirectedTo.toString());
        m_manager->get(QNetworkRequest(urlRedirectedTo));
    }
    else
    {
//        urlRedirectedTo.clear();
        slotSave(reply);
    }
    reply->deleteLater();
}

void RSSEditPopup::slotSave(QNetworkReply* reply)
{
    QDomDocument document;
    document.setContent(reply->read(reply->bytesAvailable()), true);

    QString text = document.toString();

    QString title = m_titleEdit->GetText();
    QString description = m_descEdit->GetText();
    QString author = m_authorEdit->GetText();
    QString file = m_thumbImage->GetFilename();

    LOG(VB_GENERAL, LOG_DEBUG, QString("Text to Parse: %1").arg(text));

    QDomElement root = document.documentElement();
    QDomElement channel = root.firstChildElement ("channel");
    if (!channel.isNull ())
    {
        Parse parser;
        if (title.isEmpty())
            title = channel.firstChildElement("title").text().trimmed();
        if (description.isEmpty())
            description = channel.firstChildElement("description").text();
        if (author.isEmpty())
            author = parser.GetAuthor(channel);
        if (author.isEmpty())
            author = channel.firstChildElement("managingEditor").text();
        if (author.isEmpty())
            author = channel.firstChildElement("webMaster").text();

        QString thumbnailURL = channel.firstChildElement("image").attribute("url");
        if (thumbnailURL.isEmpty())
        {
            QDomElement thumbElem = channel.firstChildElement("image");
            if (!thumbElem.isNull())
                thumbnailURL = thumbElem.firstChildElement("url").text();
        }
        if (thumbnailURL.isEmpty())
        {
            QDomNodeList nodes = channel.elementsByTagNameNS(
                           "http://www.itunes.com/dtds/podcast-1.0.dtd", "image");
            if (nodes.size())
            {
                thumbnailURL = nodes.at(0).toElement().attributeNode("href").value();
                if (thumbnailURL.isEmpty())
                    thumbnailURL = nodes.at(0).toElement().text();
            }
        }

        bool download;
        if (m_download->GetCheckState() == MythUIStateType::Full)
            download = true;
        else
            download = false;

        QDateTime updated = MythDate::current();
        QString filename("");

        if (file.isEmpty())
            filename = file;

        QString link = m_urlEdit->GetText();

        if (!thumbnailURL.isEmpty() && filename.isEmpty())
        {
            QString fileprefix = GetConfDir();

            QDir dir(fileprefix);
            if (!dir.exists())
                    dir.mkdir(fileprefix);

            fileprefix += "/MythNetvision";

            dir = QDir(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            fileprefix += "/sitecovers";

            dir = QDir(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            QFileInfo fi(thumbnailURL);
            QString rawFilename = fi.fileName();

            filename = QString("%1/%2").arg(fileprefix).arg(rawFilename);

            bool exists = QFile::exists(filename);
            if (!exists)
                HttpComms::getHttpFile(filename, thumbnailURL, 20000, 1, 2);
        }
        if (insertInDB(new RSSSite(title, filename, VIDEO_PODCAST, description, link,
                author, download, MythDate::current())))
            emit saving();
    }
    Close();
}

void RSSEditPopup::doFileBrowser()
{
    SelectImagePopup(GetConfDir() + "/MythNetvision" + "/sitecovers", *this, CEID_NEWIMAGE);
}

void RSSEditPopup::SelectImagePopup(const QString &prefix,
                            QObject &inst, const QString &returnEvent)
{
        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, prefix);
        fb->SetNameFilter(GetSupportedImageExtensionFilter());
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
            delete fb;
}

void RSSEditPopup::customEvent(QEvent *levent)
{
    if (levent->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(levent);
        if (dce->GetId() == CEID_NEWIMAGE)
        {
            m_thumbImage->SetFilename(dce->GetResultText());
            m_thumbImage->Load();
            m_thumbImage->Show();
        }
    }
}

RSSEditor::RSSEditor(MythScreenStack *parent, const QString &name) :
    MythScreenType(parent, name), m_lock(QMutex::Recursive),
    m_changed(false), m_sites(NULL), m_new(NULL), m_delete(NULL), m_edit(NULL),
    m_image(NULL), m_title(NULL), m_url(NULL), m_desc(NULL), m_author(NULL)
{
}

RSSEditor::~RSSEditor()
{
    QMutexLocker locker(&m_lock);

    if (m_changed)
        emit itemsChanged();
}

bool RSSEditor::Create(void)
{
    QMutexLocker locker(&m_lock);

    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("netvision-ui.xml", "rsseditor", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_sites, "sites", &err);
    UIUtilE::Assign(this, m_new, "new", &err);
    UIUtilE::Assign(this, m_delete, "delete", &err);
    UIUtilE::Assign(this, m_edit, "edit", &err);

    UIUtilW::Assign(this, m_image, "preview");
    UIUtilW::Assign(this, m_title, "title");
    UIUtilW::Assign(this, m_desc, "description");
    UIUtilW::Assign(this, m_url, "url");
    UIUtilW::Assign(this, m_author, "author");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'rsseditor'");
        return false;
    }

    connect(m_sites, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(slotEditSite(void)));

    connect(m_delete, SIGNAL(Clicked(void)),
            SLOT(slotDeleteSite(void)));
    connect(m_edit, SIGNAL(Clicked(void)),
            SLOT(slotEditSite(void)));
    connect(m_new, SIGNAL(Clicked(void)),
            SLOT(slotNewSite(void)));

    connect(m_sites, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(slotItemChanged(void)));

    BuildFocusList();

    loadData();

    if (m_sites->GetCount() == 0)
        SetFocusWidget(m_new);
    else
        slotItemChanged();

    return true;
}

void RSSEditor::loadData()
{
    qDeleteAll(m_siteList);
    m_siteList = findAllDBRSS();
    fillRSSButtonList();
    if (m_sites->GetCount() == 0)
    {
        m_edit->SetVisible(false);
        m_delete->SetVisible(false);
        m_sites->SetVisible(false);
    }
    else
    {
        m_edit->SetVisible(true);
        m_delete->SetVisible(true);
        m_sites->SetVisible(true);
    }
}

bool RSSEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Internet Video", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "DELETE" && GetFocusWidget() == m_sites)
        {
            slotDeleteSite();
        }
        else if (action == "EDIT" && GetFocusWidget() == m_sites)
        {
            slotEditSite();
        }
        else
            handled = false;
    }


    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RSSEditor::fillRSSButtonList()
{
    QMutexLocker locker(&m_lock);

    m_sites->Reset();

    for (RSSSite::rssList::iterator i = m_siteList.begin();
            i != m_siteList.end(); ++i)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_sites, (*i)->GetTitle());
        if (item)
        {
            item->SetText((*i)->GetTitle(), "title");
            item->SetText((*i)->GetDescription(), "description");
            item->SetText((*i)->GetURL(), "url");
            item->SetText((*i)->GetAuthor(), "author");
            item->SetData(qVariantFromValue(*i));
            item->SetImage((*i)->GetImage());
        }
    }
}

void RSSEditor::slotItemChanged()
{
    RSSSite *site = qVariantValue<RSSSite *>(m_sites->GetItemCurrent()->GetData());

    if (site)
    {
        if (m_image)
        {
            QString thumb = site->GetImage();

            m_image->Reset();

            if (!thumb.isEmpty())
            {
                m_image->SetFilename(site->GetImage());
                m_image->Load();
            }
        }
        if (m_title)
            m_title->SetText(site->GetTitle());
        if (m_desc)
            m_desc->SetText(site->GetDescription());
        if (m_url)
            m_url->SetText(site->GetURL());
        if (m_author)
            m_author->SetText(site->GetAuthor());
    }
}

void RSSEditor::slotDeleteSite()
{
    QMutexLocker locker(&m_lock);

    QString message = tr("Are you sure you want to unsubscribe from this feed?");

    MythScreenStack *m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
    {
        m_popupStack->AddScreen(confirmdialog);

        connect(confirmdialog, SIGNAL(haveResult(bool)),
                SLOT(doDeleteSite(bool)));
    }
    else
        delete confirmdialog;
}

void RSSEditor::slotEditSite()
{
    QMutexLocker locker(&m_lock);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RSSSite *site = qVariantValue<RSSSite *>(m_sites->GetItemCurrent()->GetData());

    if (site)
    {
        RSSEditPopup *rsseditpopup = new RSSEditPopup(site->GetURL(), true, mainStack, "rsseditpopup");

        if (rsseditpopup->Create())
        {
            connect(rsseditpopup, SIGNAL(saving()), this,
                           SLOT(listChanged()));

            mainStack->AddScreen(rsseditpopup);
        }
        else
        {
            delete rsseditpopup;
        }
    }
}

void RSSEditor::slotNewSite()
{
    QMutexLocker locker(&m_lock);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RSSEditPopup *rsseditpopup = new RSSEditPopup("", false, mainStack, "rsseditpopup");

    if (rsseditpopup->Create())
    {
        connect(rsseditpopup, SIGNAL(saving()), this,
                       SLOT(listChanged()));

        mainStack->AddScreen(rsseditpopup);
    }
    else
    {
        delete rsseditpopup;
    }
}

void RSSEditor::doDeleteSite(bool remove)
{
    QMutexLocker locker(&m_lock);

    if (!remove)
        return;

    RSSSite *site = qVariantValue<RSSSite *>(m_sites->GetItemCurrent()->GetData());

    if (removeFromDB(site))
        listChanged();
}

void RSSEditor::listChanged()
{
    m_changed = true;
    loadData();
}
