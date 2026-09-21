#include <cctype>
#include <chrono>
#include <charconv>
#include <optional>
#include <array>
#include <iostream>
#include <sstream>
#include <strings.h>
#include <string_view>
#include <format>
#include <regex>
#include <stdexcept>
#include <iterator>

#include "config_toml.h"

#include "libmythbase/mythlogging.h"

ExternTomlConfig::ExternTomlConfig(const std::filesystem::path& filename)
{
    load(filename);
}

void ExternTomlConfig::load(const std::filesystem::path& filename)
{
    m_root = parseFile(filename, "main");

    if (m_root.empty())
    {
        m_fatal = true;
        return;
    }

    m_section["VARIABLES"] = getSectionName(m_root, "VARIABLES");
    auto& var_section = m_section.at("VARIABLES");
    // Make sure a VARIABLES section exists
    m_root.emplace<toml::table>(var_section);

    m_basePath = filename.parent_path();

    if (!loadIncludes())
    {
        m_fatal = true;
        return;
    }

    m_section["RECORDER"] = getSectionName(m_root, "RECORDER");
    // A RECORDER section must exist
    auto& recSection = m_section.at("RECORDER");
    if (!m_root.contains(recSection) || !m_root[recSection].is_table())
    {
        std::cerr << std::format("Configuration Error: "
                                 "Missing required section [RECORDER] "
                                 "(Note: Names are case-sensitive).\n");
        m_fatal = true;
    }

    m_section["TUNER"] = getSectionName(m_root, "TUNER");
    if (!loadChannels())
    {
        m_fatal = true;
        return;
    }

    m_section["TOUCH"] = getSectionName(m_root, "TOUCH");
    if (!loadTouch())
    {
        m_fatal = true;
        return;
    }
}

std::string ExternTomlConfig::getSectionName(const toml::table& tbl,
                                               std::string_view name) const
{
    for (const auto& [k, n] : tbl)
    {
        // Extract string_view from toml::key using .str()
        std::string_view keyView = k.str();

        if (keyView.size() == name.size() &&
            strncasecmp(keyView.data(), name.data(), name.size()) == 0)
        {
            return std::string(keyView);
        }
    }
    return std::string(name);
}

toml::table ExternTomlConfig::parseFile(const std::filesystem::path& filepath,
                                        const std::string& where)
{
    try
    {
        toml::table tbl = toml::parse_file(filepath.string());

        LOG(VB_RECORD, LOG_WARNING, QString("Loaded '%1'\n%2")
            .arg(QString::fromStdString(filepath.string()))
            .arg(QString::fromStdString(tableAsString(tbl))));

        return tbl;
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "\n=========================================\n"
                  << "Critical toml configuration error\n"
                  << "=========================================\n"
                  << "The " << where << " file '" << filepath
                  << "' is not valid TOML.\n"
                  << "Details: " << err.description() << "\n"
                  << "Location: Line " << err.source().begin.line
                  << ", Column " << err.source().begin.column << "\n"
                  << "=========================================\n\n";
        return toml::table{};
    }
}

bool ExternTomlConfig::loadIncludes(void)
{
    if (const auto* includes = m_root["INCLUDE"].as_table())
    {
        if (const auto* files = includes->get_as<toml::array>("files"))
        {
            for (const auto& node : *files)
            {
                auto filename = node.value<std::string>();

                if (!filename)
                {
                    std::cerr << "'INCLUDE.files' entries must be strings"
                              << std::endl;
                    m_fatal = true;
                    return false;
                }

                /*
                 * Include filenames themselves are allowed to use
                 * variables, e.g.:
                 *
                 *     magewell-${SOURCE}.toml
                 */
                std::string expandedFilename = expandVars(std::move(*filename));
                std::filesystem::path includePath = expandedFilename;

                if (includePath.is_relative())
                    includePath = m_basePath / includePath;

                const toml::table included = parseFile(includePath, "included");

                // Only these sections are expected in modular files.
                for (const auto& section :
                         {
                             "RECORDER",
                             "TUNER",
                             "TOUCH",
                             "SCANNER"
                         })
                {
                    m_section[section] = getSectionName(included, section);
                    const auto& sectionName = m_section.at(section);
                    if (const auto* table = included[sectionName].as_table())
                    {
                        if (auto* existing = m_root[sectionName].as_table())
                            mergeTable(*table, *existing);
                        else
                            m_root.insert(sectionName, *table);
                    }
                }
            }
        }
        else
        {
            std::cerr << "'files' missing from include section" << std::endl;
        }
    }
    return true;
}

bool ExternTomlConfig::loadChannels(void)
{
    const auto& section = m_section.at("TUNER");
    const auto* tuner = m_root[section].as_table();

    if (!tuner)
        return true;

    const auto* channelsNode = tuner->get("channels");

    if (!channelsNode)
        return true;

    auto filename = channelsNode->value<std::string>();

    if (!filename)
    {
        std::cerr << "TUNER/channels must be a string" << std::endl;
        return false;
    }

    std::string expandedFilename = expandVars(std::move(*filename));
    std::filesystem::path channelsPath = expandedFilename;

    if (channelsPath.is_relative())
        channelsPath = m_basePath / channelsPath;

    // The channels TOML becomes the [channels] table.
    toml::table channels = parseFile(channelsPath, "channels");
    m_numChannels = channels.size();

    m_root.insert_or_assign("channels",
                            std::move(channels));

    return true;
}

static std::optional<std::chrono::seconds>
  parseDuration(const toml::node& node, std::string_view name)
{
    if (const auto value = node.value<int>())
    {
        if (*value < 0)
        {
            std::cerr << std::format("{} cannot be negative\n", name);
            return std::nullopt;
        }

        return std::chrono::seconds(*value);
    }

    const auto value = node.value<std::string_view>();
    if (!value)
    {
        std::cerr << std::format(
            "{} must be an integer or duration string\n", name);
        return std::nullopt;
    }

    std::array<int, 3> parts {};
    size_t count = 0;
    std::string_view remaining = *value;

    while (!remaining.empty() && count < parts.size())
    {
        // Don't allow an empty component.
        if (remaining.front() == ':')
            break;

        const char *begin = remaining.data();
        const char *end = begin + remaining.size();

        int number {};
        const auto [ptr, ec] = std::from_chars(begin, end, number);

        if (ec != std::errc{} || ptr == begin || number < 0)
            break;

        parts[count++] = number;
        remaining.remove_prefix(static_cast<size_t>(ptr - begin));

        if (!remaining.empty())
        {
            if (remaining.front() != ':')
                break;

            remaining.remove_prefix(1);
        }
    }

    if (!remaining.empty() || count == 0)
    {
        std::cerr << std::format("Invalid duration '{}' for {}\n",
                                 *value, name);
        return std::nullopt;
    }

    int total_seconds = 0;

    switch (count)
    {
        case 1:
          total_seconds = parts[0];
          break;

        case 2:
          if (parts[1] >= 60)
              return std::nullopt;

          total_seconds = parts[0] * 60 + parts[1];
          break;

        case 3:
          if (parts[1] >= 60 || parts[2] >= 60)
              return std::nullopt;

          total_seconds = parts[0] * 3600 +
                          parts[1] * 60 +
                          parts[2];
          break;
    }

    return std::chrono::seconds(total_seconds);
}

static TouchConfig parseTouchConfig(const std::string& name,
                                    const toml::table& table)
{
    TouchConfig config;

    const auto* delay = table.get("delay");
    if (delay)
    {
        config.delay = parseDuration(*delay,
                                     std::format("[TOUCH.{}].delay",
                                                 name));
        if (!config.delay)
            return config;
    }

    const auto* frequency = table.get("frequency");
    if (frequency)
    {
        config.frequency = parseDuration(*frequency,
                                         std::format("[TOUCH.{}].frequency",
                                                     name));
        if (!config.frequency)
            return config;
    }

    const auto command = table["command"].value<std::string>();
    if (!command)
    {
        std::cerr << std::format("[TOUCH.{}].command must be a string", name)
                  << std::endl;
        return {};
    }
    config.command = *command;

    const auto damaged = table["damaged_on_failure"].value<bool>();
    if (!damaged)
        config.damagedOnFailure = false;
    else
        config.damagedOnFailure = *damaged;

    if (auto level = table["log_level"].value<std::string>())
        config.logLevel = *level;
    else
        config.logLevel = "INFO";

    return config;
}

bool ExternTomlConfig::loadTouch(void)
{
    const auto& section = m_section.at("TOUCH");
    auto* touch = m_root[section].as_table();

    if (!touch)
        return true;

    for (const auto& [key, node] : *touch)
    {
        if (!node.as_table())
        {
            std::cerr << std::format("[TOUCH.{}] must be a table", key.str())
                      << std::endl;
            return false;
        }

        const toml::table table = *(node.as_table());
        const std::string name = std::string(key.str());

        m_touch.insert_or_assign(name, parseTouchConfig(name, table));
    }

    return true;
}

std::size_t ExternTomlConfig::channelCount() const noexcept
{
    return m_numChannels;
}

void ExternTomlConfig::mergeTable(const toml::table& source,
                                  toml::table& destination)
{
    for (const auto& [key, value] : source)
    {
        const std::string keyString = std::string(key.str());

        if (const auto* sourceTable = value.as_table())
        {
            if (auto* destinationNode = destination.get(keyString))
            {
                if (auto* destinationTable = destinationNode->as_table())
                {
                    mergeTable(*sourceTable, *destinationTable);
                    continue;
                }
            }

            destination.insert_or_assign(keyString, *sourceTable);
        }
        else
            destination.insert_or_assign(keyString, value);
    }
}

bool ExternTomlConfig::variableExists(std::string_view varName) const
{
    const auto& sectionName = m_section.at("VARIABLES");
    const auto* variables = m_root[sectionName].as_table();
    if (!variables)
        return false;

    for (const auto& [k, n] : *variables)
    {
        std::string_view keyView = k.str();
        if (keyView.size() == varName.size() &&
            strncasecmp(keyView.data(), varName.data(), varName.size()) == 0)
        {
            return true;
        }
    }
    return false;
}

std::string ExternTomlConfig::expandVariable(std::string_view varName) const
{
    const auto& varSection = m_section.at("VARIABLES");
    const auto* variables = m_root[varSection].as_table();
    if (!variables)
        return {};

    const toml::node* node = nullptr;
    for (const auto& [k, n] : *variables)
    {
        std::string_view keyView = k.str();
        if (keyView.size() == varName.size() &&
            strncasecmp(keyView.data(), varName.data(), varName.size()) == 0)
        {
            node = &n;
            break;
        }
    }

    if (!node)
        return {};

    if (auto* vs = node->as_string())
        return expandVars(std::string(vs->get()));
    else if (const auto* vi = node->as_integer())
        return std::to_string(vi->get());
    else if (const auto* vb = node->as_boolean())
        return vb->get() ? "true" : "false";
    else
    {
        std::cerr << std::format("Configuration variable '{}' "
                                 "is not a scalar value", varName)
                  << std::endl;
        return {};
    }
}

std::string ExternTomlConfig::expandVars(std::string value) const
{
    /*
     * ------------------------------------------------------------------
     * Regular Expression Definitions
     *
     * conditionalRegex:   Matches [? conditional block content ?]
     *                     Group 1 captures everything inside the markers.
     * variableRegex:      Matches {VARIABLE_NAME}
     *                     Group 1 captures the alphanumeric variable name.
     * ------------------------------------------------------------------
     */
    static const std::regex conditionalRegex(R"(\[\?([\s\S]*?)\?\])");
    static const std::regex variableRegex(R"(\{([A-Za-z_][A-Za-z0-9_]*)\})");


    /*
     * ------------------------------------------------------------------
     * Conditional sections
     *
     * Examples:
     *     [?taskset -c {CORE1},{CORE2}?]
     *     [?taskset -c {CORE1}?][?,{CORE2}?][?,{CORE3}?]
     *
     * All variables inside the block must exist (be defined).
     * If all exist, the variables are expanded and the block remains.
     * If any variable is missing, the entire block is stripped.
     * ------------------------------------------------------------------
     */
    for (;;)
    {
        std::smatch match;

        if (!std::regex_search(value, match, conditionalRegex))
            break;

        // The text inside the [? and ?]
        std::string contents = match[1].str();
        bool allDefined = true;

        // Scan the inner block contents for standard variables
        for (std::sregex_iterator it(contents.begin(),
                                     contents.end(), variableRegex);
             it != std::sregex_iterator{};
             ++it)
        {
            // Capture the variable name group
            const std::string name = (*it)[1].str();

            if (!variableExists(name))
            {
                allDefined = false;
                break;
            }
        }

        std::string replacement;

        if (allDefined)
        {
            // Expand the variables inside the block before merging back
            std::smatch innerMatch;
            while (std::regex_search(contents, innerMatch, variableRegex))
            {
                const std::string name = innerMatch[1].str(); // Pull inner name
                contents.replace(innerMatch.position(),
                                 innerMatch.length(), expandVariable(name));
            }
            replacement = contents;
        }

        // Replace the outer [? ... ?] block with the resolved text or
        // empty string
        value.replace(match.position(), match.length(), replacement);
    }

    /*
     * ------------------------------------------------------------------
     * Normal variables
     *
     * Unknown variables are left untouched.
     * ------------------------------------------------------------------
     */
    std::size_t offset = 0;
    std::size_t substitutions = 0;
    constexpr std::size_t maxSubstitutions = 1000;

    for (;;)
    {
        std::smatch match;
        const auto begin = value.cbegin() + static_cast<std::ptrdiff_t>(offset);

        if (!std::regex_search(begin, value.cend(), match, variableRegex))
            break;

        const std::size_t position = offset +
                                     static_cast<std::size_t>(match.position());
        const std::size_t length = static_cast<std::size_t>(match.length());
        const std::string name = match[1].str();

        if (!variableExists(name))
        {
            // Unknown variables remain completely intact.
            offset = position + length;
            continue;
        }

        const std::string replacement = expandVariable(name);

        if (++substitutions > maxSubstitutions)
        {
            std::cerr << "Too many variable substitutions; "
                      << "possible circular reference" << std::endl;
            m_fatal = true;
            return {};
        }

        value.replace(position, length, replacement);
        offset = position + replacement.length();
    }

    return value;
}

const toml::table ExternTomlConfig::variables(void) const
{
    const auto& sectionName = m_section.at("VARIABLES");
    const auto* variables = m_root[sectionName].as_table();
    if (!variables)
        return {};
    return *variables;
}

// -------------------------------------------------------------------------
// Accessors
// -------------------------------------------------------------------------
bool ExternTomlConfig::hasTable(const std::string& table) const
{
    const auto& sectionName = m_section.at(table);
    const auto* node = m_root.get(sectionName);
    return node && node->is_table();
}

void ExternTomlConfig::updateVariable(std::string_view key,
                                      std::string_view value)
{
    const auto& sectionName = m_section.at("VARIABLES");
    auto* variables = m_root[sectionName].as_table();
    if (!variables)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    variables->insert_or_assign(key, std::string(value));
}

std::optional<std::string>
  ExternTomlConfig::getValue(const toml::table& tbl,
                             std::string_view keyName) const
{
    const toml::node* val = nullptr;
    for (const auto& [k, n] : tbl)
    {
        std::string_view keyView = k.str();
        if (keyView.size() == keyName.size() &&
            strncasecmp(keyView.data(), keyName.data(), keyName.size()) == 0)
        {
            val = &n;
            break;
        }
    }

    if (!val)
        return std::nullopt;

    if (const auto* vs = val->as_string())
        return expandVars(std::string(vs->get()));
    if (const auto* vi = val->as_integer())
        return std::to_string(vi->get());
    if (const auto* vb = val->as_boolean())
        return vb->get() ? "true" : "false";

    return std::nullopt;
}

std::optional<std::string>
  ExternTomlConfig::getValue(std::string_view tableName,
                             std::string_view keyName) const
{
    const auto& section = m_section.at(std::string(tableName));
    const auto* targetTable = m_root[section].as_table();
    if (!targetTable)
        return std::nullopt;

    return getValue(*targetTable, keyName);
}

std::optional<std::string>
  ExternTomlConfig::getChannelValue(std::string_view key) const
{
    const auto* channels = m_root["channels"].as_table();

    if (!channels)
        return std::nullopt;

    const auto findInChannel =
        [&](std::string_view channel) -> std::optional<std::string>
        {
            const auto* node = channels->get(channel);
            if (!node)
                return std::nullopt;

            const auto* table = node->as_table();
            if (!table)
                return std::nullopt;

            return getValue(*table, key);
        };


    if (auto channel = getValue("VARIABLES", "callsign"))
    {
        if (const auto value = findInChannel(*channel))
            return value;
    }

    if (auto channel = getValue("VARIABLES", "channum"))
    {
        if (const auto value = findInChannel(*channel))
            return value;
    }

    return std::nullopt;
}

std::string ExternTomlConfig::tableAsString(const toml::table& tbl)
{
    std::stringstream ss;
    ss << tbl;
    std::string tomlstr = ss.str();
    return tomlstr;
}

std::string ExternTomlConfig::asString(void) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return tableAsString(m_root);
}

ChannelInfo
  ExternTomlConfig::channelInfo(const toml::table::const_iterator& iter) const
{
    const auto& [key, node] = *iter;

    ChannelInfo channel;
    channel.number = std::string(key);

    if (const auto* table = node.as_table())
    {
        if (const auto* value = table->get("name"))
            channel.name = value->value_or<std::string>("");

        if (const auto* value = table->get("callsign"))
            channel.callsign = value->value_or<std::string>("");

        if (const auto* value = table->get("xmltvid"))
            channel.xmltvid = value->value_or<std::string>("");

        if (const auto* value = table->get("icon"))
            channel.icon = value->value_or<std::string>("");
    }

    return channel;
}

std::optional<ChannelInfo> ExternTomlConfig::firstChannel(void) const
{
    std::lock_guard lock(m_mutex);

    m_channelIterValid = false;

    const auto* channels = m_root["channels"].as_table();

    if (!channels || channels->empty())
        return std::nullopt;

    m_channelIter = channels->cbegin();
    m_channelIterValid = true;

    return channelInfo(m_channelIter++);
}

std::optional<ChannelInfo> ExternTomlConfig::nextChannel(void) const
{
    std::lock_guard lock(m_mutex);

    if (!m_channelIterValid)
        return std::nullopt;

    const auto* channels = m_root["channels"].as_table();

    if (!channels || m_channelIter == channels->cend())
    {
        m_channelIterValid = false;
        return std::nullopt;
    }

    return channelInfo(m_channelIter++);
}
