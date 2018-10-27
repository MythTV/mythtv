// -*- Mode: c++ -*-

#include <QString>

#include "mythcommandlineparser.h"

class MythBackendCommandLineParser : public MythCommandLineParser
{
  public:
    MythBackendCommandLineParser(); 
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};
