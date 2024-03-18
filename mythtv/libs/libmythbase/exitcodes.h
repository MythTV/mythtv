#ifndef MYTH_EXIT_CODES_H
#define MYTH_EXIT_CODES_H

#include <cstdint>

// ALL statuses that are not to be mapped directly to an exit code *should*
// be > 128 so as to not show up as a commercial count on commflag runs.
// However, they *must* be <= 255 as exit codes only can be 8-bits.
// Additionally, the functionality of mythwelcome/welcomedialog.cpp depends on
// being able to use exit code as an 8-bit masked integer.

enum EXIT_CODES : std::uint8_t {
    GENERIC_EXIT_OK                   =   0, ///< Exited with no error
    GENERIC_EXIT_NOT_OK               = 128, ///< Exited with error
    GENERIC_EXIT_CMD_NOT_FOUND        = 129, ///< Command not found
    GENERIC_EXIT_NO_MYTHCONTEXT       = 130, ///< No MythContext available
    GENERIC_EXIT_NO_THEME             = 131, ///< No Theme available
    GENERIC_EXIT_INVALID_CMDLINE      = 132, ///< Command line parse error
    GENERIC_EXIT_DB_OUTOFDATE         = 133, ///< Database needs upgrade
    GENERIC_EXIT_DB_ERROR             = 134, ///< Database error
    GENERIC_EXIT_SOCKET_ERROR         = 135, ///< Socket error
    GENERIC_EXIT_PERMISSIONS_ERROR    = 136, ///< File permissions error
    GENERIC_EXIT_CONNECT_ERROR        = 137, ///< Can't connect to master backend
    GENERIC_EXIT_SETUP_ERROR          = 138, ///< Incorrectly setup system
    GENERIC_EXIT_INVALID_TIME         = 139, ///< Invalid time
    GENERIC_EXIT_KILLED               = 140, ///< Process killed or stopped
    GENERIC_EXIT_TIMEOUT              = 141, ///< Process timed out
    GENERIC_EXIT_RUNNING              = 142, ///< Process is running
    GENERIC_EXIT_PIPE_FAILURE         = 143, ///< Error creating I/O pipes
    GENERIC_EXIT_NO_HANDLER           = 144, ///< No MythSystemLegacy Handler
    GENERIC_EXIT_DAEMONIZING_ERROR    = 145, ///< Error daemonizing or execl
    GENERIC_EXIT_NO_RECORDING_DATA    = 146, ///< No program/recording data
    GENERIC_EXIT_REMOTE_FILE          = 147, ///< Can't transcode a remote file
    GENERIC_EXIT_RESTART              = 148, ///< Need to restart transcoding
    GENERIC_EXIT_WRITE_FRAME_ERROR    = 149, ///< Frame write error
    GENERIC_EXIT_DEADLOCK             = 150, ///< Transcode deadlock detected
    GENERIC_EXIT_IN_USE               = 151, ///< Recording in use, can't flag
    GENERIC_EXIT_START                = 152, ///< MythSystemLegacy process starting
    GENERIC_EXIT_DB_NOTIMEZONE        = 153, ///< Missing DB time zone support
};

#endif // MYTH_EXIT_CODES_H
