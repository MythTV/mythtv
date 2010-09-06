#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include <QString>
#include <unistd.h>  // for pid_t

#include "mythexp.h"
#include "mythconfig.h"
#ifdef CONFIG_LIRC
#  include "lircevent.h"
#endif
#ifdef CONFIG_JOYSTICK_MENU
#  include "jsmenuevent.h"
#endif


typedef struct {
    bool                    ready_to_lock;
#ifdef CONFIG_LIRC
    LircEventLock          *lirc;
#endif
#ifdef CONFIG_JOYSTICK_MENU
    JoystickMenuEventLock  *jsmenu;
#endif
} MythSystemLocks;


#define MYTH_SYSTEM_DONT_BLOCK_LIRC          0x1 //< myth_system() flag to avoid blocking LIRC
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU 0x2 //< myth_system() flag to avoid blocking Joystick Menu
#define MYTH_SYSTEM_DONT_BLOCK_PARENT        0x4 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_RUN_BACKGROUND           0x8 //< myth_system() flag to run the child in the background

MPUBLIC unsigned int myth_system(const QString &command, int flags = 0,
                                 uint timeout = 0);

MPUBLIC void myth_system_pre_flags(int &flags, MythSystemLocks &locks);
MPUBLIC void myth_system_post_flags(int &flags, MythSystemLocks &locks);
#ifndef USING_MINGW
MPUBLIC pid_t myth_system_fork(const QString &command, uint &result);
MPUBLIC uint myth_system_wait(pid_t pid, uint timeout, bool background = false);
MPUBLIC uint myth_system_abort(pid_t pid);
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
