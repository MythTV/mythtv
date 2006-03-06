/*
	titledialog.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    the dialog where you actually choose titles to rip
*/

#include <iostream>
using namespace std;

#include <qlayout.h>
#include <qlabel.h>
#include <qframe.h>
#include <qregexp.h>

#include <mythtv/util.h>
#include <mythtv/uitypes.h>

#include "titledialog.h"

TitleDialog::TitleDialog(QSocket *a_socket, 
                         QString d_name, 
                         QPtrList<DVDTitleInfo> *titles, 
                         MythMainWindow *parent,
                         QString window_name,
                         QString theme_filename,
                         const char* name)
            :MythThemedDialog(parent, window_name, theme_filename, name)
{

    name_editor = NULL;
    socket_to_mtd = a_socket;
    disc_name = d_name;
    if(disc_name.length() < 1)
    {
        disc_name = tr("Unknown");
    }
    dvd_titles = titles;

    wireUpTheme();
    assignFirstFocus();

    //
    //  Find the longest title and set some defaults
    //
    
    uint longest = 0;
    uint longest_time = 0;
    current_title = NULL;
    
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i)->getPlayLength() >= longest_time)
        {
            longest = i;
            longest_time = dvd_titles->at(i)->getPlayLength();
            current_title = dvd_titles->at(i);
        }
    }
    
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i) == current_title)
        {
            dvd_titles->at(i)->setName(disc_name);
            dvd_titles->at(i)->setSelected(true);
        }
        else
        {
            dvd_titles->at(i)->setName(QString(tr("%1 - Title %2")).arg(disc_name).arg(i + 1));
        }
    }

    showCurrentTitle();
    
}

void TitleDialog::showCurrentTitle()
{
    //
    //  Set the widget to display correct info
    //
    if(current_title)
    {
        //
        //  We have to if() check everything, in case the
        //  theme designer left things out.
        //
        
        if(playlength_text)
        {
            playlength_text->SetText(current_title->getTimeString());
        }
        if(ripcheck)
        {
            ripcheck->setState(current_title->getSelected());
        }
        if(name_editor)
        {   
            name_editor->setText(current_title->getName());
        }
        if(audio_select)
        {
            audio_select->cleanOut();
            QPtrList<DVDAudioInfo> *audio_tracks = current_title->getAudioTracks();
            for(uint j = 0; j < audio_tracks->count(); j++)
            {
                audio_select->addItem(j + 1, audio_tracks->at(j)->getAudioString());
            }
            audio_select->setToItem(current_title->getAudio());
        }
        if(quality_select)
        {
            quality_select->cleanOut();
            quality_select->addItem(-1, tr("ISO Image"));
            quality_select->addItem(0, tr("Perfect"));
            QString q_string = QString("SELECT name,intid FROM dvdtranscode "
                                       "WHERE input = %1 ;")
                                       .arg(current_title->getInputID());
       
            MSqlQuery a_query(MSqlQuery::InitCon());
            a_query.exec(q_string);  

            if(a_query.isActive() && a_query.size() > 0)
            {
                while(a_query.next())
                {
                    quality_select->addItem(a_query.value(1).toInt(), a_query.value(0).toString());
                }
            } 
            quality_select->setToItem(current_title->getQuality());            
        }
        
        if(subtitle_select)
        {
            subtitle_select->cleanOut();
            subtitle_select->addItem(-1, tr("None"));
            QPtrList<DVDSubTitleInfo> *subtitles = current_title->getSubTitles();
            for(uint j = 0; j < subtitles->count(); ++j)
            {
                subtitle_select->addItem(subtitles->at(j)->getID(), subtitles->at(j)->getName());
            }
            subtitle_select->setToItem(current_title->getSubTitle());
        }
        
        if(ripacthree)
        {
            ripacthree->setState(current_title->getAC3());
        }
        if(numb_titles_text)
        {
            numb_titles_text->SetText(QString(tr("Title %1 of %2")).arg(current_title->getTrack()).arg(dvd_titles->count()));
        }
    }
    else
    {
        cerr << "titledialog.o: Hmmmm .... should not have shown you this dialog." << endl;
    }
}

void TitleDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("DVD", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PAGEDOWN")
        { 
            if (next_title_button)
                next_title_button->push();
        }
        else if (action == "PAGEUP")
        {
            if (prev_title_button)
                prev_title_button->push();
        }
        else if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "SELECT")   
            activateCurrent();
        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4" || action == "5" || action == "6" ||
                 action == "7" || action == "8" || action == "9")
        {
            gotoTitle(action.toInt());
        }
        else if (action == "LEFT")
            prev_title_button->push();
        else if (action == "RIGHT")
            next_title_button->push();
        else if (action == "0") 
        {
            if (ripaway_button->GetContext() == -1)
                ripaway_button->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void TitleDialog::nextTitle()
{
    dvd_titles->find(current_title);
    current_title = dvd_titles->next();
    if(!current_title)
    {
        current_title = dvd_titles->first();
    }
    showCurrentTitle();
}

void TitleDialog::prevTitle()
{
    dvd_titles->find(current_title);
    current_title = dvd_titles->prev();
    if(!current_title)
    {
        current_title = dvd_titles->last();
    }
    showCurrentTitle();
}

void TitleDialog::gotoTitle(uint title_number)
{
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i)->getTrack() == title_number)
        {
            current_title = dvd_titles->at(i);
            i = dvd_titles->count() + 1;
            showCurrentTitle();
        }
    }
}

void TitleDialog::setQuality(int which_quality)
{
    current_title->setQuality(which_quality);
}

void TitleDialog::setSubTitle(int which_subtitle)
{
    current_title->setSubTitle(which_subtitle);
}

void TitleDialog::setAudio(int audio_index)
{
    current_title->setAudio(audio_index);
}

void TitleDialog::toggleTitle(bool on_or_off)
{
    current_title->setSelected(on_or_off);
    
    //
    //  Should we be showing the Process Title(s)
    //  Button ?
    //
    
    int numb_selected = 0;
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i)->getSelected())
        {
            ++numb_selected;
        }
    }
    if(!ripaway_button)
    {
        return;
    }
    if(numb_selected == 0)
    {
        if(ripaway_button->GetContext() != -2)
        {
            ripaway_button->SetContext(-2);
            ripaway_button->refresh();
        }
        return;
    }
    if(numb_selected == 1)
    {
        ripaway_button->setText(tr("0 Process Selected Title"));
    }
    else
    {
        ripaway_button->setText(tr("0 Process Selected Titles"));
    }
    if(ripaway_button->GetContext() != -1)
    {
        ripaway_button->SetContext(-1);
    }
    ripaway_button->refresh();
}

void TitleDialog::changeName(QString new_name)
{
    current_title->setName(new_name);
}

void TitleDialog::toggleAC3(bool on_or_off)
{
    current_title->setAC3(on_or_off);
}

void TitleDialog::viewTitle()
{
    QString player_string = gContext->GetSetting("TitlePlayCommand");
    if(player_string.length() < 1)
    {
        cerr << "titledialog.o: No title player command defined" << endl;
        return;
    }

    QString dvd_device = gContext->GetSetting("DVDDeviceLocation");
    if(dvd_device.length() < 1)
    {
        cerr << "titledialog.o: No DVD device defined" << endl;
        return;
    }
    
    int audio_track = 1;
    int channels = 2;
    if(current_title)
    {
        audio_track = current_title->getAudio();
        DVDAudioInfo *audio_in_question = current_title->getAudioTrack(audio_track - 1);
        if(audio_in_question)
        {
            channels = audio_in_question->getChannels();
        }
    }

    if(player_string.contains("mplayer"))
    {
        //
        //  Way to save a few bits mplayer
        //
        audio_track += 127;
    }
        
    player_string = player_string.replace(QRegExp("%d"), dvd_device);
    player_string = player_string.replace(QRegExp("%t"), QString("%1").arg(current_title->getTrack()));
    player_string = player_string.replace(QRegExp("%a"), QString("%1").arg(audio_track));
    player_string = player_string.replace(QRegExp("%c"), QString("%1").arg(channels));
    
    if(current_title->getSubTitle() > -1)
    {
        QString player_append = gContext->GetSetting("SubTitleCommand");
        if(player_append.length() > 1)
        {
            player_append = player_append.replace(QRegExp("%s"), QString("%1").arg(current_title->getSubTitle()));
            player_string += " ";
            player_string += player_append;
        }
    }
    // cout << "PLAYER STRING: " << player_string << endl;

    myth_system(player_string);
    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    
}

void TitleDialog::ripTitles()
{
    //
    //  OK, the user has selected whatever they want
    //  We just need to build some job strings to 
    //  send to the mtd, send them out the socket,
    //  and quit.
    //
    
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i)->getSelected())
        {
            //
            //  The spec for this command, which I'm
            //  making up on the fly is:
            //  
            //  job 
            //  {type} 
            //  {title #} 
            //  {audio_track} 
            //  {quality} 
            //  {ac3 flag} 
            //  {subtitle #} 
            //  {directory to end up in and final name} 
            //
            //  note that everything after subtitle #
            //  may include spaces (for subdirs and/or
            //  riptitle that includes spaces)
            //
            
            
            //
            //  we need to ask mythvideo stuff where it lives
            //
           
            QString destination_directory = gContext->GetSetting("mythdvd.LocalRipDirectory");

            if (!destination_directory.length()) {
                destination_directory = gContext->GetSetting("VideoStartupDir");
            }
 
            if(destination_directory.length() < 1)
            {
                cerr << "titledialog.o: I can't rip, as I have nowhere to put finished files. MythVideo installed?" << endl;
                return; 
            }            
            
            QString final_dir_and_file = destination_directory + "/" + dvd_titles->at(i)->getName();
            
            QString job_string = QString("job dvd %1 %2 %3 %4 %5 %6")
                                 .arg(dvd_titles->at(i)->getTrack())
                                 .arg(dvd_titles->at(i)->getAudio())
                                 .arg(dvd_titles->at(i)->getQuality())
                                 .arg(dvd_titles->at(i)->getAC3())
                                 .arg(dvd_titles->at(i)->getSubTitle())
                                 .arg(final_dir_and_file);
            
            QTextStream os(socket_to_mtd);
            os << job_string << "\n" ;
        }
    }
    done(0);
}

void TitleDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);
    if(name_editor)
    {
        name_editor->clearFocus();
    }
}

void TitleDialog::wireUpTheme()
{
    ripcheck = getUICheckBoxType("ripcheck");
    if(ripcheck)
    {
        connect(ripcheck, SIGNAL(pushed(bool)), this, SLOT(toggleTitle(bool)));
    }
        
    next_title_button = getUIPushButtonType("next_title_button");
    if(next_title_button)
    {
        next_title_button->allowFocus(false);
        connect(next_title_button, SIGNAL(pushed()), this, SLOT(nextTitle()));
    }

    prev_title_button = getUIPushButtonType("prev_title_button");
    if(prev_title_button)
    {
        prev_title_button->allowFocus(false);
        connect(prev_title_button, SIGNAL(pushed()), this, SLOT(prevTitle()));
    }

    playlength_text = getUITextType("playlength_text");
    editor_hack = getUIBlackHoleType("editor_hack");
    if(editor_hack)
    {
        editor_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        name_editor = new MythRemoteLineEdit(&f, this);
        name_editor->setFocusPolicy(QWidget::NoFocus);
        name_editor->setGeometry(editor_hack->getScreenArea());
        connect(editor_hack, SIGNAL(takingFocus()), 
                name_editor, SLOT(setFocus()));
        connect(name_editor, SIGNAL(tryingToLooseFocus(bool)), 
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(name_editor, SIGNAL(textChanged(QString)),
                this, SLOT(changeName(QString)));
    }

    ripaway_button = getUITextButtonType("ripaway_button");
    if(ripaway_button)
    {
        ripaway_button->setText(tr("0 Process Selected Title"));
        ripaway_button->allowFocus(false);
        connect(ripaway_button, SIGNAL(pushed()), this, SLOT(ripTitles()));
    }

    audio_select = getUISelectorType("audio_select");
    if(audio_select)
    {
        connect(audio_select, SIGNAL(pushed(int)), this, SLOT(setAudio(int)));
    }

    quality_select = getUISelectorType("quality_select");
    if(quality_select)
    {
        connect(quality_select, SIGNAL(pushed(int)), this, SLOT(setQuality(int)));
    }

    subtitle_select = getUISelectorType("subtitle_select");
    if(subtitle_select)
    {
        connect(subtitle_select, SIGNAL(pushed(int)), this, SLOT(setSubTitle(int)));
    }
    ripacthree = getUICheckBoxType("ripacthree");
    if(ripacthree)
    {
        connect(ripacthree, SIGNAL(pushed(bool)), this, SLOT(toggleAC3(bool)));
    }
    
    view_button = getUIPushButtonType("view_button");
    if(view_button)
    {
        connect(view_button, SIGNAL(pushed()), this, SLOT(viewTitle()));
    }
        
    numb_titles_text = getUITextType("numb_titles_text");
    buildFocusList();
}

TitleDialog::~TitleDialog()
{
    if(name_editor)
    {
        delete name_editor;
    }
}

