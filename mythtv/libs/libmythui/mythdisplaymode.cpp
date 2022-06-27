// QT
#include <QObject>
#include <QStringList>

// MythTV
#include "mythdisplaymode.h"

// Std
#include <cmath>

MythDisplayMode::MythDisplayMode(QSize Resolution, QSize PhysicalSize,
                                 double AspectRatio, double RefreshRate)
  : m_width(Resolution.width()),
    m_height(Resolution.height()),
    m_widthMM(PhysicalSize.width()),
    m_heightMM(PhysicalSize.height())
{
    SetAspectRatio(AspectRatio);
    if (RefreshRate > 0)
        m_refreshRates.push_back(RefreshRate);
}
MythDisplayMode::MythDisplayMode(int Width, int Height, int MMWidth, int MMHeight,
                                 double AspectRatio, double RefreshRate)
  : m_width(Width),
    m_height(Height),
    m_widthMM(MMWidth),
    m_heightMM(MMHeight)
{
    SetAspectRatio(AspectRatio);
    if (RefreshRate > 0)
        m_refreshRates.push_back(RefreshRate);
}

bool MythDisplayMode::operator < (const MythDisplayMode& Other) const
{
    if (m_width < Other.m_width)
        return true;
    if (m_height < Other.m_height)
        return true;
    return false;
}

bool MythDisplayMode::operator == (const MythDisplayMode &Other) const
{
    return m_width == Other.m_width && m_height == Other.m_height;
}

void MythDisplayMode::Init()
{
    m_width = m_height = m_widthMM = m_heightMM = 0;
    m_aspect = -1.0;
}

QSize MythDisplayMode::Resolution() const
{
    return { m_width, m_height };
}

int MythDisplayMode::Width() const
{
    return m_width;
}

int MythDisplayMode::Height() const
{
    return m_height;
}

int MythDisplayMode::WidthMM() const
{
    return m_widthMM;
}

int MythDisplayMode::HeightMM() const
{
    return m_heightMM;
}

const MythDisplayRates& MythDisplayMode::RefreshRates() const
{
    return m_refreshRates;
}

double MythDisplayMode::AspectRatio() const
{
    if (m_aspect <= 0.0)
    {
        if (0 == m_heightMM)
            return 1.0;
        return static_cast<double>(m_widthMM) / m_heightMM;
    }
    return m_aspect;
}

double MythDisplayMode::RefreshRate() const
{
    if (!m_refreshRates.empty())
        return m_refreshRates[0];
    return 0.0;
}

void MythDisplayMode::SetAspectRatio(double AspectRatio)
{
    if (AspectRatio > 0.0)
        m_aspect = AspectRatio;
    else if (HeightMM())
        m_aspect = static_cast<double>(m_widthMM) / m_heightMM;
}

void MythDisplayMode::AddRefreshRate(double Rate)
{
    m_refreshRates.push_back(Rate);
    std::sort(m_refreshRates.begin(), m_refreshRates.end());
}

void MythDisplayMode::ClearRefreshRates()
{
    m_refreshRates.clear();
}

void MythDisplayMode::SetRefreshRate(double Rate)
{
    ClearRefreshRates();
    AddRefreshRate(Rate);
}

uint64_t MythDisplayMode::CalcKey(QSize Size, double Rate)
{
    return (static_cast<uint64_t>(Size.width()) << 34) |
           (static_cast<uint64_t>(Size.height()) << 18) |
           (static_cast<uint64_t>(Rate * 1000.0));
}

/// \brief Determine whether two rates are considered equal with the given precision
bool MythDisplayMode::CompareRates(double First, double Second, double Precision)
{
    return qAbs(First - Second) < Precision;
}

int MythDisplayMode::FindBestMatch(const MythDisplayModes& Modes,
                                   const MythDisplayMode& Mode, double &TargetRate)
{
    double videorate = Mode.RefreshRate();
    bool rate2x = false;
    bool end = false;

    // We will give priority to refresh rates that are twice what is looked for
    if ((videorate > 24.5) && (videorate < 30.5))
    {
        rate2x = true;
        videorate *= 2.0;
    }

    // Amend vector with custom list
    for (size_t i = 0; i < Modes.size(); ++i)
    {
        if (Modes[i].Width() == Mode.Width() && Modes[i].Height() == Mode.Height())
        {
            auto rates = Modes[i].RefreshRates();
            if (!rates.empty() && !qFuzzyCompare(videorate + 1.0,  1.0))
            {
                while (!end)
                {
                    for (double precision : { 0.001, 0.01, 0.1 })
                    {
                        for (double rate : rates)
                        {
                            // Multiple of target_rate will do
                            if (CompareRates(videorate, rate, precision) ||
                                (qAbs(videorate - fmod(rate, videorate))
                                 <= precision) ||
                                (fmod(rate,videorate) <= precision))
                            {
                                TargetRate = rate;
                                return static_cast<int>(i);
                            }
                        }
                    }
                    // Can't find exact frame rate, so try rounding to the
                    // nearest integer, so 23.97Hz will work with 24Hz etc
                    for (double precision : { 0.01, 0.1, 1.0 })
                    {
                        double rounded = round(videorate);
                        for (double rate : rates)
                        {
                            // Multiple of target_rate will do
                            if (CompareRates(rounded, rate, precision) ||
                                (qAbs(rounded - fmod(rate, rounded)) <= precision) ||
                                (fmod(rate,rounded) <= precision))
                            {
                                TargetRate = rate;
                                return static_cast<int>(i);
                            }
                        }
                    }
                    if (rate2x)
                    {
                        videorate /= 2.0;
                        rate2x = false;
                    }
                    else
                    {
                        end = true;
                    }
                }
                TargetRate = rates[rates.size() - 1];
            }
            else if (!rates.empty())
            {
                TargetRate = rates[rates.size() - 1];
            }
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void extract_key(uint64_t key, double& rate, int& height, int& width)
{
    rate    = (key & ((1 << 18) - 1)) / 1000.0;
    height  = (key >> 18) & ((1 << 16) - 1);
    width   = (key >> 34) & ((1 << 16) - 1);
}

uint64_t MythDisplayMode::FindBestScreen(const DisplayModeMap& Map,
                                         int Width, int Height, double Rate)
{
    // 1. search for exact match (width, height, rate)
    // 2. search for resolution, ignoring rate
    // 3. search for matching height and rate (or any rate if rate = 0)
    // 4. search for 2x rate
    // 5. search for 1x rate

    for (const auto & it : Map)
    {
        double rate { 0.0 };
        int height  { 0   };
        int width   { 0   };
        extract_key(it.first, rate, height, width);
        if (width == Width && height == Height && CompareRates(Rate, rate, 0.01))
            return it.first;
    }

    for (const auto & it : Map)
    {
        double rate { 0.0 };
        int height  { 0   };
        int width   { 0   };
        extract_key(it.first, rate, height, width);
        if (width == Width && height == Height && qFuzzyCompare(rate + 1.0, 1.0))
            return it.first;
    }

    for (const auto & it : Map)
    {
        double rate { 0.0 };
        int height  { 0   };
        int width   { 0   };
        extract_key(it.first, rate, height, width);
        if ((width == 0 && height == Height &&
             (CompareRates(Rate, rate, 0.01) || qFuzzyCompare(rate + 1.0, 1.0))) ||
            (width == 0 && height == 0 && CompareRates(Rate, rate * 2.0, 0.01)) ||
            (width == 0 && height == 0 && CompareRates(Rate, rate, 0.01)))
        {
            return it.first;
        }
    }

    return 0;
}

QString MythDisplayMode::ToString() const
{
    QStringList rates;
    for (auto rate : m_refreshRates)
        rates << QString::number(rate, 'f', 2);
    return QObject::tr("%1x%2@%3Hz").arg(m_width).arg(m_height).arg(rates.join(", "));
}
