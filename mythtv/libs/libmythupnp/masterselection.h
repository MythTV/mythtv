//////////////////////////////////////////////////////////////////////////////
// Program Name: masterselection.h
//
// Purpose - Classes to Prompt user for MasterBackend
//
// Created By  : David Blain                    Created On : Jan. 25, 2007
// Modified By : Stuart Morgan                  Modified On: 4th July, 2007
//
//////////////////////////////////////////////////////////////////////////////

#include <qdialog.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qlistbox.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qmap.h>
#include <math.h>

#include "upnpdevice.h"
#include "mediarenderer.h"

#ifndef __MASTERSELECTION_H__
#define __MASTERSELECTION_H__

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class MasterSelection
{
    public:

        static int GetConnectionInfo( MediaRenderer  *pUPnp,
                                      DatabaseParams *pParams, 
                                      bool            bPromptForBackend );
};

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class ListBoxDevice : public QListBoxText
{
    public:

        DeviceLocation *m_pLocation;

        ListBoxDevice( QListBox *pList, const QString &sName,
                                        DeviceLocation *pLoc )
            : QListBoxText( pList, sName )
        {
            if ((m_pLocation = pLoc) != NULL)
                m_pLocation->AddRef();
        }

        virtual ~ListBoxDevice()
        {
            if ( m_pLocation != NULL)
                m_pLocation->Release();
        }
};

typedef QMap< QString, ListBoxDevice *>  ItemMap;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class MasterSelectionWindow : public QWidget
{
    public:

        MasterSelectionWindow();

    protected:

        //void customEvent(QCustomEvent *e);
        QObject *getTarget();

};

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class MasterSelectionDialog : public QDialog 
{
    Q_OBJECT

    protected:

        ItemMap          m_Items;

        QGridLayout     *m_pLayout;
        QListBox        *m_pListBox;

        QPushButton     *m_pOk;
        QPushButton     *m_pCancel;
        QPushButton     *m_pSearch;

        ListBoxDevice *AddItem   ( DeviceLocation *pLoc, QString sName );
        void           RemoveItem( QString sURN );

    public slots:

        void Search     ( void );
        void FillListBox( void );

    public:

                 MasterSelectionDialog( MasterSelectionWindow *pParent );
        virtual ~MasterSelectionDialog();

        void     customEvent(QCustomEvent *e);

        DeviceLocation *GetSelectedDevice( void );


};

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class PinDialog : public QDialog
{
    Q_OBJECT

    public:

                 PinDialog( QWidget *pParent, QString name, QString sMsg );
        virtual ~PinDialog();

        QString  GetPin();

    protected:

        QGridLayout     *m_pLayout;
        QListBox        *m_pListBox;

        QPushButton     *m_pOk;
        QPushButton     *m_pCancel;

        QLineEdit       *m_pPassword;
        QLabel          *m_pMessage;

    signals:

        void textChanged( const QString & );

    protected slots:

        void changed( const QString & );

};

#endif
