#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include <QString>

#include "mythexp.h"

#define MYTH_SYSTEM_DONT_BLOCK_LIRC          0x1 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU 0x2 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_PARENT        0x4 //< myth_system() flag to avoid blocking

MPUBLIC unsigned int myth_system(const QString &command, int flags = 0,
                                 uint timeout = 0);

#ifndef USING_MINGW
MPUBLIC void myth_system_pre_flags(int &flags, bool &ready_to_lock);
MPUBLIC void myth_system_post_flags(int &flags, bool &ready_to_lock);
MPUBLIC pid_t myth_system_fork(const QString &command, uint &result);
MPUBLIC uint myth_system_wait(pid_t pid, uint timeout);
MPUBLIC uint myth_system_abort(pid_t pid);
#endif

#endif

