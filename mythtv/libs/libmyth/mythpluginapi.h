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
    MPUBLIC int mythplugin_setupMenu();
    MPUBLIC void mythplugin_drawMenu(QPainter *painter, int x, int y,
                                     int w, int h);
}

#endif // MYTHPLUGINAPI_H_
