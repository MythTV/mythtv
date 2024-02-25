#include <QDomDocument>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/netutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibuttonlist.h>

// Tree headers
#include "netcommon.h"
#include "neteditorbase.h"

#define LOC      QString("NetEditorBase: ")
#define LOC_WARN QString("NetEditorBase, Warning: ")
#define LOC_ERR  QString("NetEditorBase, Error: ")

/** \brief Creates a new NetEditorBase Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
NetEditorBase::NetEditorBase(MythScreenStack *parent,
                             const QString &name) :
    MythScreenType(parent, name)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

NetEditorBase::~NetEditorBase()
{
    if (m_manager)
    {
        m_manager->disconnect();
        m_manager->deleteLater();
        m_manager = nullptr;
    }

    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_changed)
        emit ItemsChanged();
}

bool NetEditorBase::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("netvision-ui.xml", "treeeditor", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_grabbers, "grabbers", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'treeeditor'");
        return false;
    }

    connect(m_grabbers, &MythUIButtonList::itemClicked,
            this, &NetEditorBase::ToggleItem);

    BuildFocusList();

    LoadData();

    return true;
}

void NetEditorBase::LoadData()
{
    QString msg = tr("Querying Backend for Internet Content Sources...");
    CreateBusyDialog(msg);

    m_manager = new QNetworkAccessManager();

    connect(m_manager, &QNetworkAccessManager::finished,
                       this, &NetEditorBase::SlotLoadedData);

    QUrl url(GetMythXMLURL() + "GetInternetSources");
    m_reply = m_manager->get(QNetworkRequest(url));
}

bool NetEditorBase::keyPressEvent(QKeyEvent *event)
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

void NetEditorBase::SlotLoadedData()
{
    QDomDocument doc;
    doc.setContent(m_reply->readAll());
    QDomElement root = doc.documentElement();
    QDomElement content = root.firstChildElement("InternetContent");
    QDomElement grabber = content.firstChildElement("grabber");

    while (!grabber.isNull())
    {
        QString title;
        QString author;
        QString image;
        QString description;
        QString type;
        QString commandline;
        bool search = false;
        bool tree = false;

        title = grabber.firstChildElement("name").text();
        commandline = grabber.firstChildElement("command").text();
        author = grabber.firstChildElement("author").text();
        image = grabber.firstChildElement("thumbnail").text();
        type = grabber.firstChildElement("type").text();
        description = grabber.firstChildElement("description").text();
        double version = grabber.firstChildElement("version").text().toDouble();

        QString searchstring = grabber.firstChildElement("search").text();

        if (!searchstring.isEmpty() &&
            (searchstring.toLower() == "true" ||
             searchstring.toLower() == "yes" ||
             searchstring == "1"))
            search = true;

        QString treestring = grabber.firstChildElement("tree").text();

        if (!treestring.isEmpty() &&
            (treestring.toLower() == "true" ||
             treestring.toLower() == "yes" ||
             treestring == "1"))
            tree = true;

        if (type.toLower() == "video" && Matches(search, tree))
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Found Source %1...").arg(title));
            m_grabberList.append(new GrabberScript(title, image, VIDEO_FILE,
                                                   author, search, tree,
                                                   description, commandline,
                                                   version));
        }

        grabber = grabber.nextSiblingElement("grabber");
    }

    ParsedData();
 }

void NetEditorBase::ParsedData()
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = nullptr;
    }

    FillGrabberButtonList();
}

void NetEditorBase::CreateBusyDialog(const QString& title)
{
    if (m_busyPopup)
        return;

    const QString& message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
    else
    {
        delete m_busyPopup;
        m_busyPopup = nullptr;
    }
}

void NetEditorBase::FillGrabberButtonList()
{
    for (const auto & g : std::as_const(m_grabberList))
    {
        auto *item = new MythUIButtonListItem(m_grabbers, g->GetTitle());
        item->SetText(g->GetTitle(), "title");
        item->SetData(QVariant::fromValue(g));
        const QString& img = g->GetImage();
        QString thumb;

        if (!img.startsWith("/") && !img.isEmpty())
        {
            thumb = QString("%1mythnetvision/icons/%2")
                .arg(GetShareDir(), g->GetImage());
        }
        else
        {
            thumb = img;
        }

        item->SetImage(thumb);
        item->setCheckable(true);
        item->setChecked(MythUIButtonListItem::NotChecked);
        QFileInfo fi(g->GetCommandline());

        if (FindGrabberInDB(fi.fileName()))
            item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void NetEditorBase::ToggleItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *script = item->GetData().value<GrabberScript*>();
    if (!script)
        return;

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked)
    {
        if (InsertInDB(script))
        {
            m_changed = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else if (RemoveFromDB(script))
    {
        m_changed = true;
        item->setChecked(MythUIButtonListItem::NotChecked);
    }
}
