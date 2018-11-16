// The system syslog.h header file defines a series of LOG_XXX names
// that override all of the LogLevel_t enum names.
#include <syslog.h>

// Delete all of the LOG_XXX names that were just defined, allowing
// the compiler to see the LogLevel_t enum names again.  This allows
// the compiler to perform better checking whenever this enum is used
// as a function argument.
#undef	LOG_EMERG
#undef	LOG_ALERT
#undef	LOG_CRIT
#undef	LOG_ERR
#undef	LOG_WARNING
#undef	LOG_NOTICE
#undef	LOG_INFO
#undef	LOG_DEBUG
