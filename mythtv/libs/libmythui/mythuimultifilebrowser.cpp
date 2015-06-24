#include "mythuimultifilebrowser.h"

#include <QCoreApplication>

#include <mythlogging.h>
#include "mythdialogbox.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuitext.h"


/*!
 \brief Constructor
 \param parent Parent window
 \param startPath Dir to start browsing
*/
MythUIMultiFileBrowser::MythUIMultiFileBrowser(MythScreenStack *parent,
                                               const QString &startPath)
    : MythUIFileBrowser(parent, startPath),
      m_selectButton(NULL),
      m_clearButton(NULL),
      m_selectCount(NULL)
{
    m_widgetName = "MythMultiFileBrowser";
}


/*!
 \brief Create dialog
 \return bool False if dialog couldn't be created
*/
bool MythUIMultiFileBrowser::Create()
{
    if (!MythUIFileBrowser::Create())
        return false;

    // Add selection buttons & selection count
    m_selectButton = dynamic_cast<MythUIButton *>(GetChild("selectall"));
    m_clearButton  = dynamic_cast<MythUIButton *>(GetChild("clearall"));
    m_selectCount  = dynamic_cast<MythUIText *>(GetChild("selectcount"));

    if (!m_selectButton || !m_clearButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "MythUIMultiFileBrowser: Your theme is missing"
            " some UI elements! Bailing out.");
        return false;
    }

    connect(m_selectButton, SIGNAL(Clicked()), SLOT(selectPressed()));
    connect(m_clearButton, SIGNAL(Clicked()), SLOT(clearPressed()));

    return true;
}


/*!
 \brief Selects/deselects a file
 \param item Button clicked
*/
void MythUIMultiFileBrowser::PathClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MFileInfo finfo = item->GetData().value<MFileInfo>();

    if (finfo.isFile())
    {
        QString name = finfo.absoluteFilePath();

        // toggle selected state
        if (m_selected.remove(name))
        {
            item->setChecked(MythUIButtonListItem::NotChecked);
        }
        else
        {
            m_selected.insert(name);
            item->setChecked(MythUIButtonListItem::FullChecked);
        }

        // Update selection stats
        if (m_selectCount)
            m_selectCount->SetText(QString::number(m_selected.size()));
    }

    if (!finfo.isDir())
        return;

    // clear selections on every directory change
    m_selected.clear();

    MythUIFileBrowser::PathClicked(item);
}


/*!
 \brief Handle Back button
*/
void MythUIMultiFileBrowser::backPressed()
{
    m_selected.clear();
    MythUIFileBrowser::backPressed();
}


/*!
 \brief Handle Home button
*/
void MythUIMultiFileBrowser::homePressed()
{
    m_selected.clear();
    MythUIFileBrowser::homePressed();
}


/*!
 \brief Handle Accept button
*/
void MythUIMultiFileBrowser::OKPressed()
{
    if (m_retObject)
    {
        QStringList selectedPaths = m_selected.toList();
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0, "",
                                                               selectedPaths);
        QCoreApplication::postEvent(m_retObject, dce);
    }
    Close();
}


/*!
 \brief Handle Select All
*/
void MythUIMultiFileBrowser::selectPressed()
{
    // Select all files
    for (int i=0; i < m_fileList->GetCount(); ++i)
    {
        MythUIButtonListItem *btn = m_fileList->GetItemAt(i);
        MFileInfo finfo = btn->GetData().value<MFileInfo>();

        if (finfo.isFile())
            m_selected.insert(finfo.absoluteFilePath());
    }

    updateFileList();
}


/*!
 \brief Handle Clear button
*/
void MythUIMultiFileBrowser::clearPressed()
{
    m_selected.clear();
    updateFileList();
}


/*!
 \brief Populates dialog
*/
void MythUIMultiFileBrowser::updateFileList()
{
    MythUIFileBrowser::updateFileList();

    // Make buttonlist checkable & set selections
    for (int i=0; i < m_fileList->GetCount(); ++i)
    {
        MythUIButtonListItem *btn = m_fileList->GetItemAt(i);
        MFileInfo finfo = btn->GetData().value<MFileInfo>();
        btn->setCheckable(true);
        bool marked = m_selected.contains(finfo.absoluteFilePath());
        btn->setChecked(marked ? MythUIButtonListItem::FullChecked
                               : MythUIButtonListItem::NotChecked);
    }

    // Update selection stats
    if (m_selectCount)
        m_selectCount->SetText(QString::number(m_selected.size()));
}
