#include <iostream>
#include <set>
#include <map>

#include <QDomDocument>
#include <QDateTime>
#include <QImageReader>

// MythTV headers
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <util.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <netutils.h>

// Tree headers
#include "treeeditor.h"

#define LOC      QString("TreeEditor: ")
#define LOC_WARN QString("TreeEditor, Warning: ")
#define LOC_ERR  QString("TreeEditor, Error: ")

/** \brief Creates a new TreeEditor Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
TreeEditor::TreeEditor(MythScreenStack *parent,
                          const QString name) :
    MythScreenType(parent, name),
    m_lock(QMutex::Recursive),
    m_grabbers(NULL),
    m_changed(false)
{
}

TreeEditor::~TreeEditor()
{
    QMutexLocker locker(&m_lock);

    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_changed)
        emit itemsChanged();
}

bool TreeEditor::Create(void)
{
    QMutexLocker locker(&m_lock);

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

void TreeEditor::loadData()
{
    m_grabberList = fillGrabberList();
    fillGrabberButtonList();
}

bool TreeEditor::keyPressEvent(QKeyEvent *event)
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

GrabberScript::scriptList TreeEditor::fillGrabberList()
{
    QMutexLocker locker(&m_lock);

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
        QString dbIconDir = gCoreContext->GetSetting("mythnetvision.iconDir", icondir);

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
        if (tree)
            tmp.append(new GrabberScript(title, image, search, tree, commandline));
    }
    return tmp;
}

void TreeEditor::fillGrabberButtonList()
{
    QMutexLocker locker(&m_lock);

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
        if (findTreeGrabberInDB((*i)->GetCommandline()))
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
        else
            delete item;
    }
}

void TreeEditor::toggleItem(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item)
        return;

    GrabberScript *script = qVariantValue<GrabberScript *>(item->GetData());

    if (!script)
        return;

    bool checked = (item->state() == MythUIButtonListItem::FullChecked);

    if (!checked)
    {
        if (insertTreeInDB(script))
        {
            m_changed = true;
            item->setChecked(MythUIButtonListItem::FullChecked);
        }
    }
    else
    {
        if (removeTreeFromDB(script))
        {
            if (!isTreeInUse(script->GetTitle()))
                clearTreeItems(script->GetTitle());
            m_changed = true;
            item->setChecked(MythUIButtonListItem::NotChecked);
        }
    }
}

