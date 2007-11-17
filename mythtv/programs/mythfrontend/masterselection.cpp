//////////////////////////////////////////////////////////////////////////////
// Program Name: masterselection.cpp
//
// Purpose - Classes to Prompt user for MasterBackend
//
// Created By  : David Blain                    Created On : Jan. 25, 2007
// Modified By : Stuart Morgan                  Modified On: 4th July, 2007
//
//////////////////////////////////////////////////////////////////////////////

#include "masterselection.h"
#include "upnp.h"
#include "mythxmlclient.h"

#include <qapplication.h>
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qfont.h>
#include <qdir.h>
#include <qstring.h>

#include <pthread.h>

#ifdef USE_LIRC
#include "lirc.h"
#include "lircevent.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// return values:   -1 = Exit Application
//                   0 = Continue with no Connection Infomation
//                   1 = Connection Information found
/////////////////////////////////////////////////////////////////////////////

int MasterSelection::GetConnectionInfo( MediaRenderer  *pUPnp,
                                        DatabaseParams *pParams,
                                        bool            bPromptForBackend )
{
    if ((pUPnp == NULL) || (pParams == NULL))
    {
        VERBOSE( VB_UPNP, "MasterSelection::GetConnectionInfo - "
                          "Invalid NULL parameters." );
        return -1;
    }

    // ----------------------------------------------------------------------
    // Try to get the last selected (default) Master
    // ----------------------------------------------------------------------

    int                    nRetCode         = -1;
    bool                   bExitLoop        = false;
    bool                   bFirstTime       = true;
    bool                   bTryConnectAgain = false;
    QString                sPin             = "";
    MasterSelectionWindow *pMasterWindow    = NULL;
    DeviceLocation        *pDeviceLoc       = NULL;

    if (!bPromptForBackend)
        pDeviceLoc = pUPnp->GetDefaultMaster();

    if (pDeviceLoc != NULL)
        sPin = pDeviceLoc->m_sSecurityPin;

    // ----------------------------------------------------------------------
    // Loop until we have a valid DatabaseParams, or the user cancels
    // ----------------------------------------------------------------------

    while (!bExitLoop)
    {
        bTryConnectAgain = false;

        if (pDeviceLoc == NULL)
        {
            bFirstTime = true;

            if ( pMasterWindow == NULL)
            {
                pMasterWindow = new MasterSelectionWindow();
                pMasterWindow->show();
            }

            MasterSelectionDialog selectionDlg( pMasterWindow );

            // --------------------------------------------------------------
            // Show dialog and check for Cancel.
            // --------------------------------------------------------------

            if ( !selectionDlg.exec() )
            {
                nRetCode  = -1;
                bExitLoop = true;
            }
            else
            {
                // ----------------------------------------------------------
                // Did user select "No Backend"
                // ----------------------------------------------------------

                sPin = "";

                if (( pDeviceLoc = selectionDlg.GetSelectedDevice()) == NULL)
                {
                    nRetCode  = 0;
                    bExitLoop = true;
                }
            }
        }

        // ------------------------------------------------------------------
        // If DeviceLoc != NULL... Try and retrieve ConnectionInformation
        // ------------------------------------------------------------------

        if (pDeviceLoc != NULL)
        {
            QString       sMsg;
            MythXMLClient mythXml( pDeviceLoc->m_sLocation );

            UPnPResultCode eCode = mythXml.GetConnectionInfo( sPin, pParams, sMsg );

            switch( eCode )
            {
                case UPnPResult_Success:
                {
                    VERBOSE(VB_GENERAL, QString("Database Hostname: %1")
                                        .arg(pParams->dbHostName));

                    pUPnp->SetDefaultMaster( pDeviceLoc, sPin );

                    bExitLoop = true;
                    nRetCode  = 1;

                    break;
                }

                case UPnPResult_ActionNotAuthorized:
                {
                    if ( pMasterWindow == NULL)
                    {
                        pMasterWindow = new MasterSelectionWindow();
                        pMasterWindow->show();
                    }

                    // Don't show Access Denied Error to user since this
                    // is the first time they will see the pin dialog.

                    if (bFirstTime)
                    {
                        sMsg = "";
                        bFirstTime = false;
                    }

                    // Prompt For Pin and try again...

                    VERBOSE( VB_IMPORTANT, QString( "Access Denied for %1" )
                               .arg( pDeviceLoc->GetFriendlyName( true ) ));

                    PinDialog *passwordDlg = new PinDialog(pMasterWindow,
                                         pDeviceLoc->GetFriendlyName( true ), sMsg);

                    if ( passwordDlg->exec() == QDialog::Accepted )
                    {
                        sPin = passwordDlg->GetPin();
                        bTryConnectAgain = true;
                    }

                    delete passwordDlg;

                    break;
                }

                default:
                {
                    if ( pMasterWindow == NULL)
                    {
                        pMasterWindow = new MasterSelectionWindow();
                        pMasterWindow->show();
                    }

                    // Display Error Msg and have user select different Master.

                    VERBOSE( VB_IMPORTANT, QString( "Error Retrieving "
                                    "ConnectionInfomation for %1 (%2) %3" )
                                    .arg( pDeviceLoc->GetFriendlyName( true ) )
                                    .arg( eCode )
                                    .arg( UPnp::GetResultDesc( eCode )));


                    QMessageBox msgBox( "Error",
                                        QString( "(%1) - %2 : %3" )
                                           .arg( eCode )
                                           .arg( UPnp::GetResultDesc( eCode ))
                                           .arg( sMsg ),
                                        QMessageBox::Critical,
                                        QMessageBox::Ok,
                                        QMessageBox::NoButton,
                                        QMessageBox::NoButton,
                                        pMasterWindow,
                                        NULL,
                                        TRUE,
                                        Qt::WStyle_Customize | Qt::WStyle_NoBorder );

                    msgBox.exec();
                    break;
                }
            }

            if ( !bTryConnectAgain )
            {
                pDeviceLoc->Release();
                pDeviceLoc = NULL;
            }
        }
    }

    if ( pMasterWindow != NULL)
    {
        pMasterWindow->hide();
        delete pMasterWindow;
    }

    return nRetCode;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MasterSelectionWindow::MasterSelectionWindow()
    : QWidget( NULL, "BackgroundWindow" )
{
    QDesktopWidget *pDesktop = QApplication::desktop();

    int nScreenHeight = pDesktop->height();

    float fhMulti = nScreenHeight / (float)600;

    QFont font = QFont("Arial");

    if (!font.exactMatch())
        font = QFont();

    font.setStyleHint( QFont::SansSerif, QFont::PreferAntialias );
    font.setPointSize((int)floor(14 * fhMulti ));

    QColorGroup colors( QColor( 255, 255, 255 ),    // Foreground
                        QColor(   0,  64, 128 ),    // background
                        QColor( 196, 196, 196 ),    // light
                        QColor(  64,  64,  64 ),    // dark
                        QColor( 128, 128, 128 ),    // mid
                        QColor( 255, 255, 255 ),    // text
                        QColor(   0, 100, 192 ));   // Base

    QApplication::setFont   ( font, TRUE );
    QApplication::setPalette( QPalette( colors, colors, colors ), TRUE );

    showFullScreen();

}

/*
#ifdef USE_LIRC

    pthread_t lirc_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&lirc_tid, &attr, SpawnLirc, this);
    pthread_attr_destroy(&attr);

#endif

#ifdef USE_JOYSTICK_MENU
    pthread_t js_tid;

    pthread_attr_t attr2;
    pthread_attr_init(&attr2);
    pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_DETACHED);
    pthread_create(&js_tid, &attr2, SpawnJoystickMenu, this);
    pthread_attr_destroy(&attr2);
#endif

#ifdef USING_APPLEREMOTE
    pthread_t appleremote_tid;

    pthread_attr_t attr3;
    pthread_attr_init(&attr3);
    pthread_attr_setdetachstate(&attr3, PTHREAD_CREATE_DETACHED);
    pthread_create(&appleremote_tid, &attr3, SpawnAppleRemote, this);
    pthread_attr_destroy(&attr3);
#endif
}

void MasterSelectionWindow::customEvent(QCustomEvent *e)
{


#ifdef USE_LIRC

    if (e->type() == kLircKeycodeEventType)
    {
        LircKeycodeEvent *lke = (LircKeycodeEvent *)e;
        int keycode = lke->getKeycode();

        if (keycode)
        {
            switch (keycode)
            {
                case Qt::Key_Left :
                {
                    keycode = Qt::Key_BackTab;
                    break;
                }
                case Qt::Key_Right :
                {
                    keycode = Qt::Key_Tab;
                    break;
                }
                default:
                    break;
            }

            int mod = keycode & MODIFIER_MASK;
            int k = keycode & ~MODIFIER_MASK;  // trim off the mod 
            int ascii = 0;
            QString text;

            if (k & UNICODE_ACCEL)
            {
                QChar c(k & ~UNICODE_ACCEL);
                ascii = c.latin1();
                text = QString(c);
            }

            mod = ((mod & Qt::CTRL) ? Qt::ControlButton : 0) |
                ((mod & Qt::META) ? Qt::MetaButton : 0) |
                ((mod & Qt::ALT) ? Qt::AltButton : 0) |
                ((mod & Qt::SHIFT) ? Qt::ShiftButton : 0);

            QKeyEvent key(lke->isKeyDown() ? QEvent::KeyPress :
                        QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget();
            if (!key_target)
                QApplication::sendEvent(this, &key);
            else
                QApplication::sendEvent(key_target, &key);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("LircClient warning: attempt to "
                                  "convert '%1' to a key sequence failed. Fix "
                                  "your key mappings.")
                                  .arg(lke->getLircText()));
        }
    }

#endif
}

*/

QObject *MasterSelectionWindow::getTarget()
{
    QObject *key_target = NULL;

    key_target = QWidget::keyboardGrabber();

    if (!key_target)
    {
        QWidget *focus_widget = qApp->focusWidget();
        if (focus_widget && focus_widget->isEnabled())
        {
            key_target = focus_widget;
        }
    }

    if (!key_target)
        key_target = this;

    return key_target;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

PinDialog::PinDialog( QWidget *pParent, QString name, QString sMsg )
        : QDialog( pParent, "Pin Entry", TRUE, WType_Popup )
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_pPassword = new QLineEdit( this );
    m_pPassword->setEchoMode(QLineEdit::Password);
    m_pPassword->setFocus();

    m_pCancel  = new QPushButton( tr("Cancel"), this );
    m_pOk      = new QPushButton( tr("OK")    , this );

    m_pMessage = new QLabel( sMsg, this, NULL, Qt::WordBreak);

    m_pLayout  = new QGridLayout( this, 11, 8, 5 );

    m_pLayout->addMultiCellWidget( new QLabel( name, this ), 0, 0, 1, 4, Qt::AlignLeft );
    m_pLayout->addMultiCellWidget( new QLabel( tr( "Security Pin:" ), this ),
                                   3, 3, 1, 4, Qt::AlignLeft );
    m_pLayout->addMultiCellWidget(m_pPassword, 5, 5, 1, 3);

    m_pLayout->addMultiCellWidget( m_pMessage, 7, 8, 1, 4, Qt::AlignCenter );

    m_pLayout->addWidget(m_pCancel, 10, 1);
    m_pLayout->addWidget(m_pOk, 10, 3);

    connect( m_pPassword, SIGNAL( returnPressed() ), SLOT( accept ()) );
    connect( m_pPassword, SIGNAL( textChanged  ( const QString& ) ), SLOT( changed( const QString& )) );

    connect( m_pOk    , SIGNAL( clicked() ), SLOT( accept()) );
    connect( m_pCancel, SIGNAL( clicked() ), SLOT( reject()) );
}

PinDialog::~PinDialog()
{

}

QString PinDialog::GetPin()
{
    QString pin = m_pPassword->text();

    return pin;

}

void PinDialog::changed( const QString & )
{
    static bool bMsgCleared = false;

    if (!bMsgCleared)
    {
        m_pMessage->setText( "" );
        bMsgCleared = true;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MasterSelectionDialog::MasterSelectionDialog( MasterSelectionWindow *pParent )
       : QDialog( pParent, "Master Mediaserver Selection", TRUE,
                  WStyle_Customize | WStyle_NoBorder )
{
    setFocusPolicy(QWidget::NoFocus);

    m_pCancel  = new QPushButton( tr("Cancel"), this );
    m_pSearch  = new QPushButton( tr("Search"), this );

    m_pListBox = new QListBox( this );
    m_pListBox->setFocus();
    m_pListBox->setFrameShape( QFrame::WinPanel );
    m_pListBox->setSelectionMode( QListBox::Single );

    m_pOk      = new QPushButton( tr("OK")    , this );

    m_pLayout  = new QGridLayout( this, 7, 5, 6 );

    m_pLayout->setMargin( 50 );

    m_pLayout->addWidget( m_pSearch, 5, 0 );
    m_pLayout->addWidget( m_pCancel, 5, 3 );
    m_pLayout->addWidget( m_pOk    , 5, 4 );

    QString labeltext = tr( "Select Default Myth Backend Server:" );

    m_pLayout->addMultiCellWidget( new QLabel( labeltext, this ), 1, 1, 0, 4 );

    m_pLayout->addMultiCellWidget( m_pListBox, 3, 3, 0, 4 );

    connect( m_pOk    , SIGNAL( clicked() ), SLOT( accept()) );
    connect( m_pCancel, SIGNAL( clicked() ), SLOT( reject()) );
    connect( m_pSearch, SIGNAL( clicked() ), SLOT( Search()) );
    connect( m_pListBox, SIGNAL( returnPressed(QListBoxItem *) ),
            SLOT( accept()) );

    showFullScreen();

    UPnp::AddListener( this );

    QListBoxItem *pItem = AddItem( NULL, tr("Do Not Connect To MythBackend") );

    m_pListBox->setCurrentItem( pItem );

    Search();
    FillListBox();
}

MasterSelectionDialog::~MasterSelectionDialog()
{
    UPnp::RemoveListener( this );

    for (ItemMap::iterator it  = m_Items.begin();
                           it != m_Items.end();
                         ++it )
    {
        ListBoxDevice *pItem = it.data();

        if (pItem != NULL)
            delete pItem;
    }

    m_Items.clear();

}

/////////////////////////////////////////////////////////////////////////////
//  Caller MUST call Release on returned pointer
/////////////////////////////////////////////////////////////////////////////

DeviceLocation *MasterSelectionDialog::GetSelectedDevice( void )
{
    DeviceLocation *pLoc = NULL;
    ListBoxDevice  *pItem = (ListBoxDevice *)m_pListBox->selectedItem();

    if (pItem != NULL)
    {
        if ((pLoc = pItem->m_pLocation) != NULL)
            pLoc->AddRef();
    }

    return pLoc;
}

ListBoxDevice *MasterSelectionDialog::AddItem( DeviceLocation *pLoc,
                                                QString sName )
{
    ListBoxDevice *pItem = NULL;
    QString        sUSN  = "None";

    if (pLoc != NULL)
        sUSN  = pLoc->m_sUSN;

    ItemMap::iterator it = m_Items.find( sUSN );

    if ( it == m_Items.end())
    {
        pItem = new ListBoxDevice( m_pListBox, sName, pLoc );
        m_Items.insert( sUSN, pItem );
    }
    else
        pItem = it.data();

    return pItem;
}

void MasterSelectionDialog::RemoveItem( QString sUSN )
{
    ItemMap::iterator it = m_Items.find( sUSN );

    if ( it != m_Items.end() )
    {
        ListBoxDevice *pItem = it.data();

        if (pItem != NULL)
            delete pItem;

        m_Items.remove( it );
    }
}

void MasterSelectionDialog::Search( void )
{
    UPnp::PerformSearch( "urn:schemas-mythtv-org:device:MasterMediaServer:1" );
}

void MasterSelectionDialog::FillListBox( void )
{

    EntryMap ourMap;

    SSDPCacheEntries *pEntries = UPnp::g_SSDPCache.Find(
                        "urn:schemas-mythtv-org:device:MasterMediaServer:1" );

    if (pEntries != NULL)
    {
        pEntries->AddRef();
        pEntries->Lock();

        EntryMap *pMap = pEntries->GetEntryMap();


        for (EntryMap::Iterator it  = pMap->begin();
                                it != pMap->end();
                              ++it )
        {
            DeviceLocation *pEntry = (DeviceLocation *)it.data();

            if (pEntry != NULL)
            {
                pEntry->AddRef();
                ourMap.insert( pEntry->m_sUSN, pEntry );
            }
        }

        pEntries->Unlock();
        pEntries->Release();
    }

    for (EntryMap::Iterator it  = ourMap.begin();
                            it != ourMap.end();
                          ++it )
    {
        DeviceLocation *pEntry = (DeviceLocation *)it.data();

        if (pEntry != NULL)
        {
            if ( print_verbose_messages & VB_UPNP )
                AddItem( pEntry, pEntry->GetNameAndDetails( true ));
            else
                AddItem( pEntry, pEntry->GetFriendlyName( true ));

            pEntry->Release();
        }
    }
}

void MasterSelectionDialog::customEvent(QCustomEvent *e)
{

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        VERBOSE(VB_UPNP, QString("MasterSelectionDialog::customEvent - %1 : "
                                 "%2 - %3").arg(message)
                                 .arg(me->ExtraData( 0 ))
                                 .arg(me->ExtraData( 1 )));

        if (message.startsWith( "SSDP_ADD" ))
        {
            QString sURI = me->ExtraData( 0 );
            QString sURN = me->ExtraData( 1 );
            QString sURL = me->ExtraData( 2 );

            if ( sURI.startsWith( "urn:schemas-mythtv-org:device:MasterMediaServer:" ))
            {
                DeviceLocation *pLoc    = UPnp::g_SSDPCache.Find( sURI, sURN );

                if (pLoc != NULL)
                {
                    pLoc->AddRef();

                    if ( print_verbose_messages & VB_UPNP )
                        AddItem( pLoc, pLoc->GetNameAndDetails( true ));
                    else
                        AddItem( pLoc, pLoc->GetFriendlyName( true ));

                    pLoc->Release();
                }
            }
        }
        else if (message.startsWith( "SSDP_REMOVE" ))
        {
            //-=>Note: This code will never get executed until
            //         SSDPCache is changed to handle NotifyRemove correctly

            QString sURI = me->ExtraData( 0 );
            QString sURN = me->ExtraData( 1 );

            RemoveItem( sURN );
        }
    }
}
