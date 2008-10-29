#ifndef MYTHSYSTEM_H_
#define MYTHSYSTEM_H_

#include <QString>

#include "mythexp.h"

#define MYTH_SYSTEM_DONT_BLOCK_LIRC          0x1 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU 0x2 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_PARENT        0x4 //< myth_system() flag to avoid blocking

MPUBLIC unsigned int myth_system(const QString &command, int flags = 0);

#endif

