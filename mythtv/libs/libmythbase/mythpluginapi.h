#ifndef MYTHPLUGINAPI_H_
#define MYTHPLUGINAPI_H_

#include "mythbaseexp.h"
#include "mythplugin.h" // for MythPluginType

extern "C" {
    MBASE_PUBLIC int mythplugin_init(const char *libversion);
    MBASE_PUBLIC int mythplugin_run();
    MBASE_PUBLIC int mythplugin_config();
    MBASE_PUBLIC MythPluginType mythplugin_type();
    MBASE_PUBLIC void mythplugin_destroy();
}

#endif // MYTHPLUGINAPI_H_
