// -*- Mode: c++ -*-

#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythBackendCommandLineParser : public MythCommandLineParser
{
  public:
    MythBackendCommandLineParser(); 
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};
