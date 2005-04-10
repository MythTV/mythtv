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
#include <qhbox.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qtimer.h>

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

WebPage::WebPage (const char *location, int zoom, WFlags flags) : QWidget(NULL,NULL,flags)
{
    setPaletteBackgroundColor(Qt::white);
        
    QHBoxLayout *hbox = new QHBoxLayout(this);
    browser = new KHTMLPart(this);
    hbox->addWidget(browser->view());
    
    // connect signal for link clicks
    connect( browser->browserExtension(),
         SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ), 
         this, SLOT( openURLRequest(const KURL &, const KParts::URLArgs & ) ) );

    connect( browser->browserExtension(),
         SIGNAL( createNewWindow( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ), 
         this, SLOT( createNewWindow( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ) );

    // connect signals for download progress
    connect( browser,SIGNAL(started(KIO::Job*)),
         this,SLOT(started(KIO::Job*)));
    connect( browser,SIGNAL(completed()),
         this,SLOT(completed()));

    browser->setStandardFont("Arial");
    
    zoom = zoom<20 ? 20:zoom;
    zoom = zoom>300 ? 300:zoom;
    browser->setZoomFactor(zoom);
    zoomFactor=zoom;

    browser->setPluginsEnabled(false);
    browser->enableMetaRefresh(true);
    browser->setJavaEnabled(true);
    browser->enableJScript(true);
    
    openURL(QString(location));
    
    if(DEBUG)
        qApp->installEventFilter(this);
}

WebPage::WebPage (const char *location, const KParts::URLArgs &args, int zoom, WFlags flags)
                  : QWidget(NULL,NULL,flags)
{
    setPaletteBackgroundColor(Qt::white);

    QHBoxLayout *hbox = new QHBoxLayout(this);
    browser = new KHTMLPart(this);
    hbox->addWidget(browser->view());

    // connect signal for link clicks
    connect( browser->browserExtension(),
         SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ), 
         this, SLOT( openURLRequest(const KURL &, const KParts::URLArgs & ) ) );

    connect( browser->browserExtension(),
         SIGNAL( createNewWindow( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ), 
         this, SLOT( createNewWindow( const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&) ) );

    // connect signals for download progress
    connect( browser,SIGNAL(started(KIO::Job*)),
         this,SLOT(started(KIO::Job*)));
    connect( browser,SIGNAL(completed()),
         this,SLOT(completed()));

    browser->setStandardFont("Arial");
    browser->browserExtension()->setURLArgs(args);

    zoom = zoom<20 ? 20:zoom;
    zoom = zoom>300 ? 300:zoom;
    browser->setZoomFactor(zoom);
    zoomFactor=zoom;

    browser->setPluginsEnabled(false);
    browser->enableMetaRefresh(true);
    browser->setJavaEnabled(true);
    browser->enableJScript(true);
    
    openURL(QString(location));

    if(DEBUG)
        qApp->installEventFilter(this);
}

WebPage::~WebPage()
{
    if (DEBUG)
        printf("deleting web page: %s\n", sUrl.ascii());
}

void WebPage::openURL(const QString &url)
{
    sUrl = url;
    if (url != "")
        browser->openURL(url);
}

void WebPage::openURLRequest(const KURL &url, const KParts::URLArgs &args)
{
    emit newUrlRequested(url,args);
}

void WebPage::createNewWindow( const KURL &url, const KParts::URLArgs &args, 
                               const KParts::WindowArgs &windowArgs, 
                               KParts::ReadOnlyPart *&part)
{
    emit newWindowRequested(url, args, windowArgs, part);
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

void WebPage::started(KIO::Job *job)
{
    job = job;
    setCursor(QCursor(Qt::BusyCursor));
}

void WebPage::completed()
{
    setCursor(QCursor(Qt::ArrowCursor));
    
    // work around a bug in the HTML widget where it doesn't scroll
    // to the correct location if the URL has a reference
    KURL url(sUrl);
    if (url.hasRef()) {    
        
        QTimer::singleShot(1, this, SLOT(timerTimeout()));
    }
    
    setFocus();
}    

void WebPage::timerTimeout()
{
    // jump to reference if present in url
    KURL url(sUrl);
    if (url.hasRef()) {    
        if (!browser->gotoAnchor(url.encodedHtmlRef()))
            browser->gotoAnchor(url.htmlRef());
    }
}
