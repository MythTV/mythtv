/*
	main.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>

#include <mfdclient/mfdinterface.h>

#include "mfedialog.h"

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runMfe(void)
{
    MfdInterface *mfd_interface = new MfdInterface();
    MfeDialog *mfe_dialog = new MfeDialog(
                                            gContext->GetMainWindow(),
                                            "mfe_dialog",
                                            "mfe-",
                                             mfd_interface
                                         );
    mfe_dialog->exec();
    delete mfe_dialog;
    delete mfd_interface;
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mfe", libversion, 
                                    MYTH_BINARY_VERSION))
    {
        return -1;
    }
    return 0;
}

int mythplugin_run(void)
{
    runMfe();
    return 0;
}

int mythplugin_config(void)
{
    return 0;
}

