#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ChannelInfo
{
    std::string number;
    std::string name;
    std::string callsign;
    std::string xmltvid;
    std::string icon;
};

struct TouchConfig
{
    std::optional<std::chrono::seconds> delay;
    std::optional<std::chrono::seconds> frequency;

    std::string command;
    bool damagedOnFailure{false};
    std::string logLevel;
};

class ExternConfig
{
  public:
    virtual ~ExternConfig() = default;

    bool failed(void) const { return m_fatal; }

    [[nodiscard]]
    virtual bool hasTable(const std::string& table) const = 0;

    virtual void updateVariable(std::string_view key,
                                std::string_view value) = 0;

    // Touch configuration.
    [[nodiscard]]
    virtual const std::map<std::string, TouchConfig>&
      touchConfigs(void) const noexcept = 0;

    [[nodiscard]]
    virtual std::optional<std::string> getValue(std::string_view table,
                                        std::string_view key) const = 0;

    [[nodiscard]]
    std::string getValue(std::string_view table, std::string_view key,
                         const std::string& defaultValue) const;

    [[nodiscard]]
    virtual std::string expandVars(std::string value) const = 0;

    /*
     * Look up a channel override using:
     *
     *   variables.CALLSIGN -> channels[CALLSIGN]
     *   variables.CHANNUM  -> channels[CHANNUM]
     */
    [[nodiscard]]
    virtual std::optional<std::string>
      getChannelValue(std::string_view key) const = 0;

    // Channel enumeration.
    virtual bool loadChannels(void) = 0;
    [[nodiscard]]
    virtual std::size_t channelCount(void) const noexcept = 0;
    [[nodiscard]]
    virtual std::optional<ChannelInfo> firstChannel(void) const = 0;
    [[nodiscard]]
    virtual std::optional<ChannelInfo> nextChannel(void) const = 0;

    virtual std::string asString(void) const = 0;

    std::filesystem::path m_basePath;
    mutable bool m_fatal {false};

  protected:
    std::map<std::string, TouchConfig> m_touch;
};
