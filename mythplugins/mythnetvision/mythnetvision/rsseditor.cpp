// Qt
#include <QDateTime>
#include <QDomDocument>
#include <QImageReader>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythdownloadmanager.h>
#include <libmythbase/mythsorthelper.h>
#include <libmythbase/netutils.h>
#include <libmythbase/rssparse.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuifilebrowser.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

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
        for (const auto & ext : std::as_const(exts))
            ret.append(QString("*.").append(ext));

        return ret;
    }
}

RSSEditPopup::~RSSEditPopup()
{
    if (m_manager)
    {
        m_manager->disconnect();
        m_manager->deleteLater();
        m_manager = nullptr;
    }
}

bool RSSEditPopup::Create(void)
{
    // Load the theme for this screen
    bool foundtheme =
        LoadWindowFromXML("netvision-ui.xml", "rsseditpopup", this);

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

    connect(m_okButton, &MythUIButton::Clicked, this, &RSSEditPopup::ParseAndSave);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_thumbButton, &MythUIButton::Clicked, this, &RSSEditPopup::DoFileBrowser);

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

        if (m_site->GetDownload())
            m_download->SetCheckState(MythUIStateType::Full);
    }

    BuildFocusList();

    return true;
}

bool RSSEditPopup::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Internet Video",
                                                          event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RSSEditPopup::ParseAndSave(void)
{
    if (m_editing)
    {
        QString title = m_titleEdit->GetText();
        QString desc = m_descEdit->GetText();
        QString author = m_authorEdit->GetText();
        QString link = m_urlEdit->GetText();
        QString filename = m_thumbImage->GetFilename();
        bool download = m_download->GetCheckState() == MythUIStateType::Full;

        removeFromDB(m_urlText, VIDEO_PODCAST);

        std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
        RSSSite site(title, sh->doTitle(title), filename, VIDEO_PODCAST,
                     desc, link, author, download, MythDate::current());
        if (insertInDB(&site))
            emit Saving();
        Close();
    }
    else
    {
        m_manager = new QNetworkAccessManager();
        QUrl qurl(m_urlEdit->GetText());

        m_reply = m_manager->get(QNetworkRequest(qurl));

        connect(m_manager, &QNetworkAccessManager::finished, this,
                           &RSSEditPopup::SlotCheckRedirect);
    }
}

QUrl RSSEditPopup::redirectUrl(const QUrl& possibleRedirectUrl,
                               const QUrl& oldRedirectUrl)
{
    QUrl redirectUrl;
    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;
    return redirectUrl;
}

void RSSEditPopup::SlotCheckRedirect(QNetworkReply* reply)
{
    QVariant possibleRedirectUrl =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    QUrl urlRedirectedTo;
    urlRedirectedTo = redirectUrl(
        possibleRedirectUrl.toUrl(), urlRedirectedTo);

    if (!urlRedirectedTo.isEmpty())
    {
        m_urlEdit->SetText(urlRedirectedTo.toString());
        m_manager->get(QNetworkRequest(urlRedirectedTo));
    }
    else
    {
//        urlRedirectedTo.clear();
        SlotSave(reply);
    }
    reply->deleteLater();
}

void RSSEditPopup::SlotSave(QNetworkReply* reply)
{
    QDomDocument document;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    document.setContent(reply->read(reply->bytesAvailable()), true);
#else
    document.setContent(reply->read(reply->bytesAvailable()),
                        QDomDocument::ParseOption::UseNamespaceProcessing);
#endif

    QString text = document.toString();

    QString title = m_titleEdit->GetText();
    QString description = m_descEdit->GetText();
    QString author = m_authorEdit->GetText();
    QString file = m_thumbImage->GetFilename();

    LOG(VB_GENERAL, LOG_DEBUG, QString("Text to Parse: %1").arg(text));

    QDomElement root = document.documentElement();
    QDomElement channel = root.firstChildElement ("channel");
    if (!channel.isNull())
    {
        Parse parser;
        if (title.isEmpty())
            title = channel.firstChildElement("title").text().trimmed();
        if (description.isEmpty())
            description = channel.firstChildElement("description").text();
        if (author.isEmpty())
            author = Parse::GetAuthor(channel);
        if (author.isEmpty())
            author = channel.firstChildElement("managingEditor").text();
        if (author.isEmpty())
            author = channel.firstChildElement("webMaster").text();

        QString thumbnailURL =
            channel.firstChildElement("image").attribute("url");
        if (thumbnailURL.isEmpty())
        {
            QDomElement thumbElem = channel.firstChildElement("image");
            if (!thumbElem.isNull())
                thumbnailURL = thumbElem.firstChildElement("url").text();
        }
        if (thumbnailURL.isEmpty())
        {
            QDomNodeList nodes = channel.elementsByTagNameNS(
                           "http://www.itunes.com/dtds/podcast-1.0.dtd",
                           "image");
            if (nodes.size())
            {
                thumbnailURL =
                    nodes.at(0).toElement().attributeNode("href").value();
                if (thumbnailURL.isEmpty())
                    thumbnailURL = nodes.at(0).toElement().text();
            }
        }

        bool download = m_download->GetCheckState() == MythUIStateType::Full;
        QString filename("");

        if (!file.isEmpty())
            filename = file;
        else if (!thumbnailURL.isEmpty())
            filename = thumbnailURL;

        QString link = m_urlEdit->GetText();

        std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
        RSSSite site(title, sh->doTitle(title), filename, VIDEO_PODCAST, description, link,
                     author, download, MythDate::current());
        if (insertInDB(&site))
            emit Saving();
    }

    Close();
}

void RSSEditPopup::DoFileBrowser()
{
    SelectImagePopup(GetConfDir() + "/MythNetvision" + "/sitecovers", *this,
                     CEID_NEWIMAGE);
}

void RSSEditPopup::SelectImagePopup(const QString &prefix,
                            QObject &inst, const QString &returnEvent)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *fb = new MythUIFileBrowser(popupStack, prefix);
    fb->SetNameFilter(GetSupportedImageExtensionFilter());
    if (fb->Create())
    {
        fb->SetReturnEvent(&inst, returnEvent);
        popupStack->AddScreen(fb);
    }
    else
    {
        delete fb;
    }
}

void RSSEditPopup::customEvent(QEvent *levent)
{
    if (levent->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(levent);
        if ((dce != nullptr) && (dce->GetId() == CEID_NEWIMAGE))
        {
            m_thumbImage->SetFilename(dce->GetResultText());
            m_thumbImage->Load();
            m_thumbImage->Show();
        }
    }
}

RSSEditor::~RSSEditor()
{
    QMutexLocker locker(&m_lock);

    if (m_changed)
        emit ItemsChanged();
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

    connect(m_sites, &MythUIButtonList::itemClicked,
            this, &RSSEditor::SlotEditSite);

    connect(m_delete, &MythUIButton::Clicked,
            this, &RSSEditor::SlotDeleteSite);
    connect(m_edit, &MythUIButton::Clicked,
            this, &RSSEditor::SlotEditSite);
    connect(m_new, &MythUIButton::Clicked,
            this, &RSSEditor::SlotNewSite);

    connect(m_sites, &MythUIButtonList::itemSelected,
            this, &RSSEditor::SlotItemChanged);

    BuildFocusList();

    LoadData();

    if (m_sites->GetCount() == 0)
        SetFocusWidget(m_new);
    else
        SlotItemChanged();

    return true;
}

void RSSEditor::LoadData()
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

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Internet Video",
                                                          event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "DELETE" && GetFocusWidget() == m_sites)
        {
            SlotDeleteSite();
        }
        else if (action == "EDIT" && GetFocusWidget() == m_sites)
        {
            SlotEditSite();
        }
        else
        {
            handled = false;
        }
    }


    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RSSEditor::fillRSSButtonList()
{
    QMutexLocker locker(&m_lock);

    m_sites->Reset();

    for (const auto & site : std::as_const(m_siteList))
    {
        auto *item = new MythUIButtonListItem(m_sites, site->GetTitle());
        item->SetText(site->GetTitle(), "title");
        item->SetText(site->GetDescription(), "description");
        item->SetText(site->GetURL(), "url");
        item->SetText(site->GetAuthor(), "author");
        item->SetData(QVariant::fromValue(site));
        item->SetImage(site->GetImage());
    }
}

void RSSEditor::SlotItemChanged()
{
    auto *site = m_sites->GetItemCurrent()->GetData().value<RSSSite*>();
    if (site)
    {
        if (m_image)
        {
            const QString& thumb = site->GetImage();

            m_image->Reset();

            if (!thumb.isEmpty())
            {
                m_image->SetFilename(thumb);
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

void RSSEditor::SlotDeleteSite()
{
    QMutexLocker locker(&m_lock);

    QString message =
        tr("Are you sure you want to unsubscribe from this feed?");

    MythScreenStack *m_popupStack =
        GetMythMainWindow()->GetStack("popup stack");

    auto *confirmdialog = new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
    {
        m_popupStack->AddScreen(confirmdialog);

        connect(confirmdialog, &MythConfirmationDialog::haveResult,
                this, &RSSEditor::DoDeleteSite);
    }
    else
    {
        delete confirmdialog;
    }
}

void RSSEditor::SlotEditSite()
{
    QMutexLocker locker(&m_lock);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *site = m_sites->GetItemCurrent()->GetData().value<RSSSite*>();
    if (site)
    {
        auto *rsseditpopup =
            new RSSEditPopup(site->GetURL(), true, mainStack, "rsseditpopup");

        if (rsseditpopup->Create())
        {
            connect(rsseditpopup, &RSSEditPopup::Saving, this, &RSSEditor::ListChanged);

            mainStack->AddScreen(rsseditpopup);
        }
        else
        {
            delete rsseditpopup;
        }
    }
}

void RSSEditor::SlotNewSite()
{
    QMutexLocker locker(&m_lock);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *rsseditpopup = new RSSEditPopup("", false, mainStack, "rsseditpopup");

    if (rsseditpopup->Create())
    {
        connect(rsseditpopup, &RSSEditPopup::Saving, this, &RSSEditor::ListChanged);

        mainStack->AddScreen(rsseditpopup);
    }
    else
    {
        delete rsseditpopup;
    }
}

void RSSEditor::DoDeleteSite(bool remove)
{
    QMutexLocker locker(&m_lock);

    if (!remove)
        return;

    auto *site = m_sites->GetItemCurrent()->GetData().value<RSSSite*>();

    if (removeFromDB(site))
        ListChanged();
}

void RSSEditor::ListChanged()
{
    m_changed = true;
    LoadData();
}
