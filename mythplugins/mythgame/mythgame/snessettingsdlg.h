#ifndef SNESSETTINGSDLG_H
#define SNESSETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>
#include "snesrominfo.h"

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTabWidget;
class QWidget;
class MythContext;

class SnesSettingsDlg : public QDialog
{
    Q_OBJECT

public:
    SnesSettingsDlg(QWidget* parent = 0,
        const char* name = 0, bool modal = FALSE,
        bool system = false,WFlags fl = 0 );
    ~SnesSettingsDlg();

    QTabWidget* SnesTab;
    QWidget* SettingsTab;
    QPushButton* ListButton;
    QFrame* SettingsFrame;
    QGroupBox* DisplayGroup;
    QGroupBox* MiscGroup;
    QGroupBox* SoundGroup;
    QCheckBox* TransCheck;
    QCheckBox* SixteenCheck;
    QCheckBox* HiResCheck;
    QCheckBox* NoModeSwitchCheck;
    QCheckBox* FullScreenCheck;
    QCheckBox* StretchCheck;
    QCheckBox* NoSoundCheck;
    QCheckBox* StreoCheck;
    QCheckBox* EnvxCheck;
    QCheckBox* ThreadSoundCheck;
    QCheckBox* SyncSoundCheck;
    QCheckBox* InterpSoundCheck;
    QCheckBox* NoSampleCachingCheck;
    QCheckBox* AltSampleDecodeCheck;
    QCheckBox* NoEchoCheck;
    QCheckBox* NoMasterVolumeCheck;
    QCheckBox* NoJoyCheck;
    QCheckBox* InterleavedCheck;
    QCheckBox* AltInterleavedCheck;
    QCheckBox* HiRomCheck;
    QCheckBox* LowRomCheck;
    QCheckBox* HeaderCheck;
    QCheckBox* NoHeaderCheck;
    QCheckBox* NtscCheck;
    QCheckBox* PalCheck;
    QCheckBox* LayeringCheck;
    QCheckBox* NoHdmaCheck;
    QCheckBox* NoSpeedHacksCheck;
    QCheckBox* NoWindowsCheck;
    QCheckBox* BufferCheck;
    QCheckBox* NoVerifyCheck;
    QLineEdit* OptionsEdit;
    QLabel* OptionsLabel;
    QLabel* InterpolateLabel;
    QComboBox* InterpolateBox;
    QLabel* SoundSkipLabel;
    QLabel* SoundSkipValLabel;
    QSlider* SoundSkipSlider;
    QLabel* SoundQualityLabel;
    QLabel* SoundQualityValLabel;
    QSlider* SoundQualitySlider;
    QLabel* BufferSizeLabel;
    QLabel* BufferSizeValLabel;
    QSlider* BufferSizeSlider;
    QPushButton* SaveButton;
    QCheckBox* DefaultCheck;
    QWidget* ScreenTab;
    QLabel* ScreenPic;
    QWidget* SystemTab;

    void SetScreenshot(QString picfile);
    int Show(SnesGameSettings *settings);

protected slots:

    void defaultUpdate(int);
    void loadList();
    void setBufferSize(int);
    void bufferCheckUpdate(int);

protected:

    void SaveSettings();
    void ScaleImageLabel(QPixmap &pixmap, QLabel *label);
    SnesGameSettings *game_settings;
    bool bSystem;
    QPixmap Screenshot;

private:
    float wmult, hmult;
};

#endif // SNESSETTINGSDLG_H
