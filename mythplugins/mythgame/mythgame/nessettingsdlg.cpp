#include "nessettingsdlg.h"
#include <iostream>

#include <qapplication.h>
#include <qvariant.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qframe.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qtabwidget.h>
#include <qwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>

#include "neshandler.h"

#include <mythtv/mythcontext.h>

using namespace std;

#define SAVE_SETTINGS 1
#define DONT_SAVE_SETTINGS 0

NesSettingsDlg::NesSettingsDlg(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    setCursor(QCursor(Qt::BlankCursor));
    if ( !name )
      setName( "NesSettings" );

    resize( screenwidth, screenheight );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( screenwidth, screenheight ) );
    setMaximumSize( QSize( screenwidth, screenheight ) );
    setCaption( trUtf8( "Nes Settings" ) );

    NesTab = new QTabWidget( this, "NesTab" );
    NesTab->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
    NesTab->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, NesTab->sizePolicy().hasHeightForWidth() ) );
    NesTab->setBackgroundOrigin( QTabWidget::WindowOrigin );
    QFont NesTab_font(  NesTab->font() );
    NesTab_font.setFamily( "Helvetica [Urw]" );
    NesTab_font.setPointSize( (int)(gContext->GetMediumFontSize() * wmult));
    NesTab_font.setBold( TRUE );
    NesTab->setFont( NesTab_font );

    SettingsTab = new QWidget( NesTab, "SettingsTab" );
    NesTab->insertTab( SettingsTab, trUtf8( "Settings" ) );

    ListButton = new QPushButton( SettingsTab, "ListButton" );
    ListButton->setGeometry( QRect( (int)(28 * wmult), (int)(513 * hmult), 
                                    (int)(250 * wmult), (int)(40 * hmult)));
    ListButton->setText( trUtf8( "Reload Game List" ) );
    connect(ListButton, SIGNAL(pressed()), this,
            SLOT(loadList()));
}

/*  
 *  Destroys the object and frees any allocated resources
 */
NesSettingsDlg::~NesSettingsDlg()
{
    // no need to delete child widgets, Qt does it all for us
}

void NesSettingsDlg::loadList()
{
    NesHandler::getHandler()->processGames();
}
