// Myth headers
#include <mythtv/mythcontext.h>

// Mythui headers
#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythdialogbox.h>

// Mythvideo headers
#include "fileassoc.h"
#include "dbaccess.h"
#include "videopopups.h"

class FileAssociation
{
    //
    //  Simple data structure to hold
    //

  public:
    FileAssociation(const QString &new_extension) :
        id(-1), extension(new_extension), ignore(false), use_default(true),
        changed(true), loaded_from_db(false) {}
    FileAssociation(int i, const QString &e, const QString &p, bool g, bool u) :
        id(i), extension(e), player_command(p), ignore(g), use_default(u),
        changed(false), loaded_from_db(true) {}

    int     getID() const { return id; }
    QString getExtension() const { return extension; }
    QString getCommand() const { return player_command; }
    bool    getDefault() const { return use_default; }
    bool    getIgnore() const { return ignore; }

    void saveYourself()
    {
        if (changed)
        {
            id = FileAssociations::getFileAssociation()
                    .add(extension, player_command, ignore, use_default);
            loaded_from_db = true;
            changed = false;
        }
    }

    void deleteYourselfFromDB()
    {
        if (loaded_from_db)
        {
            FileAssociations::getFileAssociation().remove(id);
            loaded_from_db = false;
            id = -1;
        }
    }

    void setDefault(bool yes_or_no)
    {
        if (use_default != yes_or_no)
        {
            setChanged();
            use_default = yes_or_no;
        }
    }

    void setIgnore(bool yes_or_no)
    {
        if (ignore != yes_or_no)
        {
            setChanged();
            ignore = yes_or_no;
        }
    }

    void setCommand(const QString &new_command)
    {
        if (player_command != new_command)
        {
            setChanged();
            player_command = new_command;
        }
    }

  private:
    int          id;
    QString      extension;
    QString      player_command;
    bool         ignore;
    bool         use_default;
    bool         changed;
    bool         loaded_from_db;

  private:
    void setChanged() { changed = true; }

};

////////////////////////////////////////////////////////////

FileAssocDialog::FileAssocDialog(MythScreenStack *parent, const QString &name)
                : MythScreenType(parent, name)
{
    m_commandEdit = NULL;
    m_fileAssociations.clear();
    m_currentFileAssociation = NULL;

    loadFileAssociations();
    showCurrentFA();
}

FileAssocDialog::~FileAssocDialog()
{
    while (!m_fileAssociations.isEmpty())
        delete m_fileAssociations.takeFirst();
}

bool FileAssocDialog::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "file_associations", this);

    if (!foundtheme)
        return false;

    m_extensionList = dynamic_cast<MythListButton *> (GetChild("m_extensionList"));
    m_commandEdit = dynamic_cast<MythUITextEdit *> (GetChild("command"));
    m_ignoreCheck = dynamic_cast<MythUICheckBox *> (GetChild("m_ignoreCheck"));
    m_defaultCheck = dynamic_cast<MythUICheckBox *> (GetChild("m_defaultCheck"));
    m_doneButton = dynamic_cast<MythUIButton *> (GetChild("m_doneButton"));
    m_newButton = dynamic_cast<MythUIButton *> (GetChild("m_newButton"));
    m_deleteButton = dynamic_cast<MythUIButton *> (GetChild("m_deleteButton"));

    if (!m_commandEdit || !m_extensionList || !m_defaultCheck || !m_ignoreCheck
        || !m_doneButton || !m_newButton || !m_deleteButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    connect(m_extensionList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(switchToFA(MythListButtonItem*)));
    connect(m_commandEdit, SIGNAL(valueChanged()), SLOT(setPlayerCommand()));
    connect(m_defaultCheck, SIGNAL(valueChanged()), SLOT(toggleDefault()));
    connect(m_ignoreCheck, SIGNAL(valueChanged()), SLOT(toggleIgnore()));
    connect(m_doneButton, SIGNAL(buttonPressed()), SLOT(saveAndExit()));
    connect(m_newButton, SIGNAL(buttonPressed()), SLOT(makeNewExtension()));
    connect(m_deleteButton, SIGNAL(buttonPressed()), SLOT(deleteCurrent()));

    m_deleteButton->SetText(tr("Delete"));
    m_doneButton->SetText(tr("Done"));
    m_newButton->SetText(tr("New"));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

void FileAssocDialog::loadFileAssociations()
{
    const FileAssociations::association_list &fa_list =
            FileAssociations::getFileAssociation().getList();

    for (FileAssociations::association_list::const_iterator p = fa_list.begin();
         p != fa_list.end(); ++p)
    {
        FileAssociation *new_fa =
                new FileAssociation(p->id, p->extension, p->playcommand,
                                    p->ignore, p->use_default);
        m_fileAssociations.append(new_fa);
    }

    if (!m_fileAssociations.isEmpty())
    {
        m_currentFileAssociation = m_fileAssociations.first();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("%1: Couldn't get any filetypes from "
                                      "your database.").arg(__FILE__));
    }
}

void FileAssocDialog::saveFileAssociations()
{
    for (int i = 0; i < m_fileAssociations.count(); i++)
    {
        m_fileAssociations.at(i)->saveYourself();
    }
}

void FileAssocDialog::showCurrentFA()
{
    if (!m_currentFileAssociation)
    {
        m_extensionList->SetVisible(false);
        m_commandEdit->SetVisible(false);
        m_defaultCheck->SetVisible(false);
        m_ignoreCheck->SetVisible(false);
        m_deleteButton->SetVisible(false);

        BuildFocusList();
    }
    else
    {

        m_extensionList->SetVisible(true);
        m_extensionList->Reset();
        for (int i = 0; i < m_fileAssociations.count(); i++)
        {
            new MythListButtonItem(m_extensionList,
                                   m_fileAssociations.at(i)->getExtension(),
                                   m_fileAssociations.at(i)->getID());
        }
        m_extensionList->SetValueByData(m_currentFileAssociation->getID());

        m_commandEdit->SetVisible(true);
        m_commandEdit->SetText(m_currentFileAssociation->getCommand());

        m_defaultCheck->SetVisible(true);
        if (m_currentFileAssociation->getDefault())
            m_defaultCheck->SetCheckState(MythUIStateType::Full);
        else
            m_defaultCheck->SetCheckState(MythUIStateType::Off);

        m_ignoreCheck->SetVisible(true);
        if (m_currentFileAssociation->getIgnore())
            m_ignoreCheck->SetCheckState(MythUIStateType::Full);
        else
            m_ignoreCheck->SetCheckState(MythUIStateType::Off);

        m_deleteButton->SetVisible(true);

        BuildFocusList();
    }
}

void FileAssocDialog::switchToFA(MythListButtonItem *item)
{
    int which_one = item->GetData().toInt();
    for (int i = 0; i < m_fileAssociations.count(); i++)
    {
        if (m_fileAssociations.at(i)->getID() == which_one)
        {
            m_currentFileAssociation = m_fileAssociations.at(i);
            i = m_fileAssociations.count() + 1;
        }
    }
    showCurrentFA();
}

void FileAssocDialog::saveAndExit()
{
    saveFileAssociations();
    Close();
}

void FileAssocDialog::toggleDefault()
{
    if (m_currentFileAssociation)
    {
        if (m_defaultCheck->GetCheckState() == MythUIStateType::Full)
            m_currentFileAssociation->setDefault(true);
        else
            m_currentFileAssociation->setDefault(false);
    }
}

void FileAssocDialog::toggleIgnore()
{
    if (m_currentFileAssociation)
    {
        if (m_ignoreCheck->GetCheckState() == MythUIStateType::Full)
            m_currentFileAssociation->setIgnore(true);
        else
            m_currentFileAssociation->setIgnore(false);
    }
}

void FileAssocDialog::setPlayerCommand()
{
    QString new_command = m_commandEdit->GetText();
    if (m_currentFileAssociation)
        m_currentFileAssociation->setCommand(new_command);
}

void FileAssocDialog::deleteCurrent()
{
    if (m_currentFileAssociation)
    {
        m_currentFileAssociation->deleteYourselfFromDB();
        m_fileAssociations.remove(m_currentFileAssociation);
        m_currentFileAssociation = m_fileAssociations.first();
    }
    showCurrentFA();
}

void FileAssocDialog::makeNewExtension()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Enter the new extension:");

    MythTextInputDialog *newextdialog =
                                new MythTextInputDialog(popupStack, message);

    if (newextdialog->Create())
        popupStack->AddScreen(newextdialog);

    connect(newextdialog, SIGNAL(haveResult(QString &)),
            SLOT(createExtension(QString)), Qt::QueuedConnection);
}

void FileAssocDialog::createExtension(QString new_extension)
{
    if (new_extension.length() > 0)
    {
        FileAssociation *new_fa = new FileAssociation(new_extension);
        m_fileAssociations.append(new_fa);
        m_currentFileAssociation = new_fa;
    }
    showCurrentFA();
}
