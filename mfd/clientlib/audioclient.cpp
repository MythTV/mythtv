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
                            l_port
                          )
{
}

void AudioClient::playTrack(int container, int id)
{
    QString command = QString("play item %1 %2")
                              .arg(container)
                              .arg(id);

    client_socket_to_service->writeBlock(command.ascii(), command.length());
}

void AudioClient::handleIncoming()
{
    //
    //  We're getting something about the state of audio play.
    //
    
    char in_buffer[2049];
    
    int amount_read = client_socket_to_service->readBlock(in_buffer, 2048);
    if(amount_read < 0)
    {
        cerr << "audioclient.o: error reading from service" << endl;
        return;
    }
    else if(amount_read == 0)
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
    if(tokens.count() < 1)
    {
        cerr << "audioclient.o: got no tokens to parse" 
             << endl;
        return;
    }
    
    if(tokens[0] == "pause")
    {
        if(tokens.count() < 2)
        {
            cerr << "audioclient.o: got pause token, but no on or off"
                 << endl;
            return;
        }
        if(tokens[1] == "on")
        {
            MfdAudioPausedEvent *ape = new MfdAudioPausedEvent(mfd_id, true);
            QApplication::postEvent(mfd_interface, ape);
            return;
        }
        else if(tokens[1] == "off")
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
    
    if(tokens[0] == "stop")
    {
        MfdAudioStoppedEvent *ase = new MfdAudioStoppedEvent(mfd_id);
        QApplication::postEvent(mfd_interface, ase);
        return;
    }
    
    if(tokens[0] == "playing")
    {
        if(tokens.count() < 9)
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
    
    cerr << "getting tokens from audio server I don't understand: "
         << tokens.join(" ")
         << endl;
}

AudioClient::~AudioClient()
{
}




