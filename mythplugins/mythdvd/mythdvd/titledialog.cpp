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

#include "titledialog.h"


TitleDialog::TitleDialog(QSocket *a_socket, QString d_name, QPtrList<DVDTitleInfo> *titles, MythMainWindow *parent, const char* name)
            :MythDialog(parent, name)
{
    socket_to_mtd = a_socket;
    disc_name = d_name;
    if(disc_name.length() < 1)
    {
        disc_name = "Unkown";
    }
    dvd_titles = titles;

    //
    //  Make widgets
    //  
    
    bigvb = new QVBoxLayout(this, 0);
    
    firstdiag = new QFrame(this);
    firstdiag->setPalette(palette());
    firstdiag->setFont(font());
    
    bigvb->addWidget(firstdiag, 1); 
    
    
    vbox = new QVBoxLayout(firstdiag, (int)(24 * wmult));
    info_text = new QLabel("Please select titles, audio tracks, and "
                           "quality levels:", firstdiag);
    info_text->setAlignment(Qt::AlignHCenter);
    info_text->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(info_text);

    //
    //  First, we need to find the longest title (playing
    //  time) so we can set some defaults based on that.
    //
    
    uint longest = 0;
    uint longest_time = 0;
    
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        if(dvd_titles->at(i)->getPlayLength() > longest_time)
        {
            longest = i;
            longest_time = dvd_titles->at(i)->getPlayLength();
        }
    }
    
    QGridLayout *title_grid = new QGridLayout(vbox, dvd_titles->count(), 6);
    
    for(uint i = 0; i < dvd_titles->count(); i++)
    {
        //
        //  Build a row of settings for each title on the DVD
        //

        MythCheckBox *toggle = new MythCheckBox(firstdiag, QString("%1").arg(i));
        connect(toggle, SIGNAL(toggled(bool)), this, SLOT(toggleTitle(bool)));        

        MythRemoteLineEdit *new_edit = new MythRemoteLineEdit(firstdiag, QString("%1").arg(i));
        connect(new_edit, SIGNAL(textChanged(QString)), this, SLOT(changeName(QString)));
        if(i == longest)
        {
            new_edit->setText(disc_name);
            dvd_titles->at(i)->setName(disc_name);
            new_edit->setFocus();
            toggle->setChecked(true);
            dvd_titles->at(i)->setSelected(true);
        }
        else
        {
            new_edit->setText(QString("%1 - Title %2").arg(disc_name).arg(i + 1));
            dvd_titles->at(i)->setName(QString("%1 - Title %2").arg(disc_name).arg(i + 1));
        }
        title_grid->addWidget(toggle, i, 0);
        title_grid->addWidget(new_edit, i, 1);

        QLabel *length_label = new QLabel(dvd_titles->at(i)->getTimeString(), firstdiag);
        length_label->setBackgroundOrigin(WindowOrigin);
        length_label->setAlignment(Qt::AlignRight);
        title_grid->addWidget(length_label, i , 2);

        MythComboBox *audio = new MythComboBox(false, firstdiag, QString("%1").arg(i));
        connect(audio, SIGNAL(highlighted(int)), this, SLOT(setAudio(int)));
        QPtrList<DVDAudioInfo> *audio_tracks = dvd_titles->at(i)->getAudioTracks();
        for(uint j = 0; j < audio_tracks->count(); j++)
        {
            audio->insertItem(audio_tracks->at(j)->getAudioString());
        }
        title_grid->addWidget(audio, i, 3);

        MythComboBox *quality = new MythComboBox(false, firstdiag, QString("%1").arg(i));
        //  HACK -- perfect only
        quality->insertItem("Perfect");
        connect(quality, SIGNAL(highlighted(int)), this, SLOT(setQuality(int)));
        title_grid->addWidget(quality, i, 4);
       

        MythPushButton *view = new MythPushButton(firstdiag, QString("%1").arg(i + 1));
        connect(view, SIGNAL(pressed()), this, SLOT(viewTitle()));
        view->setText("View");
        title_grid->addWidget(view, i , 5);

    }    

    MythPushButton *ripit = new MythPushButton("Import Title(s)", firstdiag);
    connect(ripit, SIGNAL(pressed()), this, SLOT(ripTitles()));
    vbox->addWidget(ripit);

}

void TitleDialog::setQuality(int qual_index)
{
    MythComboBox *which_box = (MythComboBox *)sender();
    int which_title = atoi(which_box->name());
    dvd_titles->at(which_title)->setQuality(qual_index);
}

void TitleDialog::setAudio(int audio_index)
{
    MythComboBox *which_box = (MythComboBox *)sender();
    int which_title = atoi(which_box->name());
    dvd_titles->at(which_title)->setAudio(audio_index + 1);
}

void TitleDialog::toggleTitle(bool on_or_off)
{
    MythCheckBox *which_box = (MythCheckBox *)sender();
    int which_title = atoi(which_box->name());
    dvd_titles->at(which_title)->setSelected(on_or_off);
}

void TitleDialog::changeName(QString new_name)
{
    MythRemoteLineEdit *which_edit = (MythRemoteLineEdit *)sender();
    int which_title = atoi(which_edit->name());
    dvd_titles->at(which_title)->setName(new_name);
}

void TitleDialog::viewTitle()
{
    MythPushButton *which_button = (MythPushButton *)sender();

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
    
    QString title_string = which_button->name();
    int which_title = title_string.toInt() - 1;
    
    DVDTitleInfo *title_in_question = dvd_titles->at(which_title);
    
    int audio_track = 1;
    int channels = 2;
    if(title_in_question)
    {
        audio_track = title_in_question->getAudio();
        DVDAudioInfo *audio_in_question = title_in_question->getAudioTrack(audio_track - 1);
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
    player_string = player_string.replace(QRegExp("%t"), which_button->name());
    player_string = player_string.replace(QRegExp("%a"), QString("%1").arg(audio_track));
    player_string = player_string.replace(QRegExp("%c"), QString("%1").arg(channels));

    system(player_string);
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
            //  job {type} {title #} {audio_track} {quality} {directory to end up in} {name}
            //
            
            
            //
            //  we need to ask mythvideo stuff where it lives
            //
            
            QString destination_directory = gContext->GetSetting("VideoStartupDir");
            if(destination_directory.length() < 1)
            {
                cerr << "titledialog.o: I can't rip, as I have nowhere to put finished files. MythVideo installed?" << endl;
                return; 
            }            
            
            QString job_string = QString("job dvd %1 %2 %3 %4 %5")
                                 .arg(dvd_titles->at(i)->getTrack())
                                 .arg(dvd_titles->at(i)->getAudio())
                                 .arg(dvd_titles->at(i)->getQuality())
                                 .arg(destination_directory)
                                 .arg(dvd_titles->at(i)->getName());
            
            QTextStream os(socket_to_mtd);
            os << job_string << "\n" ;
            done(0);
        }
    }
    
}

TitleDialog::~TitleDialog()
{
}

