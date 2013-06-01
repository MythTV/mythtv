#ifndef __MYTH_EXIT_CODES__
#define __MYTH_EXIT_CODES__

// ALL statuses that are not to be mapped directly to an exit code *should*
// be > 128 so as to not show up as a commercial count on commflag runs.  
// However, they *must* be <= 255 as exit codes only can be 8-bits.
// Additionally, the functionality of mythwelcome/welcomedialog.cpp depends on 
// being able to use exit code as an 8-bit masked integer.

#define GENERIC_EXIT_OK                   0 ///< Exited with no error
#define GENERIC_EXIT_NOT_OK             128 ///< Exited with error
#define GENERIC_EXIT_CMD_NOT_FOUND      129 ///< Command not found
#define GENERIC_EXIT_NO_MYTHCONTEXT     130 ///< No MythContext available
#define GENERIC_EXIT_NO_THEME           131 ///< No Theme available
#define GENERIC_EXIT_INVALID_CMDLINE    132 ///< Command line parse error
#define GENERIC_EXIT_DB_OUTOFDATE       133 ///< Database needs upgrade
#define GENERIC_EXIT_DB_ERROR           134 ///< Database error
#define GENERIC_EXIT_SOCKET_ERROR       135 ///< Socket error
#define GENERIC_EXIT_PERMISSIONS_ERROR  136 ///< File permissions error
#define GENERIC_EXIT_CONNECT_ERROR      137 ///< Can't connect to master backend
#define GENERIC_EXIT_SETUP_ERROR        138 ///< Incorrectly setup system
#define GENERIC_EXIT_INVALID_TIME       139 ///< Invalid time
#define GENERIC_EXIT_KILLED             140 ///< Process killed or stopped
#define GENERIC_EXIT_TIMEOUT            141 ///< Process timed out
#define GENERIC_EXIT_RUNNING            142 ///< Process is running
#define GENERIC_EXIT_PIPE_FAILURE       143 ///< Error creating I/O pipes
#define GENERIC_EXIT_NO_HANDLER         144 ///< No MythSystemLegacy Handler
#define GENERIC_EXIT_DAEMONIZING_ERROR  145 ///< Error daemonizing or execl
#define GENERIC_EXIT_NO_RECORDING_DATA  146 ///< No program/recording data
#define GENERIC_EXIT_REMOTE_FILE        147 ///< Can't transcode a remote file
#define GENERIC_EXIT_RESTART            148 ///< Need to restart transcoding
#define GENERIC_EXIT_WRITE_FRAME_ERROR  149 ///< Frame write error
#define GENERIC_EXIT_DEADLOCK           150 ///< Transcode deadlock detected
#define GENERIC_EXIT_IN_USE             151 ///< Recording in use, can't flag
#define GENERIC_EXIT_START              152 ///< MythSystemLegacy process starting
#define GENERIC_EXIT_DB_NOTIMEZONE      153 ///< Missing DB time zone support

#endif // __MYTH_EXIT_CODES__
