#ifndef NESSETTINGSDLG_H
#define NESSETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>

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

class NesSettingsDlg : public QDialog
{ 
    Q_OBJECT

public:
    NesSettingsDlg(QWidget* parent = 0, 
        const char* name = 0, bool modal = FALSE, 
        WFlags fl = 0 );
    ~NesSettingsDlg();

    QTabWidget* NesTab;
    QWidget* SettingsTab;
    QPushButton* ListButton;
    
    int Show();

protected slots:

    void loadList();

private:
    float wmult, hmult;
};

#endif // NESSETTINGSDLG_H
