#include "mamesettingsdlg.h"
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
#include <qmsgbox.h>

#include "mamehandler.h"

#include <mythtv/mythcontext.h>

using namespace std;

#define SAVE_SETTINGS 1
#define DONT_SAVE_SETTINGS 0

MameSettingsDlg::MameSettingsDlg(MythContext *context, QWidget* parent,  
                                 const char* name, bool modal, bool system, 
                                 WFlags fl)
    : QDialog( parent, name, modal, fl ), bSystem(system)
{
    m_context = context;

    setCursor(QCursor(Qt::BlankCursor));
    if ( !name )
      setName( "MameSettings" );

    int screenwidth = 0, screenheight = 0;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

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
    MameTab_font.setPointSize( (int)(m_context->GetMediumFontSize() * wmult));
    MameTab_font.setBold( TRUE );
    MameTab->setFont( MameTab_font );

    SettingsTab = new QWidget( MameTab, "SettingsTab" );

    SettingsFrame = new QFrame( SettingsTab, "SettingsFrame" );
    SettingsFrame->setGeometry( QRect(0, (int)(33 * hmult), 
                                      (int)(800 * wmult), (int)(480 * hmult)));
    //SettingsFrame->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    SettingsFrame->setFrameShape( QFrame::StyledPanel );
    SettingsFrame->setFrameShadow( QFrame::Raised );

    DisplayGroup = new QGroupBox( SettingsFrame, "DisplayGroup" );
    DisplayGroup->setGeometry( QRect(0, (int)(10 * hmult), 
                                     (int)(420 * wmult), (int)(480 * hmult)));
    //DisplayGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    DisplayGroup->setTitle( trUtf8( "Display" ) );

    FullCheck = new QCheckBox( DisplayGroup, "FullCheck" );
    FullCheck->setGeometry( QRect((int)(11 * wmult), (int)(31 * hmult), 
                                  (int)(160 * wmult), (int)(31 * hmult))); 
    FullCheck->setText( trUtf8( "Full screen" ) );

    SkipCheck = new QCheckBox( DisplayGroup, "SkipCheck" );
    SkipCheck->setGeometry( QRect( (int)(195 * wmult), (int)(31 * hmult), 
                                   (int)(214 * wmult), (int)(31 * hmult))); 
    SkipCheck->setText( trUtf8( "Auto frame skip" ) );

    LeftCheck = new QCheckBox( DisplayGroup, "LeftCheck" );
    LeftCheck->setGeometry( QRect( (int)(11 * wmult), (int)(68 * hmult), 
                                   (int)(170 * wmult), (int)(31 * hmult) ) ); 
    LeftCheck->setText( trUtf8( "Rotate Left" ) );

    RightCheck = new QCheckBox( DisplayGroup, "RightCheck" );
    RightCheck->setGeometry( QRect( (int)(195 * wmult), (int)(68 * hmult), 
                                    (int)(190 * wmult), (int)(31 * hmult) ) ); 
    RightCheck->setText( trUtf8( "Rotate Right" ) );

    FlipXCheck = new QCheckBox( DisplayGroup, "FlipXCheck" );
    FlipXCheck->setGeometry( QRect( (int)(11 * wmult), (int)(105 * hmult), 
                                    (int)(178 * wmult), (int)(31 * hmult))); 
    FlipXCheck->setText( trUtf8( "Flip X Axis" ) );

    FlipYCheck = new QCheckBox( DisplayGroup, "FlipYCheck" );
    FlipYCheck->setGeometry( QRect( (int)(195 * wmult), (int)(105 * hmult), 
                                    (int)(180 * wmult), (int)(31 * hmult))); 
    FlipYCheck->setText( trUtf8( "Flip Y Axis" ) );

    ExtraArtCheck = new QCheckBox( DisplayGroup, "ExtraArtCheck" );
    ExtraArtCheck->setGeometry( QRect( (int)(195 * wmult), (int)(142 * hmult), 
                                       (int)(200 * wmult), (int)(31 * hmult))); 
    ExtraArtCheck->setText( trUtf8( "Extra Artwork" ) );

    ScanCheck = new QCheckBox( DisplayGroup, "ScanCheck" );
    ScanCheck->setGeometry( QRect( (int)(10 * wmult), (int)(140 * hmult), 
                                   (int)(178 * wmult), (int)(31 * hmult) ) ); 
    ScanCheck->setText( trUtf8( "Scanlines" ) );

    ColorCheck = new QCheckBox( DisplayGroup, "ColorCheck" );
    ColorCheck->setGeometry( QRect( (int)(11 * wmult), (int)(179 * hmult), 
                                    (int)(398 * wmult), (int)(31 * hmult) ) ); 
    ColorCheck->setText( trUtf8( "Auto color depth" ) );

    ScaleLabel = new QLabel( DisplayGroup, "ScaleLabel" );
    ScaleLabel->setGeometry( QRect( (int)(20 * wmult), (int)(220 * hmult), 
                                    (int)(71 * wmult), (int)(31  * hmult)) ); 
    ScaleLabel->setText( trUtf8( "Scale" ) );

    ScaleSlider = new QSlider( DisplayGroup, "ScaleSlider" );
    ScaleSlider->setGeometry( QRect( (int)(97 * wmult), (int)(220 * hmult), 
                                     (int)(270 * wmult), (int)(31 * hmult) ) ); 
    ScaleSlider->setMinValue( 1 );
    ScaleSlider->setMaxValue( 5 );
    ScaleSlider->setOrientation( QSlider::Horizontal );

    ScaleValLabel = new QLabel( DisplayGroup, "ScaleValLabel" );
    ScaleValLabel->setGeometry( QRect( (int)(380 * wmult), (int)(220 * hmult), 
                                       (int)(20 * wmult), (int)(31 * hmult))); 
    ScaleValLabel->setText( trUtf8( "5" ) );

    VectorGroup = new QGroupBox( DisplayGroup, "VectorGroup" );
    VectorGroup->setEnabled( TRUE );
    VectorGroup->setGeometry( QRect( (int)(17 * wmult), (int)(261 * hmult), 
                                     (int)(391 * wmult), (int)(210 * hmult))); 
    VectorGroup->setTitle( trUtf8( "Vector" ) );

    AliasCheck = new QCheckBox( VectorGroup, "AliasCheck" );
    AliasCheck->setGeometry( QRect( (int)(10 * wmult), (int)(40 * hmult), 
                                    (int)(170 * wmult), (int)(31 * hmult))); 
    AliasCheck->setText( trUtf8( "Antialiasing" ) );

    TransCheck = new QCheckBox( VectorGroup, "TransCheck" );
    TransCheck->setGeometry( QRect( (int)(190 * wmult), (int)(40 * hmult), 
                                    (int)(190 * wmult), (int)(31 * hmult) ) ); 
    TransCheck->setText( trUtf8( "Translucency" ) );

    ResBox = new QComboBox( FALSE, VectorGroup, "ResBox" );
    ResBox->setGeometry( QRect( (int)(170 * wmult), (int)(80 * hmult), 
                                (int)(210 * wmult), (int)(40 * hmult) ) );
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
    ResLabel->setGeometry( QRect( (int)(10 * wmult), (int)(82 * hmult), 
                                  (int)(150 * wmult), (int)(31 * hmult) ) );
    ResLabel->setText( trUtf8( "Resolution" ) );

    BeamLabel = new QLabel( VectorGroup, "BeamLabel" );
    BeamLabel->setGeometry( QRect( (int)(10 * wmult), (int)(129 * hmult), 
                                   (int)(80 * wmult), (int)(31 * hmult) ) ); 
    BeamLabel->setText( trUtf8( "Beam" ) );

    BeamSlider = new QSlider( VectorGroup, "BeamSlider" );
    BeamSlider->setGeometry( QRect( (int)(100 * wmult), (int)(130 * hmult), 
                                    (int)(220 * wmult), (int)(31 * hmult) ) ); 
    BeamSlider->setMouseTracking( TRUE ); 
    BeamSlider->setMinValue( 10 );
    BeamSlider->setMaxValue( 150 );
    BeamSlider->setOrientation( QSlider::Horizontal );

    BeamValLabel = new QLabel( VectorGroup, "BeamValLabel" );
    BeamValLabel->setGeometry( QRect( (int)(330 * wmult), (int)(130 * hmult), 
                                      (int)(52 * wmult), (int)(31 * hmult) ) ); 
    BeamValLabel->setText( trUtf8( "13.8" ) );

    FlickerLabel = new QLabel( VectorGroup, "FlickerLabel" );
    FlickerLabel->setGeometry( QRect( (int)(10 * wmult), (int)(169 * hmult), 
                                      (int)(90 * wmult), (int)(31 * hmult) ) ); 
    FlickerLabel->setText( trUtf8( "Flicker" ) );

    FlickerSlider = new QSlider( VectorGroup, "FlickerSlider" );
    FlickerSlider->setGeometry( QRect( (int)(100 * wmult), (int)(170 * hmult), 
                                       (int)(220 * wmult), (int)(30 * hmult))); 
    FlickerSlider->setOrientation( QSlider::Horizontal );

    FlickerValLabel = new QLabel( VectorGroup, "FlickerValLabel" );
    FlickerValLabel->setGeometry( QRect( (int)(330 * wmult), (int)(170 * hmult),
                                         (int)(52 * wmult), (int)(31 * hmult)));
    FlickerValLabel->setText( trUtf8( "88.8" ) );

    SoundGroup = new QGroupBox( SettingsFrame, "SoundGroup" );
    SoundGroup->setGeometry( QRect( (int)(410 * wmult), (int)(230 * hmult), 
                                    (int)(390 * wmult), (int)(160 * hmult)) ); 
    //SoundGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    SoundGroup->setTitle( trUtf8( "Sound" ) );

    VolumeLabel = new QLabel( SoundGroup, "VolumeLabel" );
    VolumeLabel->setGeometry( QRect( (int)(17 * wmult), (int)(120 * hmult), 
                                     (int)(95 * wmult), (int)(31 * hmult) ) ); 
    VolumeLabel->setText( trUtf8( "Volume" ) );

    SoundCheck = new QCheckBox( SoundGroup, "SoundCheck" );
    SoundCheck->setGeometry( QRect( (int)(7 * wmult), (int)(40 * hmult), 
                                    (int)(160 * wmult), (int)(31 * hmult) ) ); 
    SoundCheck->setText( trUtf8( "Use Sound" ) );

    SamplesCheck = new QCheckBox( SoundGroup, "SamplesCheck" );
    SamplesCheck->setGeometry( QRect( (int)(177 * wmult), (int)(40 * hmult), 
                                      (int)(201 * wmult), (int)(31 * hmult))); 
    SamplesCheck->setText( trUtf8( "Use Samples" ) );

    FakeCheck = new QCheckBox( SoundGroup, "FakeCheck" );
    FakeCheck->setGeometry( QRect( (int)(7 * wmult), (int)(80 * hmult), 
                                   (int)(180 * wmult), (int)(31 * hmult) ) ); 
    FakeCheck->setText( trUtf8( "Fake Sound" ) );

    VolumeValLabel = new QLabel( SoundGroup, "VolumeValLabel" );
    VolumeValLabel->setGeometry( QRect((int)(340 * wmult), (int)(120 * hmult), 
                                       (int)(40 * wmult), (int)(27 * hmult))); 
    VolumeValLabel->setText( trUtf8( "-32" ) );

    VolumeSlider = new QSlider( SoundGroup, "VolumeSlider" );
    VolumeSlider->setGeometry( QRect( (int)(117 * wmult), (int)(120 * hmult), 
                                      (int)(220 * wmult), (int)(31 * hmult))); 
    VolumeSlider->setMinValue( -32 );
    VolumeSlider->setMaxValue( 0 );
    VolumeSlider->setOrientation( QSlider::Horizontal );

    MiscGroup = new QGroupBox( SettingsFrame, "MiscGroup" );
    MiscGroup->setGeometry( QRect( (int)(407 * wmult), (int)(390 * hmult), 
                                   (int)(391 * wmult), (int)(91 * hmult) ) ); 
    //MiscGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    MiscGroup->setTitle( trUtf8( "Miscelaneous" ) );

    OptionsLabel = new QLabel( MiscGroup, "OptionsLabel" );
    OptionsLabel->setGeometry( QRect( (int)(10 * wmult), (int)(50 * hmult), 
                                      (int)(166 * wmult), (int)(31 * hmult))); 
    OptionsLabel->setText( trUtf8( "Extra Options" ) );

    CheatCheck = new QCheckBox( MiscGroup, "CheatCheck" );
    CheatCheck->setGeometry( QRect( (int)(10 * wmult), (int)(30 * hmult), 
                                    (int)(100 * wmult), (int)(21 * hmult) ) ); 
    CheatCheck->setText( trUtf8( "Cheat" ) );

    OptionsEdit = new QLineEdit( MiscGroup, "OptionsEdit" );
    OptionsEdit->setGeometry( QRect( (int)(180 * wmult), (int)(40 * hmult), 
                                     (int)(201 * wmult), (int)(37 * hmult) ) ); 
    OptionsEdit->setFocusPolicy( QLineEdit::TabFocus );
    OptionsEdit->setMaxLength( 1023 );

    InputGroup = new QGroupBox( SettingsFrame, "InputGroup" );
    InputGroup->setGeometry( QRect( (int)(410 * wmult), 0, (int)(410 * wmult), 
                                    (int)(230 * hmult) ) ); 
    //InputGroup->setPaletteBackgroundColor( QColor( 192, 192, 192 ) );
    InputGroup->setTitle( trUtf8( "Input" ) );

    JoyLabel = new QLabel( InputGroup, "JoyLabel" );
    JoyLabel->setGeometry( QRect( (int)(27 * wmult), (int)(180 * hmult), 
                                  (int)(105 * wmult), (int)(31 * hmult)) ); 
    JoyLabel->setText( trUtf8( "Joystick" ) );

    WinkeyCheck = new QCheckBox( InputGroup, "WinkeyCheck" );
    WinkeyCheck->setGeometry( QRect( (int)(11 * wmult), (int)(78 * hmult), 
                                     (int)(388 * wmult), (int)(31 * hmult) ) ); 
    WinkeyCheck->setText( trUtf8( "Use Windows Keys" ) );

    MouseCheck = new QCheckBox( InputGroup, "MouseCheck" );
    MouseCheck->setGeometry( QRect( (int)(11 * wmult), (int)(125 * hmult), 
                                    (int)(170 * wmult), (int)(31 * hmult) ) ); 
    MouseCheck->setText( trUtf8( "Use Mouse" ) );

    GrabCheck = new QCheckBox( InputGroup, "GrabCheck" );
    GrabCheck->setGeometry( QRect( (int)(188 * wmult), (int)(125 * hmult), 
                                   (int)(191 * wmult), (int)(31 * hmult) ) ); 
    GrabCheck->setText( trUtf8( "Grab Mouse" ) );

    JoyBox = new QComboBox( FALSE, InputGroup, "JoyBox" );
    JoyBox->setGeometry( QRect( (int)(137 * wmult), (int)(170 * hmult), 
                                (int)(240 * wmult), (int)(41 * hmult) ) ); 
    JoyBox->setFocusPolicy( QComboBox::TabFocus );
    JoyBox->setSizeLimit( 5 );
    JoyBox->insertItem(trUtf8("No Joystick"));
    JoyBox->insertItem(trUtf8("i386 Joystick"));
    JoyBox->insertItem(trUtf8("Fm Town Pad"));
    JoyBox->insertItem(trUtf8("X11 Joystick"));
    JoyBox->insertItem(trUtf8("1.X.X Joystick"));

    JoyCheck = new QCheckBox( InputGroup, "JoyCheck" );
    JoyCheck->setGeometry( QRect( (int)(11 * wmult), (int)(31 * hmult),
                                  (int)(388 * wmult), (int)(31 * hmult) ) ); 
    JoyCheck->setText( trUtf8( "Use Analog Joystick" ) );

    SaveButton = new QPushButton( SettingsTab, "SaveButton" );
    SaveButton->setGeometry( QRect( (int)(628 * wmult), (int)(513 * hmult), 
                                    (int)(151 * wmult), (int)(40 * hmult))); 
    SaveButton->setText( trUtf8( "Save" ) );

    if(!bSystem)
    {
        DefaultCheck = new QCheckBox( SettingsTab, "DefaultCheck" );
        DefaultCheck->setGeometry(QRect((int)(8 * wmult), (int)(3 * hmult), 
                                        (int)(190 * wmult), (int)(31 * hmult))); 
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
        ListButton->setGeometry( QRect( (int)(28 * wmult), (int)(513 * hmult), 
                                        (int)(250 * wmult), (int)(40 * hmult)));
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
    setBeamLabel((int)(game_settings->beam * 10));
    FlickerSlider->setValue(int(game_settings->flicker * 10));
    setFlickerLabel((int)(game_settings->flicker * 10));
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
    int FrameHeight = (int)(height() - 33 * hmult);
    int ImageWidth = pixmap.width();
    int ImageHeight = pixmap.height();
    int x = 0, y = 0;
    //QPoint test = label->mapToGlobal(QPoint(0,0));
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
    MameHandler::getHandler(m_context)->processGames();
}
