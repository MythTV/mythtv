/*
	audioclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's audio playing service

*/

#include <iostream>
using namespace std;

#include <qapplication.h>

#include "audioclient.h"
#include "events.h"
#include "mfdinterface.h"


AudioClient::AudioClient(
                            MfdInterface *the_mfd,
                            int an_mfd,
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            the_mfd,
                            an_mfd,
                            MFD_SERVICE_AUDIO_CONTROL,
                            l_ip_address,
                            l_port,
                            "audio"
                          )
{
    speakers.setAutoDelete(true);
}

void AudioClient::playTrack(int container, int id)
{
    QString command = QString("play item %1 %2 \n")
                              .arg(container)
                              .arg(id);

    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::playList(int container, int id, int index)
{
    QString command = QString("play list %1 %2 %3 \n")
                              .arg(container)
                              .arg(id)
                              .arg(index);

    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::stopAudio()
{
    QString command = QString("stop \n");
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::pauseAudio(bool y_or_n)
{
    QString command;
    if (y_or_n)
    {
        command = QString("pause on \n");
    }
    else
    {
        command = QString("pause off \n");
    }

    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::seekAudio(int how_much)
{   
    QString command = QString("seek %1 \n").arg(how_much);
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::nextAudio()
{   
    QString command = QString("next \n");
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::prevAudio()
{   
    QString command = QString("prev \n");
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::handleIncoming()
{
    //
    //  We're getting something about the state of audio play.
    //
    
    char in_buffer[2049];
    
    int amount_read = client_socket_to_service->readBlock(in_buffer, 2048);
    if (amount_read < 0)
    {
        cerr << "audioclient.o: error reading from service" << endl;
        return;
    }
    else if (amount_read == 0)
    {
        return;
    }
    else
    {
        in_buffer[amount_read] = '\0';
        QString incoming_text = QString(in_buffer);

        incoming_text.simplifyWhiteSpace();

        QStringList line_by_line = QStringList::split("\n", incoming_text);

        for(uint i = 0; i < line_by_line.count(); i++)
        {
            QStringList tokens = QStringList::split(" ", line_by_line[i]);
            parseFromAudio(tokens);
        }
    }
    
}

void AudioClient::parseFromAudio(QStringList &tokens)
{
    if (tokens.count() < 1)
    {
        cerr << "audioclient.o: got no tokens to parse" 
             << endl;
        return;
    }
    
    if (tokens[0] == "pause")
    {
        if (tokens.count() < 2)
        {
            cerr << "audioclient.o: got pause token, but no on or off"
                 << endl;
            return;
        }
        if (tokens[1] == "on")
        {
            MfdAudioPausedEvent *ape = new MfdAudioPausedEvent(mfd_id, true);
            QApplication::postEvent(mfd_interface, ape);
            return;
        }
        else if (tokens[1] == "off")
        {
            MfdAudioPausedEvent *ape = new MfdAudioPausedEvent(mfd_id, false);
            QApplication::postEvent(mfd_interface, ape);
            return;
        }
        cerr << "audioclinet.cpp: don't understand these tokens: "
             << tokens.join(" ")
             << endl;
        return;
    }
    
    if (tokens[0] == "stop")
    {
        MfdAudioStoppedEvent *ase = new MfdAudioStoppedEvent(mfd_id);
        QApplication::postEvent(mfd_interface, ase);
        return;
    }
    
    if (tokens[0] == "playing")
    {
        if (tokens.count() < 9)
        {
            cerr << "audio server seems to be playing, but it's not "
                 << "sending the correct number of tokens"
                 << endl;
            return;
        }

        MfdAudioPlayingEvent *ape = new MfdAudioPlayingEvent(
                                        mfd_id,
                                        tokens[1].toInt(),
                                        tokens[2].toInt(),
                                        tokens[3].toInt(),
                                        tokens[4].toInt(),
                                        tokens[5].toInt(),
                                        tokens[6].toInt(),
                                        tokens[7].toInt(),
                                        tokens[8].toInt()
                                                            );
        QApplication::postEvent(mfd_interface, ape);
        return;
    }
    
    if (tokens[0] == "speakerlist")
    {
        //
        //  Getting a list of speakers available.
        //
        
        syncSpeakerList(tokens);
        return;
        
    }
    
    if (tokens[0] == "speakername")
    {
        nameSpeakers(tokens);
        return;
    }
    
    cerr << "getting tokens from audio server I don't understand: "
         << tokens.join(" ")
         << endl;
}

void AudioClient::executeCommand(QStringList new_command)
{
    if (new_command.count() < 1)
    {
        cerr << "audioclient.o: asked to executeCommand(), "
             << "but got no commands."
             << endl;
        return;
    }
    
    if (new_command[0] == "play")
    {
        //
        //  Make sure we have enough tokens for a "play" command
        //  
        
        if (new_command.count() < 5)
        {
            cerr << "audioclient.o: asked to play audio, but not "
                 << "enough tokens provided to know what to play."
                 << endl;
            return;
        }

        //
        //  Convert the string tokens to useful variables
        //

        bool ok = true;

        int container = new_command[1].toInt(&ok);
        if (container < 0 || !ok)
        {
            cerr << "audioclient.o: error converting play command's "
                 << "container token." 
                 << endl;
            return;
        }
        
        int type = new_command[2].toInt(&ok);
        if (type < 1 || type > 2 || !ok)
        {
            cerr << "audioclient.o: error converting play command's "
                 << "type token." 
                 << endl;
            return;
        }
        
        int which_id = new_command[3].toInt(&ok);
        if (which_id < 0 || !ok)
        {
            cerr << "audioclient.o: error converting play command's "
                 << "which_id token." 
                 << endl;
            return;
        }
        
        int index = new_command[4].toInt(&ok);
        if (index < 0 || !ok)
        {
            cerr << "audioclient.o: error converting play command's "
                 << "index token." 
                 << endl;
            return;
        }
        
        //
        //  Either basic metadata item (type = 1) or an entry in a playlist (type = 2)
        //
        
        if (type == 1)
        {
            playTrack(container, which_id);
        }
        else if (type == 2)
        {
            playList(container, which_id, index);
        }
        
    }
    else if (new_command[0] == "stop")
    {
        //
        //  Stop the audio 
        //
        
        stopAudio();        
    }
    else if (new_command[0] == "pause")
    {
        //
        //  Turn audio pause on or off
        //
        
        if (new_command.count() < 2)
        {
            cerr << "audioclient.o: pause is a useless command "
                 << "unless you add \"on\" or \"off\"."
                 << endl;
            return;
        }

        if (new_command[1] == "on")
        {
            pauseAudio(true);
        }
        else if (new_command[1] == "off")
        {
            pauseAudio(false);
        }
        else
        {
            cerr << "audioclient.o: got pause command with "
                 << "neither \"on\" nor \"off\"."
                 << endl;
        }
    }
    else if (new_command[0] == "seek")
    {
        //
        //  Seek some number of seconds
        //
        
        if (new_command.count() < 2)
        {
            cerr << "audioclient.o: seek is a useless command "
                 << "without amount."
                 << endl;
            return;
        }

        bool ok = true;
        int amount = new_command[1].toInt(&ok);
        if (!ok)
        {
            cerr << "audioclient.o: could not parse seek amount"
                 << endl;
            return;
        }

        seekAudio(amount);
    }    
    else if (new_command[0] == "next")
    {
        //
        //  Next track
        //
        
        nextAudio();
    }    
    else if (new_command[0] == "prev")
    {
        //
        //  Next track
        //
        
        prevAudio();
    }    
    else if (new_command[0] == "status")
    {
        //
        //  Send status
        //

        askForStatus();
    }  
    else if (new_command[0] == "speakeruse")
    {
        //
        //  Toggle speakers
        //
        
        toggleSpeakers(new_command.join(" "));
    }      
    else
    {
        cerr << "audioclient.o: I don't understand this "
             << "command: \""
             << new_command.join(" ")
             << "\""
             << endl;
    }
    
}

void AudioClient::askForStatus()
{
    QString command = QString("status \n");
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::toggleSpeakers(const QString &full_command)
{
    QString command = QString("%1\n").arg(full_command);
    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::syncSpeakerList(QStringList &tokens)
{
    //
    //  Mark all my speakers for deletion
    //    
    
    SpeakerTracker *a_speaker;
    QPtrListIterator<SpeakerTracker> iter( speakers );

    while ( (a_speaker = iter.current()) != 0 )
    {
        a_speaker->markForDeletion(true);
        ++iter;
    }

    //
    //  Unmark any that are actually in existence
    //
    
    for (uint i = 1; i < tokens.count() - 1; i = i + 2)
    {
        bool found_it = false;
        a_speaker = iter.toFirst();
        while ( (a_speaker = iter.current()) != 0 )
        {
            if (a_speaker->possiblyUnmarkForDeletion(tokens[i], tokens[i+1]))
            {
                found_it = true;
            }
            ++iter;
        }
        if (!found_it)
        {
            bool ok;
            int new_id = tokens[i].toInt(&ok);
            if (ok)
            {
                SpeakerTracker *new_speaker = new SpeakerTracker(new_id, tokens[i + 1]);
                speakers.append(new_speaker);
            }
            else
            {
                cerr << "audio client is getting bad characters that cannot be "
                     << "parsed into a speaker id: \""
                     << tokens[i]
                     << "\""
                     << endl;
            }
        }
    }

    //
    //  Delete those still marked for deletion
    //
    
    a_speaker = iter.toFirst();
    while ( (a_speaker = iter.current()) != 0 )
    {
        if (a_speaker->markedForDeletion())
        {
            speakers.remove(a_speaker);
        }
        else
        {
            ++iter;
        }
    }
    
    
    //
    //  Find first one that does not have a name, make a name request for it
    //

    a_speaker = iter.toFirst();
    while ( (a_speaker = iter.current()) != 0 )
    {
        if (! a_speaker->haveName())
        {
            QString command = QString("speakername %1\n").arg(a_speaker->getId());
            client_socket_to_service->writeBlock(command.ascii(), command.length());
            break;
        }
        ++iter;
    }

    //
    //  Each time we get a list may be the last, so we send an event
    //
    
    MfdSpeakerListEvent *sle = new MfdSpeakerListEvent(mfd_id, &speakers);
    QApplication::postEvent(mfd_interface, sle);
}

void AudioClient::nameSpeakers(QStringList &tokens)
{

    if(tokens.count() < 2)
    {
        cerr << "AudioClient::nameSpeakers() can't do so if given no speakers"
             << endl;
        return;
    }
    
    bool ok;
    
    int target_id = tokens[1].toInt(&ok);
    
    if(!ok)
    {
        cerr << "AudioClient::nameSpeakers() got bogus speaker id of \""
             << tokens[1]
             << "\""
             << endl;
        return;
    }
    

    //
    //  Find the speaker in question
    //    
    
    SpeakerTracker *a_speaker;
    SpeakerTracker *target_speaker = NULL;
    QPtrListIterator<SpeakerTracker> iter( speakers );

    while ( (a_speaker = iter.current()) != 0 )
    {
        if (target_id == a_speaker->getId())
        {
            target_speaker = a_speaker;
            break;
        }
        ++iter;
    }

    if (!target_speaker)
    {
        cerr << "AudioClient::nameSpeakers() got reference to non-existant "
             << "speaker"
             << endl;
        return;
    }

    
    QString new_name;
    if(tokens.count() > 2)
    {
        for (uint i = 2; i < tokens.count(); i++)
        {
            if(i > 2)
            {
                new_name.append(" ");
            }
            new_name.append(tokens[i]);
        }
    }
    else
    {
        new_name = "noname";
    }
    
    target_speaker->setName(new_name);


    
    /*
    
    //
    //  Debugging
    //
    
    cout << "speaker status: "
         << endl << endl;
         
    a_speaker = iter.toFirst();
    while ( (a_speaker = iter.current()) != 0 )
    {
        cout << "speaker with id of "
             << a_speaker->getId()
             << " is called \""
             << a_speaker->getName()
             << "\" and is marked for use as: "
             << a_speaker->getInUse()
             << endl;
        ++iter;
    }
    
    */
    

    //
    //  Find first one that does not have a name, make a name request for it
    //

    a_speaker = iter.toFirst();
    while ( (a_speaker = iter.current()) != 0 )
    {
        if (! a_speaker->haveName())
        {
            QString command = QString("speakername %1\n").arg(a_speaker->getId());
            client_socket_to_service->writeBlock(command.ascii(), command.length());
            break;
        }
        ++iter;
    }

    
    
    //
    //  Each time we get a name may be the last, so we send an event
    //
    
    MfdSpeakerListEvent *sle = new MfdSpeakerListEvent(mfd_id, &speakers);
    QApplication::postEvent(mfd_interface, sle);
}

AudioClient::~AudioClient()
{
    speakers.clear();
}




