#ifndef MYTHPLUGINAPI_H_
#define MYTHPLUGINAPI_H_

#include "libmythbase/mythpluginexport.h"
#include "libmythbase/mythplugin.h" // for MythPluginType

extern "C"
{
    MPLUGIN_PUBLIC int mythplugin_init(const char *libversion);
    MPLUGIN_PUBLIC int mythplugin_run();
    MPLUGIN_PUBLIC int mythplugin_config();
    MPLUGIN_PUBLIC MythPluginType mythplugin_type();
    MPLUGIN_PUBLIC void mythplugin_destroy();

}

#endif // MYTHPLUGINAPI_H_
