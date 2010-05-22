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
        QString title, image;
        bool search = false;
        bool tree = false;

        QString commandline = QString("%1internetcontent/%2")
                                      .arg(GetShareDir()).arg(*i);
        scriptCheck.setReadChannel(QProcess::StandardOutput);
        scriptCheck.start(commandline, QStringList() << "-v");
        scriptCheck.waitForFinished();
        QByteArray result = scriptCheck.readAll();
        QString resultString(result);

        if (resultString.isEmpty())
            resultString = *i;

        QString capabilities = QString();
        QStringList specs = resultString.split("|");
        if (specs.size() > 1)
            capabilities = specs.takeAt(1);

        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Capability of script %1: %2")
                              .arg(*i).arg(capabilities.trimmed()));

        title = specs.takeAt(0);

        if (capabilities.trimmed().contains("S"))
            search = true;

        if (capabilities.trimmed().contains("T"))
            tree = true;

        QFileInfo fi(*i);
        QString basename = fi.completeBaseName();
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();

        typedef std::set<QString> image_type_list;
        image_type_list image_exts;

        for (QList<QByteArray>::const_iterator it = image_types.begin();
                it != image_types.end(); ++it)
        {
            image_exts.insert(QString(*it).toLower());
        }

        QStringList sfn;

        QString icondir = QString("%1/mythnetvision/icons/").arg(GetShareDir());
        QString dbIconDir = gCoreContext->GetSetting("mythnetvision.iconDir",
                                                     icondir);

        for (image_type_list::const_iterator ext = image_exts.begin();
                ext != image_exts.end(); ++ext)
        {
            sfn += QString(dbIconDir + basename + "." + *ext);
        }

        for (QStringList::const_iterator im = sfn.begin();
                        im != sfn.end(); ++im)
        {
            if (QFile::exists(*im))
            {
                image = *im;
                break;
            }
        }
        if (search)
            tmp.append(new GrabberScript(title, image, search, tree, commandline));
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
            item->SetImage((*i)->GetImage());
            item->setCheckable(true);
            item->setChecked(MythUIButtonListItem::NotChecked);
            if (findSearchGrabberInDB((*i)->GetCommandline()))
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
        if (insertSearchInDB(script))
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

