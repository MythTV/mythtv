#ifndef MYTHPLUGINAPI_H_
#define MYTHPLUGINAPI_H_

#include "mythexp.h"
#include "mythplugin.h" // for MythPluginType

extern "C" {
    MPUBLIC int mythplugin_init(const char *);
    MPUBLIC int mythplugin_run();
    MPUBLIC int mythplugin_config();
    MPUBLIC MythPluginType mythplugin_type();
    MPUBLIC void mythplugin_destroy();
}

#endif // MYTHPLUGINAPI_H_
