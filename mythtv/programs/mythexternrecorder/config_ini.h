#pragma once

#include "config_base.h"

#include <QSettings>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ExternIniConfig : public ExternConfig
{
  public:
    explicit ExternIniConfig(const std::filesystem::path &filename);

    [[nodiscard]]
    bool hasTable(const std::string& table) const override;

    const VarContainer& allVariables(void) const override {
        return m_variables;
    }
    void updateVariable(std::string_view key, std::string_view value) override;

    [[nodiscard]]
    const std::map<std::string, TouchConfig> &
    touchConfigs() const noexcept override
    {
        return m_touch;
    }

    [[nodiscard]]
    std::optional<std::string> getValue(std::string_view table,
                                        std::string_view key,
                                        bool expand) const override;

    bool loadChannels(void) override;

    [[nodiscard]]
    std::optional<std::string>
      getChannelValue(std::string_view key, bool expand) const override;

    [[nodiscard]]
    std::size_t channelCount() const noexcept override;

    [[nodiscard]]
    std::optional<ChannelInfo> firstChannel() const override;

    [[nodiscard]]
    std::optional<ChannelInfo> nextChannel() const override;

    std::string expandVars(std::string value) const override;

    [[nodiscard]]
    std::string asString(const QSettings& settings) const;

    [[nodiscard]]
    std::string asString(void) const override;

  private:
    struct ChannelEntry
    {
        std::string section;
        ChannelInfo info;
    };

    void load(const std::filesystem::path &filename);

    [[nodiscard]]
    QString getSectionName(std::string_view name) const;

    [[nodiscard]]
    std::optional<QString> findKey(const QSettings &settings,
                                   std::string_view section,
                                   std::string_view key) const;

    [[nodiscard]]
    std::optional<std::string> getValue(const QSettings &settings,
                                        std::string_view section,
                                        std::string_view key,
                                        bool expand) const;

    [[nodiscard]]
    std::optional<std::string> getChannelValue(const ChannelEntry &channel,
                                               std::string_view key,
                                               bool expand) const;

    [[nodiscard]]
    std::optional<ChannelInfo> makeChannelInfo(const QSettings &settings,
                                               const QString &section) const;

    [[nodiscard]]
    std::string expandVariable(std::string_view name) const;

    [[nodiscard]]
    bool variableExists(std::string_view name) const;

    mutable std::mutex m_mutex;

    QSettings m_settings;

    std::unique_ptr<QSettings> m_channelSettings;

    std::vector<ChannelEntry> m_channels;
    mutable std::size_t m_channelIndex{0};
    mutable bool m_channelIterValid{false};
};
