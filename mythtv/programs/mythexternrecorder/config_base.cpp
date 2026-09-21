#include "config_base.h"

std::string ExternConfig::getValue(std::string_view table,
                                   std::string_view key,
                                   const std::string& defaultValue) const
{
    std::optional<std::string> value = getValue(table, key);
    if (value)
        return *value;

    return defaultValue;
}
