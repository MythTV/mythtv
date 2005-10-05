#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include "libmyth/settings.h"

class BackendSettings: virtual public ConfigurationWizard {
public:
    BackendSettings();
};

class ClearCards;
class ClearChannels;
class ClearDialog: public VerticalConfigurationGroup,
                   public ConfigurationDialog
{
    Q_OBJECT
  public:
    ClearDialog();
    virtual ~ClearDialog() {}

    int exec();

    bool IsClearCardsSet() const;
    bool IsClearChannelsSet() const;

  public slots:
    void setHelpText(QString) {}

  private:
    ClearCards         *ccard;
    ClearChannels      *cchan;
    TransButtonSetting *okbtn;
};

#endif

