/*
	audioclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's audio playing service

*/

#include <iostream>
using namespace std;

#include "audioclient.h"


AudioClient::AudioClient(
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            MFD_SERVICE_AUDIO_CONTROL,
                            l_ip_address,
                            l_port
                          )
{
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
    cout << "getting these tokens: " << tokens.join(" ") << endl; 
}

AudioClient::~AudioClient()
{
}




