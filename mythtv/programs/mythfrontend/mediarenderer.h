//////////////////////////////////////////////////////////////////////////////
// Program Name: mediarenderer.h
//                                                                            
// Purpose - uPnp Media Renderer main Class
//                                                                            
// Created By  : David Blain                    Created On : Jan. 25, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef MEDIARENDERER_H
#define MEDIARENDERER_H

#include <QObject>
#include <QMutex>

#include "upnp.h"
#include "upnpcmgr.h"
#include "mythxmlclient.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MediaRenderer : public UPnp
{
    protected:

      //UPnpControl     *m_pUPnpControl {nullptr};  // Do not delete (auto deleted)
        UPnpCMGR        *m_pUPnpCMGR    {nullptr};  // Do not delete (auto deleted)

    public:
                 MediaRenderer();
        ~MediaRenderer() override;
};

#endif // MEDIARENDERER_H
