#ifndef ADVANCEDOPTIONS_H_
#define ADVANCEDOPTIONS_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>

class AdvancedOptions : public MythThemedDialog
{

  Q_OBJECT

  public:
    AdvancedOptions(MythMainWindow *parent, QString window_name,
                    QString theme_filename, const char *name = 0);

    ~AdvancedOptions(void);

    void keyPressEvent(QKeyEvent *e);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void toggleCreateISO(bool state) { bCreateISO = state; };
    void toggleDoBurn(bool state) { bDoBurn = state; };
    void toggleEraseDvdRw(bool state) { bEraseDvdRw = state; };

  private:
    void wireUpTheme(void);
    void loadConfiguration(void);
    void saveConfiguration(void);

    bool            bCreateISO;
    bool            bDoBurn;
    bool            bEraseDvdRw;
    UICheckBoxType *createISO_check;
    UICheckBoxType *doBurn_check;
    UICheckBoxType *eraseDvdRw_check;

    UITextButtonType *ok_button;
    UITextButtonType *cancel_button;
};

#endif


