#ifndef NESSETTINGSDLG_H
#define NESSETTINGSDLG_H

#include <qvariant.h>
#include <qdialog.h>
#include <qpixmap.h>

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

class NesSettingsDlg : public MythDialog
{ 
    Q_OBJECT

public:
    NesSettingsDlg(MythMainWindow *parent, 
                   const char* name = 0);
    ~NesSettingsDlg();

    QTabWidget* NesTab;
    QWidget* SettingsTab;
    QPushButton* ListButton;
    
protected slots:

    void loadList();
};

#endif // NESSETTINGSDLG_H
