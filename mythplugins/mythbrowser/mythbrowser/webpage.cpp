/*
 * webpage.cpp
 *
 * Copyright (C) 2003
 *
 * Author:
 * - Philippe Cattin <cattin@vision.ee.ethz.ch>
 *
 * Bugfixes from:
 *
 * Translations by:
 *
 */
#include "webpage.h"

#include <stdlib.h>

#include <qdragobject.h>
#include <qpainter.h>
#include <qpaintdevicemetrics.h>
#include <qlayout.h>
#include <qlabel.h>

#include <kglobal.h>
#include <klocale.h>
#include <kkeydialog.h>
#include <kaccel.h>
#include <kio/netaccess.h>
#include <kconfig.h>
#include <kurl.h>
#include <kurlrequesterdlg.h>

#include <kstdaccel.h>
#include <kaction.h>
#include <kstdaction.h>

#include <khtml_part.h>
#include <khtmlview.h>
#include <kio/jobclasses.h>


using namespace std;

WebPage::WebPage (const char *location, int zoom, int width, int height, WFlags flags) : QWidget(NULL,NULL,flags)
{
    setPaletteBackgroundColor(Qt::white);
        
//    progressBar=NULL;
    browser=new KHTMLPart(this);

    // connect signal for link clicks
    connect( browser->browserExtension(),
         SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ), 
         this, SLOT( openURLRequest(const KURL &, const KParts::URLArgs & ) ) );

    // connect signals for the progress bar...
//    connect( browser,SIGNAL(started(KIO::Job*)),
//         this,SLOT(started(KIO::Job*)));
//    connect( browser,SIGNAL(completed()),
//         this,SLOT(completed()));

    // ...and enable the progress bar
    browser->setProgressInfoEnabled(true);
    browser->showProgressInfo(true);

    browser->setStandardFont("Arial");
    browser->openURL(location);

    browser->view()->resize(width,height);

    zoom = zoom<20 ? 20:zoom;
    zoom = zoom>300 ? 300:zoom;
    browser->setZoomFactor(zoom);
    zoomFactor=zoom;

    browser->setPluginsEnabled(false);
    browser->enableMetaRefresh(true);
    browser->setJavaEnabled(true);
    browser->enableJScript(true);

    if(DEBUG)
        qApp->installEventFilter(this);
}

WebPage::WebPage (const char *location, const KParts::URLArgs &args, int zoom, 
                  int width, int height, WFlags flags) : QWidget(NULL,NULL,flags)
{
    setPaletteBackgroundColor(Qt::white);

//    progressBar=NULL;
    browser=new KHTMLPart(this);

    // connect signal for link clicks
    connect( browser->browserExtension(),
         SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ), 
         this, SLOT( openURLRequest(const KURL &, const KParts::URLArgs & ) ) );

    // connect signals for the progress bar...
//    connect( browser,SIGNAL(started(KIO::Job*)),
//         this,SLOT(started(KIO::Job*)));
//    connect( browser,SIGNAL(completed()),
//         this,SLOT(completed()));

    // ...and enable the progress bar
    browser->setProgressInfoEnabled(true);
    browser->showProgressInfo(true);

    browser->setStandardFont("Arial");
    browser->browserExtension()->setURLArgs(args);
    browser->openURL(location);

    browser->view()->resize(width,height);

    zoom = zoom<20 ? 20:zoom;
    zoom = zoom>300 ? 300:zoom;
    browser->setZoomFactor(zoom);
    zoomFactor=zoom;

    browser->setPluginsEnabled(false);
    browser->enableMetaRefresh(true);
    browser->setJavaEnabled(true);
    browser->enableJScript(true);

    if(DEBUG)
        qApp->installEventFilter(this);
}

void WebPage::openURLRequest(const KURL &url, const KParts::URLArgs &args)
{
    emit newUrlRequested(url,args);
}

void WebPage::zoomOut()
{
    zoomFactor -= 20;
    zoomFactor = zoomFactor<20 ? 20:zoomFactor;
    browser->setZoomFactor(zoomFactor);
}

void WebPage::zoomIn()
{
    zoomFactor += 20;
    zoomFactor = zoomFactor>300 ? 300:zoomFactor;
    browser->setZoomFactor(zoomFactor);
}

bool WebPage::eventFilter(QObject* object, QEvent* event)
{
    object=object;

    switch(event->type()) {
        case QEvent::KeyPress:
            printf("Key press event\n");
            break;
        case QEvent::MouseButtonPress:
            printf("MouseButtonPress Event\n");
            break;
        case QEvent::MouseButtonRelease:
            printf("MouseButtonRelease Event\n");
            break;
        case QEvent::MouseButtonDblClick:
            printf("MouseButtonDblClick Event\n");
            break;
        default:
            printf("An other event was detected %d\n",event->type());
            break;
    }
    return false;
}

/* ****
// Signals to handle a progress bar
//
void WebPage::started(KIO::Job *job)
{
//    if(progressBar==NULL) { // No progressBar is currently displayed
    if(job) { // Do we have a job object???
        if(DEBUG)
        printf("Download started\n");
        connect( job, SIGNAL(percent(KIO::Job*, unsigned long)),
             this, SLOT(percent(KIO::Job*, unsigned long)));
//        progressBar = new MythProgressDialog("Loading... ",100);
    } else { // nope
        if(DEBUG)
        printf("Download started without a job object\n");
//        progressBar = new MythProgressDialog("Loading... ",0);
    }
//    }
}

void WebPage::completed()
{
    if(DEBUG)
    printf("Download completed\n");
//    if(progressBar!=NULL) {
//    progressBar->Close();
//    progressBar=NULL;
//    }
}

void WebPage::percent(KIO::Job *job, unsigned long percent)
{
    job=job;
    if(DEBUG)
    printf(" %lu\n",percent);
}

**** */
