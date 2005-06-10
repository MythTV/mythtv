/*
	transcoderclient.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's transcoder service

*/

#include <iostream>
using namespace std;

#include <qapplication.h>

#include "transcoderclient.h"
#include "jobtracker.h"
#include "events.h"
#include "mfdinterface.h"
#include "../mfdlib/httpoutrequest.h"
#include "../mfdlib/httpinresponse.h"
#include "../mtcplib/markupcodes.h"
#include "../mtcplib/mtcpinput.h"

TranscoderClient::TranscoderClient(
                            MfdInterface *the_mfd,
                            int an_mfd,
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            the_mfd,
                            an_mfd,
                            MFD_SERVICE_TRANSCODER,
                            l_ip_address,
                            l_port,
                            "transcoder"
                          )
{
    generation = 0;
    job_list.setAutoDelete(true);
}

void TranscoderClient::executeCommand(QStringList new_command)
{

    if(new_command.count() < 1)
    {
        cerr << "no tokens to TranscoderClient::executeCommand(): "
             << new_command.join(" ")
             << endl;
        return;
    }

    if(new_command[0] == "ripaudiocd")
    {
        if(new_command.count() != 3)
        {
            cerr << "bad tokens to TranscoderClient::executeCommand(): "
                 << new_command.join(" ")
                 << endl;
            return;
        }
        bool ok;
        int which_collection = new_command[1].toInt(&ok);
        if(!ok)
        {
            cerr << "could not parse \""
                 << new_command[1]
                 << "\" into a collection number in "
                 << "TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        if(which_collection < 1)
        {
            cerr << "bad collection number ("
                 << which_collection
                 << ") is TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        int which_playlist = new_command[2].toInt(&ok);
        if(!ok)
        {
            cerr << "could not parse \""
                 << new_command[2]
                 << "\" into a playlist number in "
                 << "TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        if(which_playlist < 1)
        {
            cerr << "bad playlist number ("
                 << which_playlist
                 << ") is TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        requestRipAudioCd(which_collection, which_playlist);
    }
    else if(new_command[0] == "canceljob")
    {
        if(new_command.count() != 2)
        {
            cerr << "bad number of tokens to TranscoderClient::executeCommand(): "
                 << new_command.join(" ")
                 << endl;
            return;
        }
        bool ok;
        int which_job = new_command[1].toInt(&ok);
        if(!ok)
        {
            cerr << "could not parse \""
                 << new_command[1]
                 << "\" into a job number in "
                 << "TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        if(which_job < 1)
        {
            cerr << "bad job number ("
                 << which_job
                 << ") is TranscoderClient::executeCommand()"
                 <<endl;
            return;
        }
        requestCancelJob(which_job);
    }
    else
    {
        cerr << "bad command to TranscoderClient::executeCommand()" << endl;
    }
}

void TranscoderClient::handleIncoming()
{
    
    static int buffer_size = 8192;
    char incoming[buffer_size];
    int length = 0;

    HttpInResponse *new_response = NULL;
    
    length = client_socket_to_service->readBlock(incoming, buffer_size);
    
    while(length > 0)
    {
        if(!new_response)
        {
            new_response = new HttpInResponse(incoming, length);
        }
        else
        {
            new_response->appendToPayload(incoming, length);
        }

        if(new_response->complete())
        {
            length = 0;
        }
        else
        {
            if(new_response->allIsWell())
            {
                bool wait_a_while = true;
                while(wait_a_while)
                {
                    bool server_still_there = true;
                    
                    if(client_socket_to_service->waitForMore(1000, &server_still_there) < 1)
                    {
                        if(!server_still_there)
                        {
                            new_response->allIsWell(false);
                            wait_a_while = false;
                            length = 0;
                        }
                    }
                    else
                    {
                        length = client_socket_to_service->readBlock(incoming, buffer_size);
                        if(length > 0)
                        {
                            wait_a_while = false;
                        }
                    }
                }
            }
            else
            {
                length = 0;
            }
        }
    }
    
    if(new_response)
    {
        if(new_response->allIsWell())
        {
            processResponse(new_response);
            delete new_response;
            new_response = NULL;
        }
        else
        {
            cerr << "got an error on an http response in transcoder" << endl;
        }
    }
}



void TranscoderClient::requestRipAudioCd(int collection_id, int playlist_id)
{
    HttpOutRequest request("/startjob", ip_address);
    request.addHeader("Job-Type: AudioCD");
    request.addHeader(QString("Container: %1").arg(collection_id));
    request.addHeader(QString("Playlist: %1").arg(playlist_id));
    request.send(client_socket_to_service);
}

void TranscoderClient::requestCancelJob(int job_id)
{
    HttpOutRequest request("/canceljob", ip_address);
    request.addHeader(QString("Job: %1").arg(job_id));
    request.send(client_socket_to_service);
}

void TranscoderClient::processResponse(HttpInResponse *httpin_response)
{

    //
    //  Sanity check on the version number of the protocol
    //

    QString protocol_version_string = httpin_response->getHeader("MTCP-Server");
    if(protocol_version_string.length() > 0)
    {
        QString substring = protocol_version_string.section("/", 1, 1);
        substring = substring.section(" ", 0, 0);
        bool ok;
        int protocol_major = substring.section(".", 0, 0).toInt(&ok);
        if(ok)
        {
            int protocol_minor = substring.section(".", 1, 1).toInt(&ok);
            if(ok)
            {
                if(
                    protocol_major == MTCP_PROTOCOL_VERSION_MAJOR &&
                    protocol_minor == MTCP_PROTOCOL_VERSION_MINOR
                  )
                {
                    //
                    //  Cool
                    //
                }
                else
                {
                    cerr << "transcoderclient.o: mtcp protocol mismatch" 
                         << endl;
                }          
            }
            else
            {
                cerr << "transcoderclient.o: can't parse mtcp protocol version from "
                     << "headers" << endl;
            }
        }
        else
        {
            cerr << "transcoderclient.o: can't parse protocol version from "
                 << "mtcp headers" << endl;
        }
    }
    else
    {
        cerr << "transcoderclient.o: mtcp server has no MTCP-Server header, "
             << "can't determine protocol version"
             << endl;
    }

    //
    //  Get the generation number of the server (a status number that
    //  changes whenever something changes in a transcoding job)
    //

    QString generation_string = httpin_response->getHeader("Server-Generation");
    if(generation_string.length() > 0)
    {
        bool ok;
        int generation_number = generation_string.toInt(&ok);
        if(ok)
        {
            generation = generation_number;
        }
        else
        {
            cerr << "transcoderclient.o: could not parse \""
                 <<  generation_string
                 << "\" into an int (generation number), which "
                 << "is a disaster"
                 << endl;
        }
        
    }
    else
    {
        cerr << "transcoderclient.o: the server did NOT send a "
             << "Server-Generation header, which is very bad."
             << endl;
    }

    //
    //  Clear out any previous job history
    //

    job_list.clear();

    //
    //  Create an mtcp input object with the payload from the response
    //
    
    MtcpInput mtcp_input(httpin_response->getPayload());
    
    char first_tag = mtcp_input.peekAtNextCode();

    if(first_tag == MtcpMarkupCodes::server_info_group)
    {
        parseServerInfo(mtcp_input);
    }
    else
    {
        cerr << "transcoderclient.o: did not understand first markup code "
             << "in a mtcp payload ("
             << (int) first_tag
             << "), giving up"
             << endl;
        return;
    }

    //
    //  Build an send an event that has the current state of all the jobs
    //
    
    MfdTranscoderJobListEvent *jle = new MfdTranscoderJobListEvent(mfd_id, &job_list);
    QApplication::postEvent(mfd_interface, jle);

    sendStatusRequest();
    
    
}

void TranscoderClient::sendFirstRequest()
{
    sendStatusRequest();
}

void TranscoderClient::sendStatusRequest()
{
    //
    //  We get the client-server communications to the metadata server
    //  started by making a mdcap /server-info request
    //

    HttpOutRequest first_request("/status", ip_address);   
    first_request.addHeader(QString("Client-Generation: %1").arg(generation));
    first_request.send(client_socket_to_service); 
}

void TranscoderClient::parseServerInfo(MtcpInput &mtcp_input)
{

    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mtcp_input.popGroup(group_contents);
    
    if(group_code != MtcpMarkupCodes::server_info_group)
    {
        cerr << "transcoderclient.o: asked to parseServerInfo(), but "
             << "group code was not server_info_group ("
             << (int) group_code
             << ")"
             << endl;
        delete group_contents;
        return;
    }
    


    MtcpInput rebuilt_internals(group_contents);

    QString new_service_name;
    int     new_protocol_major;
    int     new_protocol_minor;
    
    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MtcpMarkupCodes::name:
                new_service_name = rebuilt_internals.popName();
                break;
                
            case MtcpMarkupCodes::protocol_version:
                rebuilt_internals.popProtocol(
                                                &new_protocol_major, 
                                                &new_protocol_minor
                                             );
                                             
            case MtcpMarkupCodes::job_info_group:
                parseJobInfo(rebuilt_internals);
                break;
            
            default:
                cerr << "transcoderclient.o getting content codes I don't "
                     << "understand while doing parseServerInfo() ("
                     << (int) content_code
                     << ")"
                     << endl;
                all_is_well = false;

            break;
        }
    }
}

void TranscoderClient::parseJobInfo(MtcpInput &mtcp_input)
{

    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mtcp_input.popGroup(group_contents);
    
    if(group_code != MtcpMarkupCodes::job_info_group)
    {
        cerr << "transcoderclient.o: asked to parseJobInfo(), but "
             << "group code was not job_info_group ("
             << (int) group_code
             << ")"
             << endl;
        delete group_contents;
        return;
    }
    


    MtcpInput rebuilt_internals(group_contents);

    uint32_t     new_job_count = 0;
    
    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MtcpMarkupCodes::job_count:
                new_job_count = rebuilt_internals.popJobCount();
                break;
                
            case MtcpMarkupCodes::job_group:
                parseJob(rebuilt_internals);
                break;
            
            default:
                cerr << "transcoderclient.o getting content codes I don't "
                     << "understand while doing parseJobInfo() ("
                     << (int) content_code
                     << ")"
                     << endl;
                all_is_well = false;

            break;
        }
    }
}


void TranscoderClient::parseJob(MtcpInput &mtcp_input)
{

    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mtcp_input.popGroup(group_contents);
    
    if(group_code != MtcpMarkupCodes::job_group)
    {
        cerr << "transcoderclient.o: asked to parseJob(), but "
             << "group code was not job_group ("
             << (int) group_code
             << ")"
             << endl;
        delete group_contents;
        return;
    }
    
    MtcpInput rebuilt_internals(group_contents);

    uint32_t    new_job_id = 0;
    QString     new_job_major_description;
    uint32_t    new_job_major_progress = 0;
    QString     new_job_minor_description;
    uint32_t    new_job_minor_progress = 0;
    
    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MtcpMarkupCodes::job_id:
                new_job_id = rebuilt_internals.popJobId();
                break;
                
            case MtcpMarkupCodes::job_major_description:
                new_job_major_description = rebuilt_internals.popMajorDescription();
                break;
                
            case MtcpMarkupCodes::job_major_progress:
                new_job_major_progress = rebuilt_internals.popMajorProgress();
                break;
                
            case MtcpMarkupCodes::job_minor_description:
                new_job_minor_description = rebuilt_internals.popMinorDescription();
                break;
                
            case MtcpMarkupCodes::job_minor_progress:
                new_job_minor_progress = rebuilt_internals.popMinorProgress();
                break;
                
            default:
                cerr << "transcoderclient.o getting content codes I don't "
                     << "understand while doing parseJob() ("
                     << (int) content_code
                     << ")"
                     << endl;
                all_is_well = false;

            break;
        }
    }

    //
    //  Create a new job tracker object to store the state of this job
    //
    
    if(new_job_id > 0)
    {
        JobTracker *new_jt = new JobTracker(
                                            new_job_id,
                                            new_job_major_progress,
                                            new_job_minor_progress,
                                            new_job_major_description,
                                            new_job_minor_description
                                           );
        job_list.append(new_jt);
    }
    else
    {
        cerr << "transcoderclient.o: bad job id in mtcp payload" << endl;
    }
}

TranscoderClient::~TranscoderClient()
{
    job_list.clear();
}
