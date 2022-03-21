// -*- Mode: c++ -*-

#ifndef MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H
#define MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H

#include "libmythbase/mythcommandlineparser.h"

class MythCCExtractorCommandLineParser : public MythCommandLineParser
{
  public:
    MythCCExtractorCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

#endif // MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
