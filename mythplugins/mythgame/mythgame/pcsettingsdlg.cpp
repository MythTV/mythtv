#include "pcsettingsdlg.h"

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

#include "pchandler.h"

#include <mythtv/mythcontext.h>

using namespace std;

PCSettingsDlg::PCSettingsDlg(MythMainWindow* parent,
                             const char* name, bool system)
    : MythDialog(parent, name), bSystem(system)
{
    setCursor(QCursor(Qt::BlankCursor));
    if ( !name )
      setName( "SnesSettings" );

    int screenwidth = 0, screenheight = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    resize( screenwidth, screenheight );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( screenwidth, screenheight ) );
    setMaximumSize( QSize( screenwidth, screenheight ) );
    setCaption( trUtf8( "PC Settings" ) );

    PCTab = new QTabWidget( this, "PCTab" );
    PCTab->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
    PCTab->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, PCTab->sizePolicy().hasHeightForWidth() ) );
    PCTab->setBackgroundOrigin( QTabWidget::WindowOrigin );
    QFont PCTab_font(  PCTab->font() );
    PCTab_font.setFamily( "Helvetica [Urw]" );
    PCTab_font.setPointSize( (int)(gContext->GetMediumFontSize() * wmult));
    PCTab_font.setBold( TRUE );
    PCTab->setFont( PCTab_font );

    SettingsTab = new QWidget( PCTab, "SettingsTab" );
    PCTab->insertTab( SettingsTab, trUtf8( "Settings" ) );

    SettingsFrame = new QFrame( SettingsTab, "SettingsFrame" );
    SettingsFrame->setGeometry( QRect(0, (int)(33 * hmult),
                                      (int)(800 * wmult), (int)(480 * hmult)));
    SettingsFrame->setFrameShape( QFrame::StyledPanel );
    SettingsFrame->setFrameShadow( QFrame::Raised );

    if(!bSystem)
    {
        ScreenPic = new QLabel( SettingsFrame, "ScreenPic" );
        ScreenPic->setEnabled( TRUE );
        ScreenPic->setGeometry( QRect( 200, 100, screenwidth - 300, screenheight - 200 ) );
        ScreenPic->setScaledContents( TRUE );

        NameLabel = new QLabel( SettingsFrame, "NameLabel" );
        NameLabel->setGeometry( QRect( (int)(20 * wmult), (int)(330 * hmult),
                                   (int)(161 * wmult), (int)(31  * hmult)) );
        NameLabel->setText( trUtf8( "Game Name: " ) );
        NameValLabel = new QLabel( SettingsFrame, "NameValLabel" );
        NameValLabel->setGeometry( QRect( (int)(181 * wmult), (int)(330 * hmult),
                                   (int)(400 * wmult), (int)(31  * hmult)) );

        CommandLabel = new QLabel( SettingsFrame, "CommandLabel" );
        CommandLabel->setGeometry( QRect( (int)(20 * wmult), (int)(365 * hmult),
                                   (int)(161 * wmult), (int)(31  * hmult)) );
        CommandLabel->setText( trUtf8( "Command: " ) );
        CommandValLabel = new QLabel( SettingsFrame, "CommandValLabel" );
        CommandValLabel->setGeometry( QRect( (int)(181 * wmult), (int)(365 * hmult),
                                   (int)(400 * wmult), (int)(31  * hmult)) );

        GenreLabel = new QLabel( SettingsFrame, "GenreLabel" );
        GenreLabel->setGeometry( QRect( (int)(20 * wmult), (int)(400 * hmult),
                                   (int)(161 * wmult), (int)(31  * hmult)) );
        GenreLabel->setText( trUtf8( "Genre: " ) );
        GenreValLabel = new QLabel( SettingsFrame, "GenreValLabel" );
        GenreValLabel->setGeometry( QRect( (int)(181 * wmult), (int)(400 * hmult),
                                   (int)(400 * wmult), (int)(31  * hmult)) );

        YearLabel = new QLabel( SettingsFrame, "YearLabel" );
        YearLabel->setGeometry( QRect( (int)(20 * wmult), (int)(435 * hmult),
                                   (int)(161 * wmult), (int)(31  * hmult)) );
        YearLabel->setText( trUtf8( "Year: " ) );
        YearValLabel = new QLabel( SettingsFrame, "YearValLabel" );
        YearValLabel->setGeometry( QRect( (int)(181 * wmult), (int)(435 * hmult),
                                   (int)(400 * wmult), (int)(31  * hmult)) );
    }

    
    else
    {

        ListButton = new QPushButton( SettingsFrame, "ListButton" );
        ListButton->setGeometry( QRect( (int)(100 * wmult), (int)(250 * hmult),
                                    (int)(250 * wmult), (int)(40 * hmult)));
        ListButton->setText( trUtf8( "Reload Game List" ) );
        connect(ListButton, SIGNAL(pressed()), this,
            SLOT(loadList()));
    }
}

/*
 *  Destroys the object and frees any allocated resources
 */
PCSettingsDlg::~PCSettingsDlg()
{
    // no need to delete child widgets, Qt does it all for us
}

void PCSettingsDlg::Show(PCRomInfo* rominfo)
{
    if(rominfo && !bSystem)
    {
        NameValLabel->setText( trUtf8(rominfo->Gamename()) );
        CommandValLabel->setText( trUtf8(rominfo->Romname()) );
        GenreValLabel->setText( trUtf8(rominfo->Genre()) );
        YearValLabel->setNum(int(rominfo->Year()));
    }
    show();
    exec();
}

void PCSettingsDlg::loadList()
{
    PCHandler::getHandler()->processGames();
}

void PCSettingsDlg::SetScreenshot(QString picfile)
{
    if(ScreenPic)
    {
        Screenshot.load(picfile);
        ScaleImageLabel(Screenshot,ScreenPic);
        ScreenPic->setPixmap(Screenshot);
    }
}

void PCSettingsDlg::ScaleImageLabel(QPixmap &pixmap, QLabel *label)
{
    int FrameWidth = width() / 2;
    int FrameHeight = (int)(height() - 33 * hmult) / 2;
    int ImageWidth = pixmap.width();
    int ImageHeight = pixmap.height();
    int x = 0, y = 0;
    if (float(ImageHeight)/float(ImageWidth) < float(FrameHeight)/float(FrameWidth))
    {
        int temp = ImageWidth;
        ImageWidth = FrameWidth;
        ImageHeight = (ImageWidth * ImageHeight) / temp;
        y = (FrameHeight - ImageHeight) / 2;
    }
    else
    {
        int temp = ImageHeight;
        ImageHeight = FrameHeight;
        ImageWidth = (ImageHeight * ImageWidth) / temp;
        x = (FrameWidth - ImageWidth) / 2;
    }
    label->setGeometry( QRect( x + 200, y + 25, ImageWidth, ImageHeight ) );
} 
 
