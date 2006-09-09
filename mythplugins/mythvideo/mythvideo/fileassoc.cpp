/*
    fileassoc.cpp

    (c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
    Part of the mythTV project

    Classes to manipulate the file associations stored
    in the videotypes table (in the mythconverg database)

*/

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "fileassoc.h"
#include "dbaccess.h"

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

/*
---------------------------------------------------------------------
*/

FileAssocDialog::FileAssocDialog(MythMainWindow *parent_,
                                 QString window_name,
                                 QString theme_filename,
                                 const char *name_)
                : MythThemedDialog(parent_, window_name, theme_filename, name_)
{
    command_editor = NULL;
    file_associations.clear();
    file_associations.setAutoDelete(true);
    current_fa = NULL;
    new_extension_popup = NULL;
    new_extension_editor = NULL;
    wireUpTheme();
    assignFirstFocus();

    loadFileAssociations();
    showCurrentFA();
}

void FileAssocDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            nextPrevWidgetFocus(false);
            while (getCurrentFocusWidget()->GetContext() < -1)
                nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            nextPrevWidgetFocus(true);
            while (getCurrentFocusWidget()->GetContext() < -1)
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            if (extension_select)
            {
                if (extension_select == getCurrentFocusWidget())
                    extension_select->push(false);
            }
            if (default_check)
            {
                if (default_check == getCurrentFocusWidget())
                    activateCurrent();
            }
            if (ignore_check)
            {
                if (ignore_check == getCurrentFocusWidget())
                    activateCurrent();
            }
        }
        else if (action == "RIGHT")
        {
            if (extension_select)
            {
                if (extension_select == getCurrentFocusWidget())
                    extension_select->push(true);
            }
            if (default_check)
            {
                if (default_check == getCurrentFocusWidget())
                    activateCurrent();
            }
            if (ignore_check)
            {
                if (ignore_check == getCurrentFocusWidget())
                    activateCurrent();
            }
        }
        else if (action == "SELECT")
            activateCurrent();
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void FileAssocDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);
    if (command_editor)
    {
        command_editor->clearFocus();
    }
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
        file_associations.append(new_fa);
    }

    if (file_associations.count())
    {
        current_fa = file_associations.getFirst();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("%1: Couldn't get any filetypes from "
                                      "your database.").arg(__FILE__));
    }
}

void FileAssocDialog::saveFileAssociations()
{
    for (uint i = 0; i < file_associations.count(); i++)
    {
        file_associations.at(i)->saveYourself();
    }
}

void FileAssocDialog::showCurrentFA()
{
    if (!current_fa)
    {
        if (extension_select)
        {
            extension_select->SetContext(-2);
        }
        if (command_editor)
        {
            command_editor->hide();
            command_hack->SetContext(-2);
        }
        if (default_check)
        {
            default_check->SetContext(-2);
        }
        if (ignore_check)
        {
            ignore_check->SetContext(-2);
        }
        if (delete_button)
        {
            delete_button->SetContext(-2);
        }
        UIType *current_widget = getCurrentFocusWidget();
        if (current_widget)
        {
            current_widget->looseFocus();
        }
        if (new_button)
        {
            new_button->takeFocus();
            widget_with_current_focus = new_button;
        }
        else if (done_button)
        {
            done_button->takeFocus();
            widget_with_current_focus = done_button;
        }
        else
        {
            assignFirstFocus();
        }
    }
    else
    {
        if (extension_select)
        {
            extension_select->SetContext(-1);
            extension_select->cleanOut();
            for (uint i = 0; i < file_associations.count(); i++)
            {
                extension_select->addItem(file_associations.at(i)->getID(),
                                       file_associations.at(i)->getExtension());
            }
            extension_select->setToItem(current_fa->getID());
        }

        if (command_editor)
        {
            command_hack->SetContext(-1);
            command_editor->show();
            command_editor->setText(current_fa->getCommand());
        }
        if (default_check)
        {
            default_check->SetContext(-1);
            default_check->setState(current_fa->getDefault());
        }
        if (ignore_check)
        {
            ignore_check->SetContext(-1);
            ignore_check->setState(current_fa->getIgnore());
        }
        if (delete_button)
        {
            delete_button->SetContext(-1);
        }
    }
    update();
}

void FileAssocDialog::switchToFA(int which_one)
{
    for (uint i = 0; i < file_associations.count(); i++)
    {
        if (file_associations.at(i)->getID() == which_one)
        {
            current_fa = file_associations.at(i);
            i = file_associations.count() + 1;
        }
    }
    showCurrentFA();
}

void FileAssocDialog::saveAndExit()
{
    saveFileAssociations();
    done(0);
}

void FileAssocDialog::toggleDefault(bool yes_or_no)
{
    if (current_fa)
    {
        current_fa->setDefault(yes_or_no);
    }
}

void FileAssocDialog::toggleIgnore(bool yes_or_no)
{
    if (current_fa)
    {
        current_fa->setIgnore(yes_or_no);
    }
}

void FileAssocDialog::setPlayerCommand(QString new_command)
{
    if (current_fa)
    {
        current_fa->setCommand(new_command);
    }
}

void FileAssocDialog::deleteCurrent()
{
    if (current_fa)
    {
        current_fa->deleteYourselfFromDB();
        file_associations.remove(current_fa);
        current_fa = file_associations.first();
    }
    showCurrentFA();
}

void FileAssocDialog::makeNewExtension()
{
    // Create the popup window for new extension dialog/widgets

    new_extension_popup = new MythPopupBox(gContext->GetMainWindow(),
                                           "new extension popup");
    gContext->ThemeWidget(new_extension_popup);

    new_extension_popup->addLabel("");
    new_extension_popup->addLabel(tr("Please enter the new extension:"));
    new_extension_popup->addLabel("");


    new_extension_editor = new MythRemoteLineEdit(new_extension_popup);
    new_extension_popup->addWidget(new_extension_editor);

    new_extension_popup->addButton(tr("Create new extension"), this,
                                   SLOT(createExtension()));
    new_extension_popup->addButton(tr("Cancel"), this,
                                   SLOT(removeExtensionPopup()));

    new_extension_editor->setFocus();

    new_extension_popup->ShowPopup(this, SLOT(removeExtensionPopup()));
}

void FileAssocDialog::createExtension()
{
    QString new_extension = new_extension_editor->text();
    if (new_extension.length() > 0)
    {
        FileAssociation *new_fa = new FileAssociation(new_extension);
        file_associations.append(new_fa);
        current_fa = new_fa;
    }
    removeExtensionPopup();
    showCurrentFA();
}

void FileAssocDialog::removeExtensionPopup()
{
    new_extension_popup->close();
    delete new_extension_editor;
    new_extension_editor = NULL;
    delete new_extension_popup;
    new_extension_popup = NULL;

    //
    //  Move focus to the extension
    //  selector
    //

    UIType *current_widget = getCurrentFocusWidget();
    if (current_widget)
    {
        current_widget->looseFocus();
    }
    if (extension_select)
    {
        widget_with_current_focus = extension_select;
        extension_select->takeFocus();
    }
    else
    {
        assignFirstFocus();
    }
    update();
}

void FileAssocDialog::wireUpTheme()
{
    extension_select = getUISelectorType("extension_select");
    if (extension_select)
    {
        connect(extension_select, SIGNAL(pushed(int)), this,
                SLOT(switchToFA(int)));
    }
    command_hack = getUIBlackHoleType("command_hack");
    if (command_hack)
    {
        command_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        command_editor = new MythRemoteLineEdit(&f, this);
        command_editor->setFocusPolicy(QWidget::NoFocus);
        command_editor->setGeometry(command_hack->getScreenArea());
        connect(command_hack, SIGNAL(takingFocus()),
                command_editor, SLOT(setFocus()));
        connect(command_editor, SIGNAL(tryingToLooseFocus(bool)),
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(command_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setPlayerCommand(QString)));
    }

    default_check = getUICheckBoxType("default_check");
    if (default_check)
    {
        connect(default_check, SIGNAL(pushed(bool)), this,
                SLOT(toggleDefault(bool)));
    }

    ignore_check = getUICheckBoxType("ignore_check");
    if (ignore_check)
    {
        connect(ignore_check, SIGNAL(pushed(bool)), this,
                SLOT(toggleIgnore(bool)));
    }

    done_button = getUITextButtonType("done_button");
    if (done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    new_button = getUITextButtonType("new_button");
    if (new_button)
    {
        new_button->setText(tr("New"));
        connect(new_button, SIGNAL(pushed()), this, SLOT(makeNewExtension()));
    }

    delete_button = getUITextButtonType("delete_button");
    if (delete_button)
    {
        delete_button->setText(tr("Delete"));
        connect(delete_button, SIGNAL(pushed()), this, SLOT(deleteCurrent()));
    }
    buildFocusList();
}

FileAssocDialog::~FileAssocDialog()
{
    file_associations.clear();
    if (command_editor)
    {
        delete command_editor;
    }
}
