/*
	editmetadata.cpp

	(c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
	Part of the mythTV project
	

*/

#include <mythtv/mythcontext.h>

#include "editmetadata.h"


EditMetadataDialog::EditMetadataDialog(QSqlDatabase *ldb,
                                 Metadata *source_metadata,
                                 MythMainWindow *parent, 
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    //
    //  The only thing this screen does is let the
    //  user set (some) metadata information. It only
    //  works on a single metadata entry.
    //
    
    if(!ldb)
    {
        cerr << "editmetadata.o: Where I am supposed to load stuff from if you don't give me a db pointer?" << endl;
        exit(0);
    }
    db = ldb;
    
    //
    //  Make a copy, so we can abandon changes if desired
    //

    working_metadata = new Metadata();
    working_metadata->setID(source_metadata->ID());
    working_metadata->fillDataFromID(db);
    
    title_editor = NULL; 
    player_editor = NULL; 
    wireUpTheme();
    fillWidgets();
    assignFirstFocus();
}

void EditMetadataDialog::fillWidgets()
{
    if(title_editor)
    {
        title_editor->setText(working_metadata->Title());
    }
    if(level_select)
    {
        for(int i = 1; i < 5; i++)
        {
            level_select->addItem(i, QString(tr("Level %1")).arg(i));
        }
        level_select->setToItem(working_metadata->ShowLevel());
    }
    if(child_select)
    {
        //
        //  Fill the "always play this video next" option
        //  with all available videos. 
        //  

        bool trip_catch = false;
        QString caught_name = "";
        int possible_starting_point = 0;
        child_select->addItem(0, tr("None"));

        QString q_string = QString("SELECT intid, title FROM videometadata ORDER BY title ;");
        QSqlQuery a_query(q_string, db);
        if(a_query.isActive() && a_query.numRowsAffected() > 0)
        {
            while(a_query.next())
            {
                if(trip_catch)
                {
                    //
                    //  Previous loop told us to check if the two
                    //  movie names are close enough to try and
                    //  set a default starting point.
                    //
            
                    QString target_name = a_query.value(1).toString();
                    int length_compare = 0;
                    if(target_name.length() < caught_name.length())
                    {
                        length_compare = target_name.length();
                    }
                    else
                    {
                        length_compare = caught_name.length();
                    }

                    QString caught_name_three_quarters = caught_name.left((int)(length_compare * 0.75));
                    QString target_name_three_quarters = target_name.left((int)(length_compare * 0.75));
                 
                    if(caught_name_three_quarters == target_name_three_quarters && working_metadata->ChildID() == -1)
                    {
                        possible_starting_point = a_query.value(0).toInt();
                        working_metadata->setChildID(possible_starting_point);
                    }
                    trip_catch = false;
                }
                if(a_query.value(0).toUInt() != working_metadata->ID())
                {
                    child_select->addItem(a_query.value(0).toInt(),
                                          a_query.value(1).toString());
                }
                else
                {
                    //
                    //  This is the current file. Set a flag so the default
                    //  selected child will be set next loop
                    //

                    trip_catch = true;
                    caught_name = a_query.value(1).toString();
                }
            }
        }
        if(working_metadata->ChildID() > 0)
        {
            child_select->setToItem(working_metadata->ChildID());
        }
        else
        {
            child_select->setToItem(possible_starting_point);
        }
    }
    if(browse_check)
    {
        browse_check->setState(working_metadata->Browse());
    }
    if(coverart_text)
    {
        coverart_text->SetText(working_metadata->CoverFile());
    }
    if(player_editor)
    {
        player_editor->setText(working_metadata->PlayCommand());
    }
    
}

void EditMetadataDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool something_pushed = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT") 
        {
            something_pushed = false;
            if (level_select)
            {
                if (getCurrentFocusWidget() == level_select)
                {
                    level_select->push(false);
                    something_pushed = true;
                }
            }
            if (child_select)
            {
                if (getCurrentFocusWidget() == child_select)
                {
                    child_select->push(false);
                    something_pushed = true;
                }
            }
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "RIGHT")
        {
            something_pushed = false;
            if (level_select)
            {
                if (getCurrentFocusWidget() == level_select)
                {
                    level_select->push(true);
                    something_pushed = true;
                }
            }
            if (child_select)
            {
                if (getCurrentFocusWidget() == child_select)
                {
                    child_select->push(true);
                    something_pushed = true;
                }
            }
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "SELECT")
            activateCurrent();
        else if (action == "0")
        {    
            if (done_button)
                done_button->push();
        }
        else
            handled = false;
    }
    
    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}


void EditMetadataDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);
    
    MythRemoteLineEdit *which_editor = (MythRemoteLineEdit *)sender();

    if(which_editor)
    {
        which_editor->clearFocus();
    }
}

void EditMetadataDialog::saveAndExit()
{
    //
    //  Persist to database
    //
    
    working_metadata->updateDatabase(db);
    
    //
    //  All done
    //

    done(0);
}

void EditMetadataDialog::setTitle(QString new_title)
{
    working_metadata->setTitle(new_title);
}

void EditMetadataDialog::setPlayer(QString new_command)
{
    working_metadata->setPlayCommand(new_command);
}

void EditMetadataDialog::setLevel(int new_level)
{
    working_metadata->setShowLevel(new_level);
}

void EditMetadataDialog::setChild(int new_child)
{
    working_metadata->setChildID(new_child);
}

void EditMetadataDialog::toggleBrowse(bool yes_or_no)
{
    working_metadata->setBrowse(yes_or_no);
}

void EditMetadataDialog::findCoverArt()
{
    QString *new_coverart_file = new QString("");
    if(working_metadata->CoverFile() != tr("No Cover"))
    {
        *new_coverart_file = working_metadata->CoverFile();
    }

    MythImageFileDialog *nca = 
        new MythImageFileDialog(new_coverart_file,
                                gContext->GetSetting("VideoStartupDir"),
                                gContext->GetMainWindow(),
                                "file_chooser",
                                "video-",
                                "image file chooser",
                                true);
    nca->exec();
    if(new_coverart_file->length() > 0)
    {
        working_metadata->setCoverFile(*new_coverart_file);
        if(coverart_text)
        {
            coverart_text->SetText(*new_coverart_file);
        }
    }
    
    delete nca;
    delete new_coverart_file; 
}

void EditMetadataDialog::wireUpTheme()
{
    title_hack = getUIBlackHoleType("title_hack");
    if(title_hack)
    {
        title_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        title_editor = new MythRemoteLineEdit(&f, this);
        title_editor->setFocusPolicy(QWidget::NoFocus);
        title_editor->setGeometry(title_hack->getScreenArea());
        connect(title_hack, SIGNAL(takingFocus()), 
                title_editor, SLOT(setFocus()));
        connect(title_editor, SIGNAL(tryingToLooseFocus(bool)), 
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(title_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setTitle(QString)));
    }

    player_hack = getUIBlackHoleType("player_hack");
    if(player_hack)
    {
        player_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        player_editor = new MythRemoteLineEdit(&f, this);
        player_editor->setFocusPolicy(QWidget::NoFocus);
        player_editor->setGeometry(player_hack->getScreenArea());
        connect(player_hack, SIGNAL(takingFocus()), 
                player_editor, SLOT(setFocus()));
        connect(player_editor, SIGNAL(tryingToLooseFocus(bool)), 
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(player_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setPlayer(QString)));
    }
    
    level_select = getUISelectorType("level_select");
    if(level_select)
    {
        connect(level_select, SIGNAL(pushed(int)),
                this, SLOT(setLevel(int)));
    }
    
    child_select = getUISelectorType("child_select");
    if(child_select)
    {
        connect(child_select, SIGNAL(pushed(int)),
                this, SLOT(setChild(int)));
    }
    
    browse_check = getUICheckBoxType("browse_check");
    if(browse_check)
    {
        connect(browse_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleBrowse(bool)));
    }
    
    coverart_button = getUIPushButtonType("coverart_button");
    if(coverart_button)
    {
        connect(coverart_button, SIGNAL(pushed()),
                this, SLOT(findCoverArt()));
    }
    coverart_text = getUITextType("coverart_text");

    done_button = getUITextButtonType("done_button");
    if(done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    buildFocusList();
}


EditMetadataDialog::~EditMetadataDialog()
{
    if(title_editor)
    {
        delete title_editor;
    }
    if(player_editor)
    {
        delete player_editor;
    }
    if(working_metadata)
    {
        delete working_metadata;
    }
}

