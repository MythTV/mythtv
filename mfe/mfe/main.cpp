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
    int screen_width = 0;
    int screen_height = 0;
    float screen_wmult = 0.0;
    float screen_hmult = 0.0;
    gContext->GetScreenSettings(screen_width, screen_wmult, screen_height, screen_hmult);
    MfdInterface *mfd_interface = new MfdInterface(screen_width, screen_height);
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

