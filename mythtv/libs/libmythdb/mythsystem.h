#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include <QString>
#include <unistd.h>  // for pid_t

#include "mythexp.h"

typedef enum MythSystemMask {
    kMSNone                     = 0x00000000,
    kMSDontBlockInputDevs       = 0x00000001, //< avoid blocking LIRC & Joystick Menu
    kMSDontDisableDrawing       = 0x00000002, //< avoid disabling UI drawing
    kMSRunBackground            = 0x00000004, //< run child in the background
    kMSProcessEvents            = 0x00000008, //< process events while waiting
    kMSInUi                     = 0x00000010, //< the parent is in the UI
} MythSystemFlag;

MPUBLIC unsigned int myth_system(const QString &command, 
                                 uint flags = kMSNone,
                                 uint timeout = 0);

MPUBLIC void myth_system_pre_flags(uint &flags);
MPUBLIC void myth_system_post_flags(uint &flags);
#ifndef USING_MINGW
MPUBLIC pid_t myth_system_fork(const QString &command, uint &result);
MPUBLIC uint myth_system_wait(pid_t pid, uint timeout, bool background = false,
                              bool processEvents = true);
MPUBLIC uint myth_system_abort(pid_t pid);
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
