#ifndef MAMESETTINGSDLG_H
#define MAMESETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>
#include "mametypes.h"

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

class MameSettingsDlg : public QDialog
{ 
    Q_OBJECT

public:
    MameSettingsDlg(MythContext *context, QWidget* parent = 0, 
                    const char* name = 0, bool modal = FALSE, 
                    bool system = false, WFlags fl = 0 );
    ~MameSettingsDlg();

    QTabWidget* MameTab;
    QWidget* SettingsTab;
    QFrame* SettingsFrame;
    QGroupBox* DisplayGroup;
    QCheckBox* FullCheck;
    QCheckBox* SkipCheck;
    QCheckBox* LeftCheck;
    QCheckBox* RightCheck;
    QCheckBox* FlipXCheck;
    QCheckBox* FlipYCheck;
    QCheckBox* ExtraArtCheck;
    QCheckBox* ScanCheck;
    QCheckBox* ColorCheck;
    QLabel* ScaleLabel;
    QSlider* ScaleSlider;
    QLabel* ScaleValLabel;
    QGroupBox* VectorGroup;
    QCheckBox* AliasCheck;
    QCheckBox* TransCheck;
    QComboBox* ResBox;
    QLabel* BeamLabel;
    QSlider* BeamSlider;
    QLabel* BeamValLabel;
    QLabel* FlickerLabel;
    QSlider* FlickerSlider;
    QLabel* FlickerValLabel;
    QGroupBox* SoundGroup;
    QLabel* VolumeLabel;
    QCheckBox* SoundCheck;
    QCheckBox* SamplesCheck;
    QCheckBox* FakeCheck;
    QLabel* VolumeValLabel;
    QSlider* VolumeSlider;
    QGroupBox* MiscGroup;
    QLabel* OptionsLabel;
    QCheckBox* CheatCheck;
    QLineEdit* OptionsEdit;
    QGroupBox* InputGroup;
    QLabel* JoyLabel;
    QLabel* ResLabel;
    QCheckBox* WinkeyCheck;
    QCheckBox* MouseCheck;
    QCheckBox* GrabCheck;
    QComboBox* JoyBox;
    QCheckBox* JoyCheck;
    QPushButton* SaveButton;
    QPushButton* ListButton;
    QCheckBox* DefaultCheck;
    QWidget* ScreenTab;
    QLabel* ScreenPic;
    QWidget* flyer;
    QLabel* FlyerPic;
    QWidget* cabinet;
    QLabel* CabinetPic;

    int Show(GameSettings *settings, bool vector);
    void SetScreenshot(QString picfile);
    void SetCabinet(QString picfile);
    void SetFlyer(QString picfile);

  protected slots:

    void defaultUpdate(int);
    void setBeamLabel(int);
    void setFlickerLabel(int);
    void loadList();

  protected:

    void SaveSettings();
    void ScaleImageLabel(QPixmap &pixmap, QLabel *label);

    GameSettings *game_settings;
    QPixmap Screenshot;
    QPixmap Flyer;
    QPixmap Cabinet;
    bool bSystem;

private:
    MythContext *m_context;
    float wmult, hmult;
};

#endif // MAMESETTINGSDLG_H
