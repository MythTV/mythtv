#include "mamesettingsdlg.h"
#include <iostream.h>

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
#include <qmsgbox.h>

#include "mamehandler.h"

#include <mythtv/settings.h>

extern Settings *globalsettings;

#define SAVE_SETTINGS 1
#define DONT_SAVE_SETTINGS 0

MameSettingsDlg::MameSettingsDlg( QWidget* parent,  const char* name, bool modal, bool system, WFlags fl )
    : QDialog( parent, name, modal, fl ), bSystem(system)
{
    setCursor(QCursor(Qt::BlankCursor));
    if ( !name )
      setName( "MameSettings" );

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    resize( screenwidth, screenheight );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( screenwidth, screenheight ) );
    setMaximumSize( QSize( screenwidth, screenheight ) );
    //setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    setCaption( trUtf8( "XMame Settings" ) );

    MameTab = new QTabWidget( this, "MameTab" );
    MameTab->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
    MameTab->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, MameTab->sizePolicy().hasHeightForWidth() ) );
    //MameTab->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    MameTab->setBackgroundOrigin( QTabWidget::WindowOrigin );
    QFont MameTab_font(  MameTab->font() );
    MameTab_font.setFamily( "Helvetica [Urw]" );
    MameTab_font.setPointSize( 16 );
    MameTab_font.setBold( TRUE );
    MameTab->setFont( MameTab_font );

    SettingsTab = new QWidget( MameTab, "SettingsTab" );

    SettingsFrame = new QFrame( SettingsTab, "SettingsFrame" );
    SettingsFrame->setGeometry( QRect( 0, 33, 800, 480 ) );
    //SettingsFrame->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    SettingsFrame->setFrameShape( QFrame::StyledPanel );
    SettingsFrame->setFrameShadow( QFrame::Raised );

    DisplayGroup = new QGroupBox( SettingsFrame, "DisplayGroup" );
    DisplayGroup->setGeometry( QRect( 0, 10, 420, 480 ) );
    //DisplayGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    DisplayGroup->setTitle( trUtf8( "Display" ) );

    FullCheck = new QCheckBox( DisplayGroup, "FullCheck" );
    FullCheck->setGeometry( QRect( 11, 31, 160, 31 ) ); 
    FullCheck->setText( trUtf8( "Full screen" ) );

    SkipCheck = new QCheckBox( DisplayGroup, "SkipCheck" );
    SkipCheck->setGeometry( QRect( 195, 31, 214, 31 ) ); 
    SkipCheck->setText( trUtf8( "Auto frame skip" ) );

    LeftCheck = new QCheckBox( DisplayGroup, "LeftCheck" );
    LeftCheck->setGeometry( QRect( 11, 68, 170, 31 ) ); 
    LeftCheck->setText( trUtf8( "Rotate Left" ) );

    RightCheck = new QCheckBox( DisplayGroup, "RightCheck" );
    RightCheck->setGeometry( QRect( 195, 68, 190, 31 ) ); 
    RightCheck->setText( trUtf8( "Rotate Right" ) );

    FlipXCheck = new QCheckBox( DisplayGroup, "FlipXCheck" );
    FlipXCheck->setGeometry( QRect( 11, 105, 178, 31 ) ); 
    FlipXCheck->setText( trUtf8( "Flip X Axis" ) );

    FlipYCheck = new QCheckBox( DisplayGroup, "FlipYCheck" );
    FlipYCheck->setGeometry( QRect( 195, 105, 180, 31 ) ); 
    FlipYCheck->setText( trUtf8( "Flip Y Axis" ) );

    ExtraArtCheck = new QCheckBox( DisplayGroup, "ExtraArtCheck" );
    ExtraArtCheck->setGeometry( QRect( 195, 142, 200, 31 ) ); 
    ExtraArtCheck->setText( trUtf8( "Extra Artwork" ) );

    ScanCheck = new QCheckBox( DisplayGroup, "ScanCheck" );
    ScanCheck->setGeometry( QRect( 10, 140, 178, 31 ) ); 
    ScanCheck->setText( trUtf8( "Scanlines" ) );

    ColorCheck = new QCheckBox( DisplayGroup, "ColorCheck" );
    ColorCheck->setGeometry( QRect( 11, 179, 398, 31 ) ); 
    ColorCheck->setText( trUtf8( "Auto color depth" ) );

    ScaleLabel = new QLabel( DisplayGroup, "ScaleLabel" );
    ScaleLabel->setGeometry( QRect( 20, 220, 71, 31 ) ); 
    ScaleLabel->setText( trUtf8( "Scale" ) );

    ScaleSlider = new QSlider( DisplayGroup, "ScaleSlider" );
    ScaleSlider->setGeometry( QRect( 97, 220, 270, 31 ) ); 
    ScaleSlider->setMinValue( 1 );
    ScaleSlider->setMaxValue( 5 );
    ScaleSlider->setOrientation( QSlider::Horizontal );

    ScaleValLabel = new QLabel( DisplayGroup, "ScaleValLabel" );
    ScaleValLabel->setGeometry( QRect( 380, 220, 20, 31 ) ); 
    ScaleValLabel->setText( trUtf8( "5" ) );

    VectorGroup = new QGroupBox( DisplayGroup, "VectorGroup" );
    VectorGroup->setEnabled( TRUE );
    VectorGroup->setGeometry( QRect( 17, 261, 391, 210 ) ); 
    VectorGroup->setTitle( trUtf8( "Vector" ) );

    AliasCheck = new QCheckBox( VectorGroup, "AliasCheck" );
    AliasCheck->setGeometry( QRect( 10, 40, 170, 31 ) ); 
    AliasCheck->setText( trUtf8( "Antialiasing" ) );

    TransCheck = new QCheckBox( VectorGroup, "TransCheck" );
    TransCheck->setGeometry( QRect( 190, 40, 190, 31 ) ); 
    TransCheck->setText( trUtf8( "Translucency" ) );

    ResBox = new QComboBox( FALSE, VectorGroup, "ResBox" );
    ResBox->setGeometry( QRect( 170, 80, 210, 40 ) );
    ResBox->setFocusPolicy( QComboBox::TabFocus );
    ResBox->setAcceptDrops( TRUE );
    ResBox->setMaxCount( 6 );
    ResBox->insertItem(trUtf8("Use Scale"));
    ResBox->insertItem(trUtf8("640 x 480"));
    ResBox->insertItem(trUtf8("800 x 600"));
    ResBox->insertItem(trUtf8("1024 x 768"));
    ResBox->insertItem(trUtf8("1280 x 1024"));
    ResBox->insertItem(trUtf8("1600 x 1200"));

    ResLabel = new QLabel( VectorGroup, "ResLabel" );
    ResLabel->setGeometry( QRect( 10, 82, 150, 31 ) );
    ResLabel->setText( trUtf8( "Resolution" ) );

    BeamLabel = new QLabel( VectorGroup, "BeamLabel" );
    BeamLabel->setGeometry( QRect( 10, 129, 80, 31 ) ); 
    BeamLabel->setText( trUtf8( "Beam" ) );

    BeamSlider = new QSlider( VectorGroup, "BeamSlider" );
    BeamSlider->setGeometry( QRect( 100, 130, 220, 31 ) ); 
    BeamSlider->setMouseTracking( TRUE );
    BeamSlider->setMinValue( 10 );
    BeamSlider->setMaxValue( 150 );
    BeamSlider->setOrientation( QSlider::Horizontal );

    BeamValLabel = new QLabel( VectorGroup, "BeamValLabel" );
    BeamValLabel->setGeometry( QRect( 330, 130, 52, 31 ) ); 
    BeamValLabel->setText( trUtf8( "13.8" ) );

    FlickerLabel = new QLabel( VectorGroup, "FlickerLabel" );
    FlickerLabel->setGeometry( QRect( 10, 169, 90, 31 ) ); 
    FlickerLabel->setText( trUtf8( "Flicker" ) );

    FlickerSlider = new QSlider( VectorGroup, "FlickerSlider" );
    FlickerSlider->setGeometry( QRect( 100, 170, 220, 30 ) ); 
    FlickerSlider->setOrientation( QSlider::Horizontal );

    FlickerValLabel = new QLabel( VectorGroup, "FlickerValLabel" );
    FlickerValLabel->setGeometry( QRect( 330, 170, 52, 31 ) ); 
    FlickerValLabel->setText( trUtf8( "88.8" ) );

    SoundGroup = new QGroupBox( SettingsFrame, "SoundGroup" );
    SoundGroup->setGeometry( QRect( 410, 230, 390, 160 ) ); 
    //SoundGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    SoundGroup->setTitle( trUtf8( "Sound" ) );

    VolumeLabel = new QLabel( SoundGroup, "VolumeLabel" );
    VolumeLabel->setGeometry( QRect( 17, 120, 95, 31 ) ); 
    VolumeLabel->setText( trUtf8( "Volume" ) );

    SoundCheck = new QCheckBox( SoundGroup, "SoundCheck" );
    SoundCheck->setGeometry( QRect( 7, 40, 160, 31 ) ); 
    SoundCheck->setText( trUtf8( "Use Sound" ) );

    SamplesCheck = new QCheckBox( SoundGroup, "SamplesCheck" );
    SamplesCheck->setGeometry( QRect( 177, 40, 201, 31 ) ); 
    SamplesCheck->setText( trUtf8( "Use Samples" ) );

    FakeCheck = new QCheckBox( SoundGroup, "FakeCheck" );
    FakeCheck->setGeometry( QRect( 7, 80, 180, 31 ) ); 
    FakeCheck->setText( trUtf8( "Fake Sound" ) );

    VolumeValLabel = new QLabel( SoundGroup, "VolumeValLabel" );
    VolumeValLabel->setGeometry( QRect( 340, 120, 40, 27 ) ); 
    VolumeValLabel->setText( trUtf8( "-32" ) );

    VolumeSlider = new QSlider( SoundGroup, "VolumeSlider" );
    VolumeSlider->setGeometry( QRect( 117, 120, 220, 31 ) ); 
    VolumeSlider->setMinValue( -32 );
    VolumeSlider->setMaxValue( 0 );
    VolumeSlider->setOrientation( QSlider::Horizontal );

    MiscGroup = new QGroupBox( SettingsFrame, "MiscGroup" );
    MiscGroup->setGeometry( QRect( 407, 390, 391, 91 ) ); 
    //MiscGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    MiscGroup->setTitle( trUtf8( "Miscelaneous" ) );

    OptionsLabel = new QLabel( MiscGroup, "OptionsLabel" );
    OptionsLabel->setGeometry( QRect( 10, 50, 166, 31 ) ); 
    OptionsLabel->setText( trUtf8( "Extra Options" ) );

    CheatCheck = new QCheckBox( MiscGroup, "CheatCheck" );
    CheatCheck->setGeometry( QRect( 10, 30, 100, 21 ) ); 
    CheatCheck->setText( trUtf8( "Cheat" ) );

    OptionsEdit = new QLineEdit( MiscGroup, "OptionsEdit" );
    OptionsEdit->setGeometry( QRect( 180, 40, 201, 37 ) ); 
    OptionsEdit->setFocusPolicy( QLineEdit::TabFocus );
    OptionsEdit->setMaxLength( 1023 );

    InputGroup = new QGroupBox( SettingsFrame, "InputGroup" );
    InputGroup->setGeometry( QRect( 410, 0, 410, 230 ) ); 
    //InputGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    InputGroup->setTitle( trUtf8( "Input" ) );

    JoyLabel = new QLabel( InputGroup, "JoyLabel" );
    JoyLabel->setGeometry( QRect( 27, 180, 105, 31 ) ); 
    JoyLabel->setText( trUtf8( "Joystick" ) );

    WinkeyCheck = new QCheckBox( InputGroup, "WinkeyCheck" );
    WinkeyCheck->setGeometry( QRect( 11, 78, 388, 31 ) ); 
    WinkeyCheck->setText( trUtf8( "Use Windows Keys" ) );

    MouseCheck = new QCheckBox( InputGroup, "MouseCheck" );
    MouseCheck->setGeometry( QRect( 11, 125, 170, 31 ) ); 
    MouseCheck->setText( trUtf8( "Use Mouse" ) );

    GrabCheck = new QCheckBox( InputGroup, "GrabCheck" );
    GrabCheck->setGeometry( QRect( 188, 125, 191, 31 ) ); 
    GrabCheck->setText( trUtf8( "Grab Mouse" ) );

    JoyBox = new QComboBox( FALSE, InputGroup, "JoyBox" );
    JoyBox->setGeometry( QRect( 137, 170, 240, 41 ) ); 
    JoyBox->setFocusPolicy( QComboBox::TabFocus );
    JoyBox->setSizeLimit( 5 );
    JoyBox->insertItem(trUtf8("No Joystick"));
    JoyBox->insertItem(trUtf8("i386 Joystick"));
    JoyBox->insertItem(trUtf8("Fm Town Pad"));
    JoyBox->insertItem(trUtf8("X11 Joystick"));
    JoyBox->insertItem(trUtf8("1.X.X Joystick"));

    JoyCheck = new QCheckBox( InputGroup, "JoyCheck" );
    JoyCheck->setGeometry( QRect( 11, 31, 388, 31 ) ); 
    JoyCheck->setText( trUtf8( "Use Analog Joystick" ) );

    SaveButton = new QPushButton( SettingsTab, "SaveButton" );
    SaveButton->setGeometry( QRect( 628, 513, 151, 40 ) ); 
    SaveButton->setText( trUtf8( "Save" ) );

    if(!bSystem)
    {
        DefaultCheck = new QCheckBox( SettingsTab, "DefaultCheck" );
        DefaultCheck->setGeometry( QRect( 8, 3, 190, 31 ) ); 
        DefaultCheck->setText( trUtf8( "use defaults" ) );
    }
    MameTab->insertTab( SettingsTab, trUtf8( "Settings" ) );

    if(!bSystem)
    {
        ScreenTab = new QWidget( MameTab, "ScreenTab" );

        ScreenPic = new QLabel( ScreenTab, "ScreenPic" );
        ScreenPic->setEnabled( TRUE );
        ScreenPic->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
        ScreenPic->setScaledContents( TRUE );
        MameTab->insertTab( ScreenTab, trUtf8( "ScreenShot" ) );

        flyer = new QWidget( MameTab, "flyer" );

        FlyerPic = new QLabel( flyer, "FlyerPic" );
        FlyerPic->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
        FlyerPic->setScaledContents( TRUE );
        MameTab->insertTab( flyer, trUtf8( "Flyer" ) );

        cabinet = new QWidget( MameTab, "cabinet" );

        CabinetPic = new QLabel( cabinet, "CabinetPic" );
        CabinetPic->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
        CabinetPic->setScaledContents( TRUE );
        MameTab->insertTab( cabinet, trUtf8( "Cabinet Photo" ) );
    }

    // tab order
    if(bSystem)
    {
        setTabOrder( MameTab, FullCheck );
    }
    else
    {
        setTabOrder( MameTab, DefaultCheck );
        setTabOrder( DefaultCheck, FullCheck );
    }
    setTabOrder( FullCheck, SkipCheck );
    setTabOrder( SkipCheck, LeftCheck );
    setTabOrder( LeftCheck, RightCheck );
    setTabOrder( RightCheck, FlipXCheck );
    setTabOrder( FlipXCheck, FlipYCheck );
    setTabOrder( FlipYCheck, ScanCheck );
    setTabOrder( ScanCheck, ExtraArtCheck );
    setTabOrder( ExtraArtCheck, ColorCheck );
    setTabOrder( ColorCheck, ScaleSlider );
    setTabOrder( ScaleSlider, AliasCheck );
    setTabOrder( AliasCheck, TransCheck );
    setTabOrder( TransCheck, ResBox );
    setTabOrder( ResBox, BeamSlider );
    setTabOrder( BeamSlider, FlickerSlider );
    setTabOrder( FlickerSlider, JoyCheck );
    setTabOrder( JoyCheck, WinkeyCheck );
    setTabOrder( WinkeyCheck, MouseCheck );
    setTabOrder( MouseCheck, GrabCheck );
    setTabOrder( GrabCheck, JoyBox );
    setTabOrder( JoyBox, SoundCheck );
    setTabOrder( SoundCheck, SamplesCheck );
    setTabOrder( SamplesCheck, FakeCheck );
    setTabOrder( FakeCheck, VolumeSlider );
    setTabOrder( VolumeSlider, CheatCheck );
    setTabOrder( CheatCheck, OptionsEdit );
    setTabOrder( OptionsEdit, SaveButton );

    if(!bSystem)
    {
        connect(DefaultCheck, SIGNAL(stateChanged(int)), this,
            SLOT(defaultUpdate(int)));
    }
    connect(VolumeSlider, SIGNAL(valueChanged(int)), VolumeValLabel,
            SLOT(setNum(int)));
    connect(ScaleSlider, SIGNAL(valueChanged(int)), ScaleValLabel,
            SLOT(setNum(int)));
    connect(FlickerSlider, SIGNAL(valueChanged(int)), this,
            SLOT(setFlickerLabel(int)));
    connect(BeamSlider, SIGNAL(valueChanged(int)), this,
            SLOT(setBeamLabel(int)));
    connect(SaveButton, SIGNAL(pressed()), this,
            SLOT(accept()));

    if(bSystem)
    {
        SettingsFrame->setEnabled(true);
        ListButton = new QPushButton( SettingsTab, "ListButton" );
        ListButton->setGeometry( QRect( 28, 513, 250, 40 ) );
        ListButton->setText( trUtf8( "Reload Game List" ) );
        setTabOrder( SaveButton, ListButton );
        connect(ListButton, SIGNAL(pressed()), this,
                SLOT(loadList()));
    }
}

/*  
 *  Destroys the object and frees any allocated resources
 */
MameSettingsDlg::~MameSettingsDlg()
{
    // no need to delete child widgets, Qt does it all for us
}

int MameSettingsDlg::Show(GameSettings *settings, bool vector)
{
    game_settings = settings;
    if(!bSystem)
    {
        DefaultCheck->setChecked(game_settings->default_options);
    }
    VectorGroup->setEnabled(vector);
    FullCheck->setChecked(game_settings->fullscreen);
    SkipCheck->setChecked(game_settings->autoframeskip);
    LeftCheck->setChecked(game_settings->rot_left);
    RightCheck->setChecked(game_settings->rot_right);
    FlipXCheck->setChecked(game_settings->flipx);
    FlipYCheck->setChecked(game_settings->flipy);
    ExtraArtCheck->setChecked(game_settings->extra_artwork);
    ScanCheck->setChecked(game_settings->scanlines);
    ColorCheck->setChecked(game_settings->auto_colordepth);
    ScaleSlider->setValue(game_settings->scale);
    ScaleValLabel->setNum(game_settings->scale);
    AliasCheck->setChecked(game_settings->antialias);
    TransCheck->setChecked(game_settings->translucency);
    BeamSlider->setValue(int(game_settings->beam * 10));
    setBeamLabel(game_settings->beam * 10);
    FlickerSlider->setValue(int(game_settings->flicker * 10));
    setFlickerLabel(game_settings->flicker * 10);
    SoundCheck->setChecked(game_settings->sound);
    SamplesCheck->setChecked(game_settings->samples);
    FakeCheck->setChecked(game_settings->fake_sound);
    VolumeValLabel->setNum(game_settings->volume);
    VolumeSlider->setValue(game_settings->volume);
    CheatCheck->setChecked(game_settings->cheat);
    OptionsEdit->setText(game_settings->extra_options);
    WinkeyCheck->setChecked(game_settings->winkeys);
    MouseCheck->setChecked(game_settings->mouse);
    GrabCheck->setChecked(game_settings->grab_mouse);
    showFullScreen();
    setActiveWindow();
    if(exec())
    {
        SaveSettings();
        return SAVE_SETTINGS;
    }
    else
        return DONT_SAVE_SETTINGS;
}

void MameSettingsDlg::defaultUpdate(int state)
{
    SettingsFrame->setEnabled(!state);
    game_settings->default_options = state;
}

void MameSettingsDlg::setBeamLabel(int value)
{
    float actualValue = float(value) / 10.0;
    BeamValLabel->setNum(actualValue);
    game_settings->beam = actualValue;
}

void MameSettingsDlg::setFlickerLabel(int value)
{
    FlickerValLabel->setNum(value);
    game_settings->flicker = value;
}

void MameSettingsDlg::SaveSettings()
{
    if(!bSystem)
        game_settings->default_options = DefaultCheck->isChecked();
    game_settings->fullscreen = FullCheck->isChecked();
    game_settings->autoframeskip = SkipCheck->isChecked();
    game_settings->rot_left = LeftCheck->isChecked();
    game_settings->rot_right = RightCheck->isChecked();
    game_settings->flipx = FlipXCheck->isChecked();
    game_settings->flipy = FlipYCheck->isChecked();
    game_settings->extra_artwork = ExtraArtCheck->isChecked();
    game_settings->scanlines = ScanCheck->isChecked();
    game_settings->auto_colordepth = ColorCheck->isChecked();
    game_settings->scale = ScaleSlider->value();
    game_settings->antialias = AliasCheck->isChecked();
    game_settings->translucency = TransCheck->isChecked();
    game_settings->beam = float(BeamSlider->value()) / 10.0;
    game_settings->flicker = FlickerSlider->value();
    game_settings->sound = SoundCheck->isChecked();
    game_settings->samples = SamplesCheck->isChecked();
    game_settings->fake_sound = FakeCheck->isChecked();
    game_settings->volume = VolumeSlider->value();
    game_settings->cheat = CheatCheck->isChecked();
    game_settings->extra_options = OptionsEdit->text();
    game_settings->winkeys = WinkeyCheck->isChecked();
    game_settings->mouse = MouseCheck->isChecked();
    game_settings->grab_mouse = GrabCheck->isChecked();
}

void MameSettingsDlg::SetScreenshot(QString picfile)
{
    if(ScreenPic)
    {
        Screenshot.load(picfile);
        ScaleImageLabel(Screenshot,ScreenPic);
        ScreenPic->setPixmap(Screenshot);
    }
}

void MameSettingsDlg::SetFlyer(QString picfile)
{
    if(FlyerPic)
    {
        Flyer.load(picfile);
        ScaleImageLabel(Flyer,FlyerPic);
        FlyerPic->setPixmap(Flyer);
    }
}

void MameSettingsDlg::SetCabinet(QString picfile)
{
    if(CabinetPic)
    {
        Cabinet.load(picfile);
        ScaleImageLabel(Cabinet,CabinetPic);
        CabinetPic->setPixmap(Cabinet);
    }
}

void MameSettingsDlg::ScaleImageLabel(QPixmap &pixmap, QLabel *label)
{
    int FrameWidth = width();
    int FrameHeight = height() - 33;
    int ImageWidth = pixmap.width();
    int ImageHeight = pixmap.height();
    int x = 0, y = 0;
    QPoint test = label->mapToGlobal(QPoint(0,0));
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
    label->setGeometry( QRect( x, y, ImageWidth, ImageHeight ) );
}

void MameSettingsDlg::loadList()
{
    MameHandler::getHandler()->processGames();
}
