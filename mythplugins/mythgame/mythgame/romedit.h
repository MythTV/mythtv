#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

#include <qstring.h>

#include <mythtv/mythcontext.h>
#include <mythtv/settings.h>

class GameEditDialog : public ConfigurationWizard
{
  public:
    GameEditDialog(const QString &romname);
};

#endif // ROMEDITDLG_H_
