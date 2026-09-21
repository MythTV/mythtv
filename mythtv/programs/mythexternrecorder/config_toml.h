#pragma once

#include <toml++/toml.hpp>

#include <filesystem>
#include <string>
#include <string_view>

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "config_base.h"
#include "touchmonitor.h"

class ExternTomlConfig : public ExternConfig
{
  public:
    explicit ExternTomlConfig(const std::filesystem::path& filename);

    [[nodiscard]]
    bool hasTable(const std::string& table) const override;

    void updateVariable(std::string_view key, std::string_view value) override;

    [[nodiscard]]
    const std::map<std::string, TouchConfig>&
      touchConfigs() const noexcept override
    {
        return m_touch;
    }

    std::string expandVars(std::string value) const override;

    static std::string tableAsString(const toml::table& tbl);
    std::string asString(void) const override;

    bool loadChannels(void) override;

  private:
    void load(const std::filesystem::path& filename);
    bool loadIncludes(void);

    bool loadTouch(void);

    std::string getSectionName(const toml::table& tbl,
                               std::string_view name) const;

    static toml::table parseFile(const std::filesystem::path& filename,
                                 const std::string& where);

    static void mergeTable(const toml::table& source,
                           toml::table& destination);

    std::string expandVariable(std::string_view name) const;

    std::string sectionName(const std::string& section);

    bool variableExists(std::string_view name) const;

    const toml::table variables(void) const;

    [[nodiscard]]
    std::optional<std::string> getValue(const toml::table& tbl,
                                        std::string_view keyName) const;

    [[nodiscard]]
    virtual std::optional<std::string> getValue(std::string_view table,
                                        std::string_view key) const override;

    std::optional<std::string> getChannelValue(std::string_view key) const override;

    [[nodiscard]]
    std::size_t channelCount() const noexcept override;

    [[nodiscard]]
    ChannelInfo channelInfo(const toml::table::const_iterator& iter) const;
    [[nodiscard]]
    std::optional<ChannelInfo> firstChannel(void) const override;
    [[nodiscard]]
    std::optional<ChannelInfo> nextChannel(void) const override;

    mutable std::mutex m_mutex;
    toml::table m_root;

    std::map<std::string, std::string> m_section;

    mutable toml::table::const_iterator m_channelIter;
    mutable bool m_channelIterValid {false};
    int m_numChannels {0};
};
