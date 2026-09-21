#include <QMetaType>
#include <QFile>

#include <algorithm>
#include <format>
#include <iostream>
#include <utility>
#include <regex>
#include <fstream>
#include <filesystem>

#include "libmythbase/mythlogging.h"

#include "config_ini.h"

ExternIniConfig::ExternIniConfig(const std::filesystem::path &filename)
    : m_settings(QString::fromStdString(filename.string()),
                 QSettings::IniFormat)
{
    load(filename);
}

void ExternIniConfig::load(const std::filesystem::path &filename)
{
    std::ifstream checkStream(filename);
    if (!checkStream.is_open())
    {
        std::cerr << std::format("Unable to read configuration file '{}': "
                                 "Permission denied or file missing.\n",
                                 filename.string());
        m_fatal = true;
        return;
    }
    checkStream.close(); // Clean up descriptor immediately

    /*
     * Parse the INI
     * Calling sync() pulls the mapping into cache.
     * Skip m_settings.allKeys() because its internal
     * fcntl() write-locking behaviors fail under Linux paths.
     */
    m_settings.sync();

    // Validate Format and Parse Status
    if (m_settings.status() == QSettings::FormatError)
    {
        std::cerr << std::format("Malformed syntax detected in '{}'. "
                                 "Check formatting tokens.\n",
                                 filename.string());
        m_fatal = true;
        return;
    }

    m_basePath = filename.parent_path();
    if (!loadChannels())
    {
        m_fatal = true;
    }
}

bool ExternIniConfig::hasTable(const std::string& table) const
{
    const QString requested = QString::fromStdString(table);

    for (const auto &section : m_settings.childGroups())
    {
        if (section.compare(requested, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

QString ExternIniConfig::getSectionName(std::string_view name) const
{
    const QString requested = QString::fromStdString(std::string(name));

    for (const auto &section : m_settings.childGroups())
    {
        if (section.compare(requested, Qt::CaseInsensitive) == 0)
            return section;
    }

    return requested;
}

std::optional<QString> ExternIniConfig::findKey(const QSettings &settings,
                                                std::string_view section,
                                                std::string_view key) const
{
    QString sectionName;
    QString requestedKey;

    if (key == "recfinished")
    {
        sectionName = "RECORDER";
        requestedKey = "cleanup";
    }
    else if (key == "newepisode")
    {
        sectionName = "TUNER";
        requestedKey = "newepisodecommand";
    }
    else if (key=="recstarting")
    {
        sectionName = "TUNER";
        requestedKey = "ondatastart";
    }
    else
    {
        sectionName = QString::fromStdString(std::string(section));
        requestedKey = QString::fromStdString(std::string(key));
    }

    const QString prefix = sectionName + '/';

    for (const auto &candidate : settings.allKeys())
    {
        if (!candidate.startsWith(prefix, Qt::CaseSensitive))
            continue;

        const QString candidateKey = candidate.mid(prefix.size());

        if (candidateKey.compare(requestedKey, Qt::CaseInsensitive) == 0)
            return candidate;
    }

    return std::nullopt;
}

static std::string normalizeKey(std::string_view key)
{
    std::string result(key);

    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::tolower(c));
                   });

    return result;
}

std::optional<std::string>
  ExternIniConfig::getRawValue(const QSettings &settings,
                               std::string_view section,
                               std::string_view key) const
{
    if (section == "VARIABLES")
    {
        std::lock_guard lock(m_mutex);

        const auto it = m_variables.find(normalizeKey(key));

        if (it != m_variables.end())
            return it->second;
    }

    const auto actualKey = findKey(settings, section, key);

    if (!actualKey)
        return std::nullopt;

    return settings.value(*actualKey).toString().toStdString();
}

std::optional<std::string>
  ExternIniConfig::getValue(const QSettings &settings,
                            std::string_view section,
                            std::string_view key) const
{
    const auto value = getRawValue(settings, section, key);

    if (!value)
        return std::nullopt;

    return expandVars(*value);
}

std::optional<std::string> ExternIniConfig::getValue(std::string_view table,
                                                     std::string_view key) const
{
    return getValue(m_settings, table, key);
}

void ExternIniConfig::updateVariable(std::string_view key,
                                     std::string_view value)
{
    std::lock_guard lock(m_mutex);

    m_variables[normalizeKey(key)] = std::string(value);
}

bool ExternIniConfig::variableExists(std::string_view name) const
{
    return getRawValue(m_settings, "VARIABLES", name).has_value();
}

std::string ExternIniConfig::expandVariable(std::string_view name) const
{
    const auto value = getRawValue(m_settings, "VARIABLES", name);

    if (!value)
        return {};

    return expandVars(*value);
}

std::string ExternIniConfig::expandVars(std::string value) const
{
    /*
     * ------------------------------------------------------------------
     * Conditional sections:
     *
     *     [[taskset -c %CORE1%,%CORE2%]]
     * or:
     *     [[taskset -c %CORE1%]][[, %CORE2%]][[, %CORE3%]]
     *
     * All variables inside the block must exist.
     *
     * If CORE1 and CORE2 exist:
     *
     *     taskset -c 5,6
     *
     * If either is missing:
     *
     *     <nothing>
     * ------------------------------------------------------------------
     */

    static const std::regex conditionalRegex(R"(\[\[([^\]]*)\]\])");
    static const std::regex variableRegex(R"(%([A-Za-z_][A-Za-z0-9_]*)%)");

    // Process conditional blocks until none remain.
    for (;;)
    {
        std::smatch match;

        if (!std::regex_search(value, match, conditionalRegex))
            break;

        const std::string contents = match[1].str();
        bool allDefined = true;

        for (std::sregex_iterator it(contents.begin(), contents.end(),
                                     variableRegex);
             it != std::sregex_iterator{};
             ++it)
        {
            const std::string name = (*it)[1].str();

            if (!variableExists(name))
            {
                allDefined = false;
                break;
            }
        }


        std::string replacement;

        if (allDefined)
            replacement = contents;

        value.replace(match.position(), match.length(), replacement);
    }

    /*
     * ------------------------------------------------------------------
     * Normal variables.
     *
     * Unknown variables are deliberately left untouched.
     *
     * This means:
     *
     *     %DEVICE%
     *
     * is expanded if DEVICE exists, while:
     *
     *     %URL%
     *
     * remains intact if URL isn't one of our configuration variables.
     * ------------------------------------------------------------------
     */

    std::size_t offset = 0;
    std::size_t substitutions = 0;

    constexpr std::size_t maxSubstitutions = 1000;

    for (;;)
    {
        std::smatch match;

        const auto begin = value.cbegin() +
                           static_cast<std::ptrdiff_t>(offset);

        if (!std::regex_search(begin, value.cend(), match, variableRegex))
            break;

        const std::size_t position = offset +
                                     static_cast<std::size_t>(match.position());

        const std::size_t length = static_cast<std::size_t>(match.length());

        const std::string name = match[1].str();

        if (!variableExists(name))
        {
            // Unknown variables remain intact.
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

bool ExternIniConfig::loadChannels(void)
{
    const auto filename = getValue(m_settings, "TUNER", "channels");

    if (!filename)
        return true;

    std::filesystem::path channelsPath = expandVars(*filename);

    if (channelsPath.is_relative())
        channelsPath = m_basePath / channelsPath;

    auto settings = std::make_unique<QSettings>
                    (QString::fromStdString(channelsPath.string()),
                     QSettings::IniFormat);

    if (settings->status() != QSettings::NoError)
    {
        std::cerr << std::format("Unable to load channel configuration '{}'",
                                 channelsPath.string())
                  << std::endl;

        return false;
    }

    m_channels.clear();

    /*
     * Each group in the channel INI is a channel.
     *
     * Examples:
     *
     *   [51]
     *   [ABCNL]
     *   [ACC]
     */
    for (const auto &section : settings->childGroups())
    {
        const auto info = makeChannelInfo(*settings, section);

        if (!info)
            continue;

        m_channels.push_back(ChannelEntry {
                .section = section.toStdString(),
                .info = *info
            } );
    }

    m_channelSettings = std::move(settings);

    return true;
}

std::optional<ChannelInfo>
  ExternIniConfig::makeChannelInfo(const QSettings &settings,
                                   const QString &section) const
{
    ChannelInfo info;

    /*
     * The section name is the default channel number.
     *
     * [51] -> number = "51"
     *
     * If this is actually a callsign section:
     *
     * [ABCNL]
     * CHANNUM=51
     *
     * then CHANNUM overrides the section name.
     */
    info.number = section.toStdString();

    if (const auto value = getValue(settings, section.toStdString(), "channum"))
        info.number = *value;

    if (const auto value = getValue(settings, section.toStdString(), "name"))
        info.name = *value;

    if (const auto value =
        getValue(settings, section.toStdString(), "callsign"))
    {
        info.callsign = *value;
    }

    if (const auto value = getValue(settings, section.toStdString(), "xmltvid"))
        info.xmltvid = *value;

    if (const auto value = getValue(settings, section.toStdString(), "icon"))
        info.icon = *value;

    return info;
}

std::optional<std::string>
  ExternIniConfig::getChannelValue(std::string_view key) const
{
    if (!m_channelSettings)
        return std::nullopt;

    if (key == "command")
        key = "tune";
    else if (key == "recstarting")
        key = "ondatastart";
    else if (key == "newepisode")
        key = "newepisodecommand";

    if (auto channel = getValue(m_settings, "VARIABLES", "callsign"))
    {
        if (const auto value = getValue(*m_channelSettings, *channel, key))
            return value;
    }

    if (auto channel = getValue(m_settings, "VARIABLES", "channum"))
    {
        if (const auto value = getValue(*m_channelSettings, *channel, key))
            return value;
    }

    return std::nullopt;
}

std::size_t ExternIniConfig::channelCount() const noexcept
{
    return m_channels.size();
}

std::optional<ChannelInfo> ExternIniConfig::firstChannel() const
{
    std::lock_guard lock(m_mutex);

    m_channelIterValid = false;

    if (m_channels.empty())
        return std::nullopt;

    m_channelIndex = 0;
    m_channelIterValid = true;

    return m_channels[m_channelIndex++].info;
}

std::optional<ChannelInfo> ExternIniConfig::nextChannel() const
{
    std::lock_guard lock(m_mutex);

    if (!m_channelIterValid || m_channelIndex >= m_channels.size())
    {
        m_channelIterValid = false;
        return std::nullopt;
    }

    return m_channels[m_channelIndex++].info;
}

std::string ExternIniConfig::asString(const QSettings& settings) const
{
    std::lock_guard lock(m_mutex);

    std::string result;

    // Export the Runtime Variables map into its own section
    if (!m_variables.empty())
    {
        result += "[VARIABLES]\n";
        for (const auto& [name, value] : m_variables)
        {
            result += name;
            result += '=';
            result += value;
            result += '\n';
        }
        result += '\n'; // Separate sections by a clean newline
    }

    // Export the standard QSettings layout (unchanged)
    for (const auto &section : settings.childGroups())
    {
        result += '[';
        result += section.toStdString();
        result += "]\n";

        const QString prefix = section + '/';

        for (const auto &key : settings.allKeys())
        {
            if (!key.startsWith(prefix))
                continue;

            result += key.mid(prefix.size()).toStdString();
            result += '=';
            result += settings.value(key).toString().toStdString();
            result += '\n';
        }

        result += '\n';
    }

    return result;
}

std::string ExternIniConfig::asString(void) const
{
    return asString(m_settings);
}
