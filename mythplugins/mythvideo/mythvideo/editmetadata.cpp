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
                  : MythThemedDialog(parent, window_name, theme_filename, name)
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
    if (category_select)
    {
        category_select->addItem(0,"Unknown");
        QString q_string = QString("SELECT intid, category FROM videocategory"
                                   " ORDER BY category");
        QSqlQuery a_query(q_string,db);
        if (a_query.isActive())
        {
            while (a_query.next())
            {
                QString cat = QString::fromUtf8(a_query.value(1).toString());
                category_select->addItem(a_query.value(0).toInt(), cat);
            }
        }
        category_select->setToItem(working_metadata->getIdCategory(db));
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

        QString q_string = QString("SELECT intid, title FROM videometadata "
                                   "ORDER BY title ;");
        QSqlQuery a_query(q_string, db);
        if(a_query.isActive() && a_query.size() > 0)
        {
            while(a_query.next())
            {
                QString query_name = QString::fromUtf8(a_query.value(1).toString());
                if(trip_catch)
                {
                    //
                    //  Previous loop told us to check if the two
                    //  movie names are close enough to try and
                    //  set a default starting point.
                    //
            
                    QString target_name = query_name;
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
                                          query_name);
                }
                else
                {
                    //
                    //  This is the current file. Set a flag so the default
                    //  selected child will be set next loop
                    //

                    trip_catch = true;
                    caught_name = query_name;
                }
            }
        }
        if(working_metadata->ChildID() > 0)
        {
            child_select->setToItem(working_metadata->ChildID());
            cachedChildSelection = working_metadata->ChildID();
        }
        else
        {
            child_select->setToItem(possible_starting_point);
            cachedChildSelection = possible_starting_point;
        }
    }
    if(child_select && child_check)
    {
        child_check->setState(cachedChildSelection > 0);
        child_select->allowFocus(cachedChildSelection > 0);
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

void EditMetadataDialog::toggleChild(bool yes_or_no)
{
    if(child_select)
    {
        if(yes_or_no)
        {
            child_select->setToItem(cachedChildSelection);
            working_metadata->setChildID(cachedChildSelection);
        }
        else
        {
            child_select->setToItem(0);
            working_metadata->setChildID(0);
        }
        child_select->allowFocus(yes_or_no);
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
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    category_select->push(false);
                    something_pushed = true;
                }
            }
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
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    category_select->push(true);
                    something_pushed = true;
                }
            }
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
        {
            something_pushed = false;
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    QString category = QString("");
                    bool ok = false;
                    MythInputDialog    *newcategory = new MythInputDialog(
                                    QObject::tr("New category"),
                                    &ok,
                                    &category,
                                    gContext->GetMainWindow());
                    newcategory->exec();
                    delete newcategory;    

                    if (ok)
                    {
                        working_metadata->setCategory(category);
                        int id = working_metadata->getIdCategory(db);
                        category_select->addItem(id, category);
                        category_select->setToItem(id);
                    }
                    something_pushed = true;
                }
            }
    
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
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

void EditMetadataDialog::setCategory(int new_category)
{
    working_metadata->setIdCategory(db, new_category);
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
    if(child_check)
    {
        child_check->setState(new_child > 0);
        cachedChildSelection = new_child;
    }
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

    QString fileprefix = gContext->GetSetting("VideoArtworkDir");
    // If the video artwork setting hasn't been set default to
    // using ~/.mythtv/MythVideo
    if( fileprefix.length() == 0 )
    {
        fileprefix = MythContext::GetConfDir() + "/MythVideo";
    }


    MythImageFileDialog *nca = 
        new MythImageFileDialog(new_coverart_file,
                                fileprefix,
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

    category_select = getUISelectorType("category_select");
    if(level_select)
    {
        connect(category_select, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));
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

    child_check = getUICheckBoxType("child_check");
    if(child_check)
    {
        connect(child_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleChild(bool)));
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


/*
---------------------------------------------------------------------
*/

MythInputDialog::MythInputDialog(QString message,
                                       bool *success,
                                       QString *target,
                                       MythMainWindow *parent, 
                                       const char *name, 
                                       bool)
                   :MythDialog(parent, name, false)
{
    success_flag = success;
    target_text = target;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    this->setGeometry((screenwidth - 400 ) / 2,
                      (screenheight - 50 ) / 2,
                      400,50);
    QFrame *outside_border = new QFrame(this);
    outside_border->setGeometry(0,0,400,50);
    outside_border->setFrameStyle(QFrame::Panel | QFrame::Raised );
    outside_border->setLineWidth(4);
    QLabel *message_label = new QLabel(message, this);
    message_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    message_label->setGeometry(15,10,180,30);
    message_label->setBackgroundOrigin(ParentOrigin);
    text_editor = new MythLineEdit(this);
    text_editor->setGeometry(200,10,185,30);
    text_editor->setBackgroundOrigin(ParentOrigin);

    this->setActiveWindow();
    text_editor->setFocus();
}

void MythInputDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                MythDialog::keyPressEvent(e);
            }
            else if (action == "SELECT")
            {
                *success_flag = true;
                *target_text = QString(text_editor->text());
                handled = true;
                MythDialog::keyPressEvent(e);
                done(0);
            }
        }
    }
}


MythInputDialog::~MythInputDialog()
{
}
