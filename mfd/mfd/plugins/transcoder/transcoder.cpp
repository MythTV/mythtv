/*
    transcoder.cpp

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include "transcoder.h"
#include "settings.h"

Transcoder *transcoder = NULL;


Transcoder::Transcoder(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, mfdContext->getNumSetting("mfd_transcoder_port"), "transcoder", 2)
{

    //
    //  Register this (mtcp) service
    //

    Service *mtcp_service = new Service(
                                        QString("MythTranscoder on %1").arg(hostname),
                                        QString("mtcp"),
                                        hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *mtcp_service);
    QApplication::postEvent(parent, se);
    delete mtcp_service;
}



Transcoder::~Transcoder()
{
}

