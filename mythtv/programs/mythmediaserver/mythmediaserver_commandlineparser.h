// Qt
#include <QString>

// MythTV
#include "libmythbase/mythcommandlineparser.h"

class MythMediaServerCommandLineParser : public MythCommandLineParser
{
  public:
    MythMediaServerCommandLineParser(); 
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

