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
    m_changed(false)
{
}

SearchEditor::~SearchEditor()
{
    qDeleteAll(m_grabberList);
    m_grabberList.clear();

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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'treeeditor'");
        return false;
    }

    connect(m_grabbers, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(toggleItem(MythUIButtonListItem*)));

    BuildFocusList();

    loadData();

    QString icondir = QString("%1/mythnetvision/icons/").arg(GetShareDir());
    gCoreContext->SaveSetting("mythnetvision.iconDir", icondir);

    return true;
}

void SearchEditor::loadData()
{
    m_grabberList = fillGrabberList();
    fillGrabberButtonList();
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

GrabberScript::scriptList SearchEditor::fillGrabberList()
{
    GrabberScript::scriptList tmp;
    QDir ScriptPath = QString("%1/internetcontent/").arg(GetShareDir());
    QStringList Scripts = ScriptPath.entryList(QDir::Files | QDir::Executable);

    for (QStringList::const_iterator i = Scripts.begin();
            i != Scripts.end(); ++i)
    {
        QProcess scriptCheck;
        QString title, image, description, author, type;
        double version;
        bool search = false;
        bool tree = false;

        QString commandline = QString("%1internetcontent/%2")
                                      .arg(GetShareDir()).arg(*i);
        scriptCheck.setReadChannel(QProcess::StandardOutput);
        scriptCheck.start(commandline, QStringList() << "-v");
        scriptCheck.waitForFinished();
        QByteArray result = scriptCheck.readAll();

        QDomDocument versioninfo;
        versioninfo.setContent(result);
        QDomElement item = versioninfo.documentElement();

        title = item.firstChildElement("name").text();
        author = item.firstChildElement("author").text();
        image = item.firstChildElement("thumbnail").text();
        type = item.firstChildElement("type").text();
        description = item.firstChildElement("description").text();
        version = item.firstChildElement("version").text().toDouble();
        QString treestring = item.firstChildElement("tree").text();
        if (!treestring.isEmpty() && (treestring.toLower() == "true" ||
            treestring.toLower() == "yes" || treestring == "1"))
            tree = true;
        QString searchstring = item.firstChildElement("search").text();
        if (!searchstring.isEmpty() && (searchstring.toLower() == "true" ||
            searchstring.toLower() == "yes" || searchstring == "1"))
            search = true;

        if (search && type.toLower() == "video")
            tmp.append(new GrabberScript(title, image, VIDEO, author,
                       search, tree, description, commandline, version));
    }
    return tmp;
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
            if (findSearchGrabberInDB(fi.fileName(), VIDEO))
                item->setChecked(MythUIButtonListItem::FullChecked);
        }
        else
            delete item;
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
        if (insertSearchInDB(script, VIDEO))
        {
            m_changed = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else
    {
        if (removeSearchFromDB(script))
        {
            m_changed = true;
            item->setChecked(MythUIButtonListItem::NotChecked);
        }
    }
}

