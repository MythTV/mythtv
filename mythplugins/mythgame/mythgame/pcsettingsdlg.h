#ifndef PCSETTINGSDLG_H
#define PCSETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>
#include "pcrominfo.h"

#include <mythtv/mythdialogs.h>

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

class PCSettingsDlg : public MythDialog
{
    Q_OBJECT
public:
    PCSettingsDlg(MythMainWindow* parent, const char* name = 0,
                  bool system = false);
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
