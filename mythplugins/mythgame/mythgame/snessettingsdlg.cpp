#include "snessettingsdlg.h"
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

#include "sneshandler.h"

#include <mythtv/mythcontext.h>

using namespace std;

#define SAVE_SETTINGS 1
#define DONT_SAVE_SETTINGS 0

SnesSettingsDlg::SnesSettingsDlg(MythMainWindow *parent,
                                 const char *name, bool system)
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
    setCaption( trUtf8( "Snes Settings" ) );

    SnesTab = new QTabWidget( this, "SnesTab" );
    SnesTab->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
    SnesTab->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, SnesTab->sizePolicy().hasHeightForWidth() ) );
    SnesTab->setBackgroundOrigin( QTabWidget::WindowOrigin );
    QFont SnesTab_font(  SnesTab->font() );
    SnesTab_font.setFamily( "Helvetica [Urw]" );
    SnesTab_font.setPointSize( (int)(gContext->GetMediumFontSize() * wmult));
    SnesTab_font.setBold( TRUE );
    SnesTab->setFont( SnesTab_font );

    SettingsTab = new QWidget( SnesTab, "SettingsTab" );
    if(bSystem)
        SnesTab->insertTab( SettingsTab, trUtf8( "defaults" ) );
    else
        SnesTab->insertTab( SettingsTab, trUtf8( "Settings" ) );

    SettingsFrame = new QFrame( SettingsTab, "SettingsFrame" );
    SettingsFrame->setGeometry( QRect(0, (int)(33 * hmult),
                                      (int)(800 * wmult), (int)(480 * hmult)));
    SettingsFrame->setFrameShape( QFrame::StyledPanel );
    SettingsFrame->setFrameShadow( QFrame::Raised );

    if(!bSystem)
    {
        DefaultCheck = new QCheckBox( SettingsTab, "DefaultCheck" );
        DefaultCheck->setGeometry(QRect((int)(8 * wmult), (int)(3 * hmult),
                                        (int)(190 * wmult), (int)(31 * hmult)));
        DefaultCheck->setText( trUtf8( "use defaults" ) );
    }

    DisplayGroup = new QGroupBox( SettingsFrame, "DisplayGroup" );
    DisplayGroup->setGeometry( QRect(0, (int)(10 * hmult),
                                     (int)(420 * wmult), (int)(175 * hmult)));
    DisplayGroup->setTitle( trUtf8( "Display" ) );

    TransCheck = new QCheckBox( DisplayGroup, "TransCheck" );
    TransCheck->setGeometry( QRect((int)(11 * wmult), (int)(31 * hmult),
                                  (int)(190 * wmult), (int)(31 * hmult)));
    TransCheck->setText( trUtf8( "Transparency" ) );

    SixteenCheck = new QCheckBox( DisplayGroup, "SixteenCheck" );
    SixteenCheck->setGeometry( QRect( (int)(225 * wmult), (int)(31 * hmult),
                                   (int)(184 * wmult), (int)(31 * hmult)));
    SixteenCheck->setText( trUtf8( "16 Bit" ) );

    HiResCheck = new QCheckBox( DisplayGroup, "HiResCheck" );
    HiResCheck->setGeometry( QRect( (int)(11 * wmult), (int)(60 * hmult),
                                   (int)(150 * wmult), (int)(31 * hmult) ) );
    HiResCheck->setText( trUtf8( "Use Hi-res" ) );

    NoModeSwitchCheck = new QCheckBox( DisplayGroup, "NoModeSwitchCheck" );
    NoModeSwitchCheck->setGeometry( QRect( (int)(175 * wmult), (int)(60 * hmult),
                                    (int)(210 * wmult), (int)(31 * hmult) ) );
    NoModeSwitchCheck->setText( trUtf8( "no modeswitch" ) );

    FullScreenCheck = new QCheckBox( DisplayGroup, "FullScreenCheck" );
    FullScreenCheck->setGeometry( QRect( (int)(11 * wmult), (int)(89 * hmult),
                                    (int)(178 * wmult), (int)(31 * hmult)));
    FullScreenCheck->setText( trUtf8( "Full Screen" ) );

    StretchCheck = new QCheckBox( DisplayGroup, "StretchCheck" );
    StretchCheck->setGeometry( QRect( (int)(195 * wmult), (int)(89 * hmult),
                                    (int)(180 * wmult), (int)(31 * hmult)));
    StretchCheck->setText( trUtf8( "Stretch to fit" ) );

    InterpolateBox = new QComboBox( FALSE, DisplayGroup, "InterpolateBox" );
    InterpolateBox->setGeometry( QRect( (int)(170 * wmult), (int)(126 * hmult),
                                (int)(210 * wmult), (int)(40 * hmult) ) );
    InterpolateBox->setFocusPolicy( QComboBox::TabFocus );
    InterpolateBox->setAcceptDrops( TRUE );
    InterpolateBox->setMaxCount( 6 );
    InterpolateBox->insertItem(trUtf8("None"));
    InterpolateBox->insertItem(trUtf8("Interpolate 1"));
    InterpolateBox->insertItem(trUtf8("Interpolate 2"));
    InterpolateBox->insertItem(trUtf8("Interpolate 3"));
    InterpolateBox->insertItem(trUtf8("Interpolate 4"));
    InterpolateBox->insertItem(trUtf8("Interpolate 5"));

    InterpolateLabel = new QLabel( DisplayGroup, "InterpolateLabel" );
    InterpolateLabel->setGeometry( QRect( (int)(10 * wmult), (int)(126 * hmult),
                                  (int)(150 * wmult), (int)(31 * hmult) ) );
    InterpolateLabel->setText( trUtf8( "Interpolation" ) );

    MiscGroup = new QGroupBox( SettingsFrame, "MiscGroup" );
    MiscGroup->setGeometry( QRect(0, (int)(185 * hmult),
                                     (int)(420 * wmult), (int)(285 * hmult)));
    MiscGroup->setTitle( trUtf8( "Misc" ) );

    NoJoyCheck = new QCheckBox( MiscGroup, "NoJoyCheck" );
    NoJoyCheck->setGeometry( QRect((int)(11 * wmult), (int)(31 * hmult),
                                  (int)(190 * wmult), (int)(31 * hmult)));
    NoJoyCheck->setText( trUtf8( "No Joystick" ) );

    LayeringCheck = new QCheckBox( MiscGroup, "LayeringCheck" );
    LayeringCheck->setGeometry( QRect( (int)(225 * wmult), (int)(31 * hmult),
                                   (int)(184 * wmult), (int)(31 * hmult)));
    LayeringCheck->setText( trUtf8( "Layering" ) );

    InterleavedCheck = new QCheckBox( MiscGroup, "InterleavedCheck" );
    InterleavedCheck->setGeometry( QRect( (int)(11 * wmult), (int)(60 * hmult),
                                   (int)(180 * wmult), (int)(31 * hmult) ) );
    InterleavedCheck->setText( trUtf8( "Interleaved" ) );

    LowRomCheck = new QCheckBox( MiscGroup, "LowRomCheck" );
    LowRomCheck->setGeometry( QRect( (int)(225 * wmult), (int)(60 * hmult),
                                    (int)(180 * wmult), (int)(31 * hmult)));
    LowRomCheck->setText( trUtf8( "Low Rom" ) );

    AltInterleavedCheck = new QCheckBox( MiscGroup, "AltInterleavedCheck" );
    AltInterleavedCheck->setGeometry( QRect( (int)(11 * wmult), (int)(89 * hmult),
                                    (int)(210 * wmult), (int)(31 * hmult) ) );
    AltInterleavedCheck->setText( trUtf8( "Alt Interleaved" ) );

    HiRomCheck = new QCheckBox( MiscGroup, "HiRomCheck" );
    HiRomCheck->setGeometry( QRect( (int)(225 * wmult), (int)(89 * hmult),
                                    (int)(178 * wmult), (int)(31 * hmult)));
    HiRomCheck->setText( trUtf8( "Hi Rom" ) );

    HeaderCheck = new QCheckBox( MiscGroup, "HeaderCheck" );
    HeaderCheck->setGeometry( QRect( (int)(11 * wmult), (int)(118 * hmult),
                                    (int)(210 * wmult), (int)(31 * hmult) ) );
    HeaderCheck->setText( trUtf8( "Header" ) );

    NtscCheck = new QCheckBox( MiscGroup, "NtscCheck" );
    NtscCheck->setGeometry( QRect( (int)(225 * wmult), (int)(118 * hmult),
                                    (int)(178 * wmult), (int)(31 * hmult)));
    NtscCheck->setText( trUtf8( "NTSC" ) );

    NoHeaderCheck = new QCheckBox( MiscGroup, "NoHeaderCheck" );
    NoHeaderCheck->setGeometry( QRect( (int)(11 * wmult), (int)(147 * hmult),
                                    (int)(210 * wmult), (int)(31 * hmult) ) );
    NoHeaderCheck->setText( trUtf8( "No Header" ) );

    PalCheck = new QCheckBox( MiscGroup, "PalCheck" );
    PalCheck->setGeometry( QRect( (int)(225 * wmult), (int)(147 * hmult),
                                    (int)(178 * wmult), (int)(31 * hmult)));
    PalCheck->setText( trUtf8( "PAL" ) );

    NoHdmaCheck = new QCheckBox( MiscGroup, "NoHdmaCheck" );
    NoHdmaCheck->setGeometry( QRect( (int)(11 * wmult), (int)(176 * hmult),
                                    (int)(210 * wmult), (int)(31 * hmult) ) );
    NoHdmaCheck->setText( trUtf8( "No H-DMA" ) );

    NoWindowsCheck = new QCheckBox( MiscGroup, "NoWindowsCheck" );
    NoWindowsCheck->setGeometry( QRect( (int)(225 * wmult), (int)(176 * hmult),
                                    (int)(178 * wmult), (int)(31 * hmult)));
    NoWindowsCheck->setText( trUtf8( "No Windows" ) );

    NoSpeedHacksCheck = new QCheckBox( MiscGroup, "NoSpeedHacksCheck" );
    NoSpeedHacksCheck->setGeometry( QRect( (int)(11 * wmult), (int)(205 * hmult),
                                    (int)(260 * wmult), (int)(31 * hmult) ) );
    NoSpeedHacksCheck->setText( trUtf8( "No Speed Hacks" ) );

    OptionsLabel = new QLabel( MiscGroup, "OptionsLabel" );
    OptionsLabel->setGeometry( QRect( (int)(20 * wmult), (int)(240 * hmult),
                                    (int)(166 * wmult), (int)(31  * hmult)) );
    OptionsLabel->setText( trUtf8( "Extra Options" ) );

    OptionsEdit = new QLineEdit( MiscGroup, "OptionsEdit" );
    OptionsEdit->setGeometry( QRect( (int)(195 * wmult), (int)(240 * hmult),
                                     (int)(201 * wmult), (int)(37 * hmult) ) );
    OptionsEdit->setFocusPolicy( QLineEdit::TabFocus );
    OptionsEdit->setMaxLength( 1023 );

    SoundGroup = new QGroupBox( SettingsFrame, "SoundGroup" );
    SoundGroup->setGeometry( QRect((int)(430 * wmult), (int)(10 * hmult),
                                     (int)(360 * wmult), (int)(460 * hmult)));
    SoundGroup->setTitle( trUtf8( "Sound" ) );

    NoSoundCheck = new QCheckBox( SoundGroup, "NoSoundCheck" );
    NoSoundCheck->setGeometry( QRect((int)(11 * wmult), (int)(31 * hmult),
                                  (int)(170 * wmult), (int)(31 * hmult)));
    NoSoundCheck->setText( trUtf8( "No Sound" ) );

    StreoCheck = new QCheckBox( SoundGroup, "StreoCheck" );
    StreoCheck->setGeometry( QRect( (int)(195 * wmult), (int)(31 * hmult),
                                   (int)(154 * wmult), (int)(31 * hmult)));
    StreoCheck->setText( trUtf8( "Stereo" ) );

    EnvxCheck = new QCheckBox( SoundGroup, "EnvxCheck" );
    EnvxCheck->setGeometry( QRect((int)(11 * wmult), (int)(63 * hmult),
                                  (int)(170 * wmult), (int)(31 * hmult)));
    EnvxCheck->setText( trUtf8( "Envx" ) );

    NoEchoCheck = new QCheckBox( SoundGroup, "NoEchoCheck" );
    NoEchoCheck->setGeometry( QRect( (int)(195 * wmult), (int)(63 * hmult),
                                   (int)(154 * wmult), (int)(31 * hmult)));
    NoEchoCheck->setText( trUtf8( "No Echo" ) );

    ThreadSoundCheck = new QCheckBox( SoundGroup, "ThreadSoundCheck" );
    ThreadSoundCheck->setGeometry( QRect((int)(11 * wmult), (int)(95 * hmult),
                                  (int)(170 * wmult), (int)(31 * hmult)));
    ThreadSoundCheck->setText( trUtf8( "Threaded" ) );

    SyncSoundCheck = new QCheckBox( SoundGroup, "SyncSoundCheck" );
    SyncSoundCheck->setGeometry( QRect( (int)(195 * wmult), (int)(95 * hmult),
                                   (int)(154 * wmult), (int)(31 * hmult)));
    SyncSoundCheck->setText( trUtf8( "Synced" ) );

    InterpSoundCheck = new QCheckBox( SoundGroup, "InterpSoundCheck" );
    InterpSoundCheck->setGeometry( QRect((int)(11 * wmult), (int)(127 * hmult),
                                  (int)(300 * wmult), (int)(31 * hmult)));
    InterpSoundCheck->setText( trUtf8( "Interpolated" ) );

    NoSampleCachingCheck = new QCheckBox( SoundGroup, "NoSampleCachingCheck" );
    NoSampleCachingCheck->setGeometry( QRect((int)(11 * wmult), (int)(159 * hmult),
                                  (int)(300 * wmult), (int)(31 * hmult)));
    NoSampleCachingCheck->setText( trUtf8( "No sample caching" ) );

    AltSampleDecodeCheck = new QCheckBox( SoundGroup, "AltSampleDecodeCheck" );
    AltSampleDecodeCheck->setGeometry( QRect((int)(11 * wmult), (int)(191 * hmult),
                                  (int)(300 * wmult), (int)(31 * hmult)));
    AltSampleDecodeCheck->setText( trUtf8( "Alt sample decoding" ) );

    NoMasterVolumeCheck = new QCheckBox( SoundGroup, "NoMasterVolumeCheck" );
    NoMasterVolumeCheck->setGeometry( QRect((int)(11 * wmult), (int)(223 * hmult),
                                  (int)(300 * wmult), (int)(31 * hmult)));
    NoMasterVolumeCheck->setText( trUtf8( "No master volume" ) );

    BufferCheck = new QCheckBox( SoundGroup, "BufferCheck" );
    BufferCheck->setGeometry( QRect((int)(11 * wmult), (int)(255 * hmult),
                                  (int)(300 * wmult), (int)(31 * hmult)));
    BufferCheck->setText( trUtf8( "Set buffer size" ) );

    BufferSizeSlider = new QSlider( SoundGroup, "BufferSizeSlider" );
    BufferSizeSlider->setGeometry( QRect( (int)(120 * wmult), (int)(290 * hmult),
                                     (int)(120 * wmult), (int)(31 * hmult) ) );
    BufferSizeSlider->setMinValue( 0 );
    BufferSizeSlider->setMaxValue( 32 );
    BufferSizeSlider->setOrientation( QSlider::Horizontal );

    BufferSizeLabel = new QLabel( SoundGroup, "BufferSizeLabel" );
    BufferSizeLabel->setGeometry( QRect( (int)(20 * wmult), (int)(290 * hmult),
                                    (int)(91 * wmult), (int)(31  * hmult)) );
    BufferSizeLabel->setText( trUtf8( "(bytes)" ) );

    BufferSizeValLabel = new QLabel( SoundGroup, "BufferSizeValLabel" );
    BufferSizeValLabel->setGeometry( QRect( (int)(250 * wmult), (int)(290 * hmult),
                                       (int)(100 * wmult), (int)(31 * hmult)));
    BufferSizeValLabel->setText( trUtf8( "128" ) );

    SoundSkipSlider = new QSlider( SoundGroup, "SoundSkipSlider" );
    SoundSkipSlider->setGeometry( QRect( (int)(180 * wmult), (int)(330 * hmult),
                                     (int)(120 * wmult), (int)(31 * hmult) ) );
    SoundSkipSlider->setMinValue( 0 );
    SoundSkipSlider->setMaxValue( 3 );
    SoundSkipSlider->setOrientation( QSlider::Horizontal );

    SoundSkipLabel = new QLabel( SoundGroup, "SoundSkipLabel" );
    SoundSkipLabel->setGeometry( QRect( (int)(20 * wmult), (int)(330 * hmult),
                                    (int)(151 * wmult), (int)(31  * hmult)) );
    SoundSkipLabel->setText( trUtf8( "sound skip" ) );

    SoundSkipValLabel = new QLabel( SoundGroup, "SoundSkipValLabel" );
    SoundSkipValLabel->setGeometry( QRect( (int)(310 * wmult), (int)(330 * hmult),
                                       (int)(40 * wmult), (int)(31 * hmult)));
    SoundSkipValLabel->setText( trUtf8( "0" ) );

    SoundQualitySlider = new QSlider( SoundGroup, "SoundQualitySlider" );
    SoundQualitySlider->setGeometry( QRect( (int)(180 * wmult), (int)(370 * hmult),
                                     (int)(120 * wmult), (int)(31 * hmult) ) );
    SoundQualitySlider->setMinValue( 0 );
    SoundQualitySlider->setMaxValue( 7 );
    SoundQualitySlider->setOrientation( QSlider::Horizontal );

    SoundQualityLabel = new QLabel( SoundGroup, "SoundQualityLabel" );
    SoundQualityLabel->setGeometry( QRect( (int)(20 * wmult), (int)(370 * hmult),
                                    (int)(151 * wmult), (int)(31  * hmult)) );
    SoundQualityLabel->setText( trUtf8( "Quality" ) );

    SoundQualityValLabel = new QLabel( SoundGroup, "SoundQualityValLabel" );
    SoundQualityValLabel->setGeometry( QRect( (int)(310 * wmult), (int)(370 * hmult),
                                       (int)(40 * wmult), (int)(31 * hmult)));
    SoundQualityValLabel->setText( trUtf8( "4" ) );

    SaveButton = new QPushButton( SettingsTab, "SaveButton" );
    SaveButton->setGeometry( QRect( (int)(628 * wmult), (int)(513 * hmult),
                                    (int)(151 * wmult), (int)(40 * hmult)));
    SaveButton->setText( trUtf8( "Save" ) );

    /*if(bSystem)
    {
        ListButton = new QPushButton( SettingsTab, "ListButton" );
        ListButton->setGeometry( QRect( (int)(28 * wmult), (int)(513 * hmult),
                                    (int)(250 * wmult), (int)(40 * hmult)));
        ListButton->setText( trUtf8( "Reload Game List" ) );
        connect(ListButton, SIGNAL(pressed()), this,
            SLOT(loadList()));
    }*/

    if(!bSystem)
    {
        ScreenTab = new QWidget( SnesTab, "ScreenTab" );

        ScreenPic = new QLabel( ScreenTab, "ScreenPic" );
        ScreenPic->setEnabled( TRUE );
        ScreenPic->setGeometry( QRect( 0, 0, screenwidth, screenheight ) );
        ScreenPic->setScaledContents( TRUE );
        SnesTab->insertTab( ScreenTab, trUtf8( "ScreenShot" ) );
    }
    else
    {
        SystemTab = new QWidget( SnesTab, "SystemTab" );
        SnesTab->insertTab( SystemTab, trUtf8( "System" ) );

        NoVerifyCheck = new QCheckBox( SystemTab, "NoVerifyCheck" );
        NoVerifyCheck->setGeometry( QRect( (int)(100 * wmult), (int)(215 * hmult),
                                   (int)(250 * wmult), (int)(31 * hmult)));
        NoVerifyCheck->setText( trUtf8( "Don't verify Rom's" ) );

        ListButton = new QPushButton( SystemTab, "ListButton" );
        ListButton->setGeometry( QRect( (int)(100 * wmult), (int)(250 * hmult),
                                    (int)(250 * wmult), (int)(40 * hmult)));
        ListButton->setText( trUtf8( "Reload Game List" ) );
        connect(ListButton, SIGNAL(pressed()), this,
            SLOT(loadList()));
    }

    if(!bSystem)
    {
        connect(DefaultCheck, SIGNAL(stateChanged(int)), this,
            SLOT(defaultUpdate(int)));
    }
    connect(SoundSkipSlider, SIGNAL(valueChanged(int)), SoundSkipValLabel,
            SLOT(setNum(int)));
    connect(SoundQualitySlider, SIGNAL(valueChanged(int)), SoundQualityValLabel,
            SLOT(setNum(int)));
    connect(BufferSizeSlider, SIGNAL(valueChanged(int)), this,
            SLOT(setBufferSize(int)));
    connect(SaveButton, SIGNAL(pressed()), this,
            SLOT(accept()));
    connect(BufferCheck, SIGNAL(stateChanged(int)), this,
            SLOT(bufferCheckUpdate(int)));
}

/*
 *  Destroys the object and frees any allocated resources
 */
SnesSettingsDlg::~SnesSettingsDlg()
{
    // no need to delete child widgets, Qt does it all for us
}

int SnesSettingsDlg::Show(SnesGameSettings *settings)
{
    game_settings = settings;
    TransCheck->setChecked(game_settings->transparency);
    SixteenCheck->setChecked(game_settings->sixteen);
    HiResCheck->setChecked(game_settings->hi_res);
    NoModeSwitchCheck->setChecked(game_settings->no_mode_switch);
    FullScreenCheck->setChecked(game_settings->full_screen);
    StretchCheck->setChecked(game_settings->stretch);
    NoSoundCheck->setChecked(game_settings->no_sound);
    StreoCheck->setChecked(game_settings->stereo);
    EnvxCheck->setChecked(game_settings->envx);
    ThreadSoundCheck->setChecked(game_settings->thread_sound);
    SyncSoundCheck->setChecked(game_settings->sound_sync);
    InterpSoundCheck->setChecked(game_settings->interpolated_sound);
    NoSampleCachingCheck->setChecked(game_settings->no_sample_caching);
    AltSampleDecodeCheck->setChecked(game_settings->alt_sample_decode);
    NoEchoCheck->setChecked(game_settings->no_echo);
    NoMasterVolumeCheck->setChecked(game_settings->no_master_volume);
    NoJoyCheck->setChecked(game_settings->no_joy);
    InterleavedCheck->setChecked(game_settings->interleaved);
    AltInterleavedCheck->setChecked(game_settings->alt_interleaved);
    HiRomCheck->setChecked(game_settings->hi_rom);
    LowRomCheck->setChecked(game_settings->low_rom);
    HeaderCheck->setChecked(game_settings->header);
    NoHeaderCheck->setChecked(game_settings->no_header);
    NtscCheck->setChecked(game_settings->ntsc);
    PalCheck->setChecked(game_settings->pal);
    LayeringCheck->setChecked(game_settings->layering);
    NoHdmaCheck->setChecked(game_settings->no_hdma);
    NoSpeedHacksCheck->setChecked(game_settings->no_speed_hacks);
    NoWindowsCheck->setChecked(game_settings->no_windows);
    BufferCheck->setChecked((game_settings->buffer_size == 0)?false:true);
    OptionsEdit->setText(game_settings->extra_options);
    SoundSkipValLabel->setNum(int(game_settings->sound_skip));
    SoundSkipSlider->setValue(game_settings->sound_skip);
    SoundQualityValLabel->setNum(int(game_settings->sound_quality));
    SoundQualitySlider->setValue(game_settings->sound_quality);
    BufferSizeValLabel->setNum(int(game_settings->buffer_size));
    BufferSizeSlider->setValue(game_settings->buffer_size);
    InterpolateBox->setCurrentItem(game_settings->interpolate);
    if(game_settings->buffer_size == 0)
    {
        BufferCheck->setChecked(false);
        BufferSizeValLabel->setEnabled(false);
        BufferSizeSlider->setEnabled(false);
        BufferSizeLabel->setEnabled(false);
    }
    else
    {
        BufferCheck->setChecked(true);
        BufferSizeValLabel->setEnabled(true);
        BufferSizeSlider->setEnabled(true);
        BufferSizeLabel->setEnabled(true);
    }
    if(!bSystem)
    {
        DefaultCheck->setChecked(game_settings->default_options);
    }
    show();
    if(exec())
    {
        SaveSettings();
        return SAVE_SETTINGS;
    }
    else
        return DONT_SAVE_SETTINGS;
}

void SnesSettingsDlg::loadList()
{
    SnesHandler::getHandler()->processGames(!NoVerifyCheck->isChecked());
}

void SnesSettingsDlg::SaveSettings()
{
    if(!bSystem)
        game_settings->default_options = DefaultCheck->isChecked();
    game_settings->transparency = TransCheck->isChecked();
    game_settings->sixteen = SixteenCheck->isChecked();
    game_settings->hi_res = HiResCheck->isChecked();
    game_settings->no_mode_switch = NoModeSwitchCheck->isChecked();
    game_settings->full_screen = FullScreenCheck->isChecked();
    game_settings->stretch = StretchCheck->isChecked();
    game_settings->no_sound = NoSoundCheck->isChecked();
    game_settings->stereo = StreoCheck->isChecked();
    game_settings->envx = EnvxCheck->isChecked();
    game_settings->thread_sound = ThreadSoundCheck->isChecked();
    game_settings->sound_sync = SyncSoundCheck->isChecked();
    game_settings->interpolated_sound = InterpSoundCheck->isChecked();
    game_settings->no_sample_caching = NoSampleCachingCheck->isChecked();
    game_settings->alt_sample_decode = AltSampleDecodeCheck->isChecked();
    game_settings->no_echo = NoEchoCheck->isChecked();
    game_settings->no_master_volume = NoMasterVolumeCheck->isChecked();
    game_settings->no_joy = NoJoyCheck->isChecked();
    game_settings->interleaved = InterleavedCheck->isChecked();
    game_settings->alt_interleaved = AltInterleavedCheck->isChecked();
    game_settings->hi_rom = HiRomCheck->isChecked();
    game_settings->low_rom = LowRomCheck->isChecked();
    game_settings->header = HeaderCheck->isChecked();
    game_settings->no_header = NoHeaderCheck->isChecked();
    game_settings->ntsc = NtscCheck->isChecked();
    game_settings->pal = PalCheck->isChecked();
    game_settings->layering = LayeringCheck->isChecked();
    game_settings->no_hdma = NoHdmaCheck->isChecked();
    game_settings->no_speed_hacks = NoSpeedHacksCheck->isChecked();
    game_settings->no_windows = NoWindowsCheck->isChecked();
    game_settings->extra_options = OptionsEdit->text();
    game_settings->sound_skip = SoundSkipSlider->value();
    game_settings->sound_quality = SoundQualitySlider->value();
    game_settings->interpolate = InterpolateBox->currentItem();
    if(BufferCheck->isChecked())
        game_settings->buffer_size = BufferSizeSlider->value();
    else
        game_settings->buffer_size = 0;
}

void SnesSettingsDlg::defaultUpdate(int state)
{
    SettingsFrame->setEnabled(!state);
    game_settings->default_options = state;
}

void SnesSettingsDlg::bufferCheckUpdate(int state)
{
    if(!state)
    {
        BufferSizeValLabel->setNum(int(0));
        BufferSizeSlider->setValue(0);
    }
    BufferSizeValLabel->setEnabled(state);
    BufferSizeSlider->setEnabled(state);
    BufferSizeLabel->setEnabled(state);
}

void SnesSettingsDlg::setBufferSize(int buffer)
{
    BufferSizeValLabel->setNum(buffer * 128);
}

void SnesSettingsDlg::SetScreenshot(QString picfile)
{
    if(ScreenPic)
    {
        Screenshot.load(picfile);
        ScaleImageLabel(Screenshot,ScreenPic);
        ScreenPic->setPixmap(Screenshot);
    }
}

void SnesSettingsDlg::ScaleImageLabel(QPixmap &pixmap, QLabel *label)
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

