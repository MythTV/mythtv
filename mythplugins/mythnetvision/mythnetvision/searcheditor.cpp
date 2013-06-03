#include <iostream>
#include <set>
#include <map>

#include <QImageReader>

// MythTV headers
#include <mythuibuttonlist.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <netutils.h>
#include <netgrabbermanager.h>

// Search headers
#include "searcheditor.h"
#include "netcommon.h"

#define LOC      QString("SearchEditor: ")
#define LOC_WARN QString("SearchEditor, Warning: ")
#define LOC_ERR  QString("SearchEditor, Error: ")

/** \brief Creates a new SearchEditor Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
SearchEditor::SearchEditor(MythScreenStack *parent,
                          const QString name) :
    MythScreenType(parent, name),
    m_grabbers(NULL),
    m_busyPopup(NULL),
    m_popupStack(),
    m_manager(NULL),
    m_reply(NULL),
    m_changed(false)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

SearchEditor::~SearchEditor()
{
    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_manager)
    {
        m_manager->disconnect();
        m_manager->deleteLater();
        m_manager = NULL;
    }

    if (m_changed)
        emit itemsChanged();
}

bool SearchEditor::Create(void)
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

    connect(m_grabbers, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(toggleItem(MythUIButtonListItem*)));

    BuildFocusList();

    loadData();

    return true;
}

void SearchEditor::loadData()
{
    QString msg = tr("Querying Backend for Internet Content Sources...");
    createBusyDialog(msg);

    m_manager = new QNetworkAccessManager();

    connect(m_manager, SIGNAL(finished(QNetworkReply*)),
                       SLOT(slotLoadedData(void)));

    QUrl url(GetMythXMLURL() + "GetInternetSources");
    m_reply = m_manager->get(QNetworkRequest(url));
}

void SearchEditor::slotLoadedData()
{
    QDomDocument doc;
    doc.setContent(m_reply->readAll());
    QDomElement root = doc.documentElement();
    QDomElement content = root.firstChildElement("InternetContent");
    QDomElement grabber = content.firstChildElement("grabber");

    while (!grabber.isNull())
    {
        QString title, author, image, description, type, commandline;
        double version;
        bool search = false;
        bool tree = false;

        title = grabber.firstChildElement("name").text();
        commandline = grabber.firstChildElement("command").text();
        author = grabber.firstChildElement("author").text();
        image = grabber.firstChildElement("thumbnail").text();
        type = grabber.firstChildElement("type").text();
        description = grabber.firstChildElement("description").text();
        version = grabber.firstChildElement("version").text().toDouble();

        QString treestring = grabber.firstChildElement("tree").text();

        if (!treestring.isEmpty() && (treestring.toLower() == "true" ||
            treestring.toLower() == "yes" || treestring == "1"))
            tree = true;

        QString searchstring = grabber.firstChildElement("search").text();
        
        if (!searchstring.isEmpty() && (searchstring.toLower() == "true" ||
            searchstring.toLower() == "yes" || searchstring == "1"))
            search = true;

        if (type.toLower() == "video" && search)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Found Search Source %1...").arg(title));
            m_grabberList.append(new GrabberScript(title, image, VIDEO_FILE, author,
                       search, tree, description, commandline, version));
        }

        grabber = grabber.nextSiblingElement("grabber");
    }

    parsedData();
}

void SearchEditor::parsedData()
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    fillGrabberButtonList();
}

void SearchEditor::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
    else
    {
        delete m_busyPopup;
        m_busyPopup = NULL;
    }
}

bool SearchEditor::keyPressEvent(QKeyEvent *event)
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

void SearchEditor::fillGrabberButtonList()
{
    for (GrabberScript::scriptList::iterator i = m_grabberList.begin();
            i != m_grabberList.end(); ++i)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_grabbers, (*i)->GetTitle());
        if (item)
        {
            item->SetText((*i)->GetTitle(), "title");
            item->SetData(qVariantFromValue(*i));
            QString img = (*i)->GetImage();
            QString thumb;

            if (!img.startsWith("/") && !img.isEmpty())
                thumb = QString("%1mythnetvision/icons/%2").arg(GetShareDir())
                            .arg((*i)->GetImage());
            else
                thumb = img;

            item->SetImage(thumb);
            item->setCheckable(true);
            item->setChecked(MythUIButtonListItem::NotChecked);
            QFileInfo fi((*i)->GetCommandline());

            if (findSearchGrabberInDB(fi.fileName(), VIDEO_FILE))
                item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
}

void SearchEditor::toggleItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    GrabberScript *script = qVariantValue<GrabberScript *>(item->GetData());

    if (!script)
        return;

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked)
    {
        if (insertSearchInDB(script, VIDEO_FILE))
        {
            m_changed = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else if (removeSearchFromDB(script))
    {
        m_changed = true;
        item->setChecked(MythUIButtonListItem::NotChecked);
    }
}

