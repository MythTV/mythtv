//////////////////////////////////////////////////////////////////////////////
// Program Name: httpstatus.h
//
// Purpose - Html & XML status HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2STATUS_H_
#define V2STATUS_H_

// Qt
#include <QDomDocument>
#include <QMutex>
#include <QMap>

// MythTV
#include "libmythbase/http/mythhttpdata.h"
#include "libmythbase/http/mythhttpservice.h"
#include "libmythbase/http/mythmimedatabase.h"
#include "libmythbase/programinfo.h"

// MythBackend
#include "preformat.h"
#include "v2backendStatus.h"

class Scheduler;
class AutoExpire;
class EncoderLink;
class MainServer;

#define STATUS_SERVICE QString("/Status/")
#define STATUS_HANDLE  QString("Status")

class V2Status : public MythHTTPService
{

    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("Status",       "methods=GET,POST,HEAD")
    Q_CLASSINFO("xml",          "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetBackendStatus", "methods=GET,POST,HEAD")

    public:
        V2Status();
        ~V2Status() override  = default;
        static void RegisterCustomTypes();

    public slots:
        Preformat*         Status         ( ); // HTML
        Preformat*         GetStatusHTML  ( ); // HTML
        Preformat*         GetStatus ();  // XML
        Preformat*         xml ();        // XML
        V2BackendStatus*   GetBackendStatus(); // Standardized version of GetStatus

    private:

        Scheduler                   *m_pSched       {nullptr};
        QMap<int, EncoderLink *>    *m_pEncoders;
        MainServer                  *m_pMainServer  {nullptr};
        bool                         m_bIsMaster;
        int                          m_nPreRollSeconds;
        QMutex                       m_settingLock;

    private:

        void    FillStatusXML     ( QDomDocument *pDoc);

        static void    PrintStatus       ( QTextStream &os, QDomDocument *pDoc );
        static int     PrintEncoderStatus( QTextStream &os, const QDomElement& encoders );
        static int     PrintScheduled    ( QTextStream &os, const QDomElement& scheduled );
        static int     PrintFrontends    ( QTextStream &os, const QDomElement& frontends );
        static int     PrintBackends     ( QTextStream &os, const QDomElement& backends );
        static int     PrintJobQueue     ( QTextStream &os, const QDomElement& jobs );
        static int     PrintMachineInfo  ( QTextStream &os, const QDomElement& info );
        static int     PrintMiscellaneousInfo ( QTextStream &os, const QDomElement& info );

        static void    FillProgramInfo   ( QDomDocument *pDoc,
                                           QDomNode     &node,
                                           ProgramInfo  *pInfo,
                                           bool          bIncChannel = true,
                                           bool          bDetails    = true );

        static void    FillChannelInfo   ( QDomElement  &channel,
                                           ProgramInfo  *pInfo,
                                           bool          bDetails = true );
        void FillDriveSpace(V2MachineInfo* pMachineInfo);
};

#endif
