/*
	fileassoc.cpp

	(c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
	Part of the mythTV project
	
    Classes to manipulate the file associations stored
    in the videotypes table (in the mythconverg database)

*/

#include <qaccel.h>

#include "fileassoc.h"

FileAssociation::FileAssociation(const QString &new_extension)
{
    //
    //  Create a new file association
    //  (empty)
    //

    loaded_from_db = false;
    changed = true;
    id = -1;
    extension = new_extension;
    player_command = "";
    ignore = false;
    use_default = true;
}

FileAssociation::FileAssociation(int   i,
                                 const QString &e,
                                 const QString &p,
                                 bool  g,
                                 bool  u)
{

    //
    // "Create" a file association that
    //  already exists in the db
    //

    loaded_from_db = true;
    changed = false;

    id = i;
    extension = e;
    player_command = p;
    ignore = g;
    use_default = u;    

};

void FileAssociation::saveYourself()
{
    if(changed)
    {
        if(loaded_from_db)
        {
            //
            //  This one existed before, just need to
            //  save its changes.
            //

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("UPDATE videotypes SET playcommand = :COMMAND, "
                          "f_ignore = :IGNORE, use_default = :DEFAULT "
                          "WHERE intid = :ID ;");
            query.bindValue(":COMMAND", player_command);
            query.bindValue(":IGNORE", ignore);
            query.bindValue(":DEFAULT", use_default);
            query.bindValue(":ID", id);
            if (!query.exec() && !query.isActive())
                MythContext::DBError("videotypes update", query);
        }
        else
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("INSERT INTO videotypes "
                          "(extension, playcommand, f_ignore, use_default) "
                          "VALUES (:EXT, :COMMAND, :IGNORE, :DEFAULT) ;");
            query.bindValue(":EXT", extension);
            query.bindValue(":COMMAND", player_command);
            query.bindValue(":IGNORE", ignore);
            query.bindValue(":DEFAULT", use_default);

            if(!query.exec() && !query.isActive())
                MythContext::DBError("videotypes insert", query);
        }
    }
}

void FileAssociation::deleteYourselfFromDB()
{
    if(loaded_from_db)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM videotypes WHERE intid = :ID ;");
        query.bindValue(":ID", id);
        if(!query.exec())
            MythContext::DBError("delete videotypes", query);
    }
}

/*
---------------------------------------------------------------------
*/

FileAssocDialog::FileAssocDialog(MythMainWindow *parent, 
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
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
    if(command_editor)
    {
        command_editor->clearFocus();
    }
}

void FileAssocDialog::loadFileAssociations()
{
    QString q_string = QString("SELECT intid, extension, playcommand, "
                               "f_ignore, use_default "
                               "FROM videotypes ;");
    
    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.exec(q_string);

    if(a_query.isActive() && a_query.size() > 0)
    {
        while(a_query.next())
        {
            FileAssociation *new_fa = new FileAssociation(a_query.value(0).toInt(),
                                                          a_query.value(1).toString(),
                                                          a_query.value(2).toString(),
                                                          a_query.value(3).toBool(),
                                                          a_query.value(4).toBool());
            if(file_associations.count() < 1)
            {
                current_fa = new_fa;
            }
            file_associations.append(new_fa);
            
            //
            //  Don't delete new'd file association, as the pointers
            //  are now "owned" by file_associations pointer list.
            //                                                          
        }
    }
    else
    {
        cerr << "fileassoc.o: Couldn'g get any filetypes from your database." << endl;
    }
    
}

void FileAssocDialog::saveFileAssociations()
{
    for(uint i = 0; i < file_associations.count(); i++)
    {
        file_associations.at(i)->saveYourself();
    }
}

void FileAssocDialog::showCurrentFA()
{
    if(!current_fa)
    {
        if(extension_select)
        {
            extension_select->SetContext(-2);
        }
        if(command_editor)
        {
            command_editor->hide();
            command_hack->SetContext(-2);
        }
        if(default_check)
        {
            default_check->SetContext(-2);
        }
        if(ignore_check)
        {
            ignore_check->SetContext(-2);
        }
        if(delete_button)
        {
            delete_button->SetContext(-2);
        }
        UIType *current_widget = getCurrentFocusWidget();
        if(current_widget)
        {
            current_widget->looseFocus();
        }
        if(new_button)
        {
            new_button->takeFocus();
            widget_with_current_focus = new_button;
        }
        else if(done_button)
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
        if(extension_select)
        {
            extension_select->SetContext(-1);
            extension_select->cleanOut();
            for(uint i = 0; i < file_associations.count(); i++)
            {
                extension_select->addItem(file_associations.at(i)->getID(),
                                          file_associations.at(i)->getExtension());
            }
            extension_select->setToItem(current_fa->getID());
        }
    
        if(command_editor)
        {
            command_hack->SetContext(-1);
            command_editor->show();
            command_editor->setText(current_fa->getCommand());
        }
        if(default_check)
        {
            default_check->SetContext(-1);
            default_check->setState(current_fa->getDefault());
        }
        if(ignore_check)
        {
            ignore_check->SetContext(-1);
            ignore_check->setState(current_fa->getIgnore());
        }
        if(delete_button)
        {
            delete_button->SetContext(-1);
        }
    }
    update();
}

void FileAssocDialog::switchToFA(int which_one)
{
    for(uint i = 0; i < file_associations.count(); i++)
    {
        if(file_associations.at(i)->getID() == which_one)
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
    if(current_fa)
    {
        current_fa->setDefault(yes_or_no);
        current_fa->setChanged();
    }
}

void FileAssocDialog::toggleIgnore(bool yes_or_no)
{
    if(current_fa)
    {
        current_fa->setIgnore(yes_or_no);
        current_fa->setChanged();
    }
}

void FileAssocDialog::setPlayerCommand(QString new_command)
{
    if(current_fa)
    {
        if(current_fa->getCommand() != new_command)
        {
            current_fa->setCommand(new_command);
            current_fa->setChanged();
        }
    }
}

void FileAssocDialog::deleteCurrent()
{
    if(current_fa)
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
    if(new_extension.length() > 0)
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
    if(current_widget)
    {
        current_widget->looseFocus();
    }
    if(extension_select)
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
    if(extension_select)
    {
        connect(extension_select, SIGNAL(pushed(int)), this, SLOT(switchToFA(int)));
    }
    command_hack = getUIBlackHoleType("command_hack");
    if(command_hack)
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
    if(default_check)
    {
        connect(default_check, SIGNAL(pushed(bool)), this, SLOT(toggleDefault(bool)));
    }
    
    ignore_check = getUICheckBoxType("ignore_check");
    if(ignore_check)
    {
        connect(ignore_check, SIGNAL(pushed(bool)), this, SLOT(toggleIgnore(bool)));
    }

    done_button = getUITextButtonType("done_button");
    if(done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    new_button = getUITextButtonType("new_button");
    if(new_button)
    {
        new_button->setText(tr("New"));
        connect(new_button, SIGNAL(pushed()), this, SLOT(makeNewExtension()));
    }

    delete_button = getUITextButtonType("delete_button");
    if(delete_button)
    {
        delete_button->setText(tr("Delete"));
        connect(delete_button, SIGNAL(pushed()), this, SLOT(deleteCurrent()));
    }
    buildFocusList();
}

FileAssocDialog::~FileAssocDialog()
{
    file_associations.clear();
    if(command_editor)
    {
        delete command_editor;
    }
}

