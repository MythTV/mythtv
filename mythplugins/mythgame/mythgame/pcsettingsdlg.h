#ifndef PCSETTINGSDLG_H
#define PCSETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>
#include "pcrominfo.h"

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

class PCSettingsDlg : public QDialog
{
    Q_OBJECT

public:
    PCSettingsDlg(QWidget* parent = 0,
        const char* name = 0, bool modal = FALSE,
        bool system = false,WFlags fl = 0 );
    ~PCSettingsDlg();

    QTabWidget* PCTab;
    QWidget* SettingsTab;
    QPushButton* ListButton;
    QFrame* SettingsFrame;
    QLabel* NameLabel;
    QLabel* NameValLabel;
    QLabel* CommandLabel;
    QLabel* CommandValLabel;
    QLabel* GenreLabel;
    QLabel* GenreValLabel;
    QLabel* YearLabel;
    QLabel* YearValLabel;
    QLabel* ScreenPic;

    void SetScreenshot(QString picfile);
    void Show(PCRomInfo*);

protected slots:

    void loadList();

protected:

    void ScaleImageLabel(QPixmap &pixmap, QLabel *label);
    bool bSystem;
    QPixmap Screenshot;

private:
    float wmult, hmult;
};

#endif // PCSETTINGSDLG_H
