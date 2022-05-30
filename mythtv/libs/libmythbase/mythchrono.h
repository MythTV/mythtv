#ifndef MYTHCHRONO_H
#define MYTHCHRONO_H

#include <cmath>
#include <sys/time.h> // For gettimeofday
#include <QMetaType>


// Include the std::chrono definitions, and set up to parse the
// std::chrono literal suffixes.
#include <chrono>
using std::chrono::duration_cast;
using namespace std::chrono_literals;
Q_DECLARE_METATYPE(std::chrono::seconds);
Q_DECLARE_METATYPE(std::chrono::milliseconds);
Q_DECLARE_METATYPE(std::chrono::microseconds);

// Grab the underlying std::chrono::duration data type for future use.
using CHRONO_TYPE = std::chrono::seconds::rep;

// Copy these c++20 literals from the chrono header file
#if __cplusplus <= 201703L
namespace std::chrono // NOLINT(cert-dcl58-cpp)
{
    using days   = duration<CHRONO_TYPE, ratio<86400>>;
    using weeks  = duration<CHRONO_TYPE, ratio<604800>>;
    using months = duration<CHRONO_TYPE, ratio<2629746>>;
    using years  = duration<CHRONO_TYPE, ratio<31556952>>;
}
#endif // C++20


//
// Set up some additional data types for use by MythTV.
//

// There are a number of places that hold/manipulate time in the form
// of a floating point number. Create unique types for these.
using floatsecs = std::chrono::duration<double>;
using floatmsecs = std::chrono::duration<double, std::milli>;
using floatusecs = std::chrono::duration<double, std::micro>;

// There are a handful of places that hold a time value in units of
// AV_TIME_BASE. Create a unique type for this.
#if defined AV_TIME_BASE
using av_duration = std::chrono::duration<int64_t,std::ratio<1,AV_TIME_BASE>>;
#endif

// Define namespaces for the presentation timestamp and a literal
// suffix, and set up to parse the literal suffix.
namespace mpeg
{
    namespace chrono
    {
        using pts = std::chrono::duration<CHRONO_TYPE, std::ratio<1, 90000>>;
    }
    namespace chrono_literals
    {
        constexpr mpeg::chrono::pts operator ""_pts(unsigned long long v) {
            return mpeg::chrono::pts(v); }
    }
}
using namespace mpeg::chrono_literals;

// Set up types to easily reference the system clock.
using SystemClock = std::chrono::system_clock;
using SystemTime = std::chrono::time_point<SystemClock>;


//
// Functions for converting existing data types to std::chrono.
//

/// @brief Helper function for convert a floating point number to a duration.
///
/// \param  value A floating point number that represents a time in seconds.
/// \returns The same number of seconds as a std::chrono::seconds.
template <typename T>
typename std::enable_if_t<std::is_floating_point_v<T>, std::chrono::seconds>
secondsFromFloat (T value)
{
    return std::chrono::seconds(static_cast<int64_t>(value));
}

/// @brief Helper function for convert a floating point number to a duration.
///
/// \param  value A floating point number that represents a time in milliseconds.
/// \returns The same number of seconds as a std::chrono::milliseconds.
template <typename T>
typename std::enable_if_t<std::is_floating_point_v<T>, std::chrono::milliseconds>
millisecondsFromFloat (T value)
{
    return std::chrono::milliseconds(static_cast<int64_t>(value));
}

/// @brief Helper function for convert a floating point number to a duration.
///
/// \param  value A floating point number that represents a time in microseconds.
/// \returns The same number of seconds as a std::chrono::microseconds.
template <typename T>
typename std::enable_if_t<std::is_floating_point_v<T>, std::chrono::microseconds>
microsecondsFromFloat (T value)
{
    return std::chrono::microseconds(static_cast<int64_t>(value));
}

/// @brief Build a duration from separate minutes, seconds, etc.
static constexpr
std::chrono::milliseconds millisecondsFromParts (int hours, int minutes = 0,
                                                 int seconds = 0, int milliseconds = 0)
{
    return std::chrono::hours(hours)
        + std::chrono::minutes(minutes)
        + std::chrono::seconds(seconds)
        + std::chrono::milliseconds(milliseconds);
}

/// @brief Convert a timeval to a duration.
///
/// Always specify the desired precision when calling this function.
/// I.E. Always call in the form:
/// durationFromTimeval<std::chrono::milliseconds>(t);
///
/// Timevals can support a precision as small as microseconds.
template <typename T>
constexpr T durationFromTimeval (timeval t)
{
    std::chrono::microseconds value = std::chrono::seconds(t.tv_sec) +
        std::chrono::microseconds(t.tv_usec);
    if constexpr (std::is_same_v<T,std::chrono::microseconds>)
        return value;
    return duration_cast<T>(value);
}

/// @brief Compute delta between timevals and convert to a duration.
///
/// Always specify the desired precision when calling this function.
/// I.E. Always call in the form:
/// durationFromTimevalDelta<std::chrono::milliseconds>(t);
///
/// Timevals can support a precision as small as microseconds.
template <typename T>
constexpr T durationFromTimevalDelta (timeval a, timeval b)
{
    auto usec_a = durationFromTimeval<std::chrono::microseconds>(a);
    auto usec_b = durationFromTimeval<std::chrono::microseconds>(b);
    if constexpr (std::is_same_v<T,std::chrono::microseconds>)
        return usec_a - usec_b;
    return duration_cast<T>(usec_a - usec_b);
}

/// @brief Convert a timespec to a duration.
///
/// Always specify the desired precision when calling this function.
/// I.E. Always call in the form:
/// durationFromTimespec<std::chrono::milliseconds>(t);
///
/// Timevals can support a precision as small as nanoseconds.
template <typename T>
constexpr T durationFromTimespec (struct timespec time)
{
    std::chrono::nanoseconds nsec = std::chrono::seconds(time.tv_sec)
        + std::chrono::nanoseconds(time.tv_nsec);
    if constexpr (std::is_same_v<T,std::chrono::nanoseconds>)
        return nsec;
    return duration_cast<T>(nsec);
}


//
// Get current time as std::chrono duration
//

/// @brief Get the currenttime as a duration.
///
/// Always specify the desired precision when calling this function.
/// I.E. Always call in the form:
/// nowAsDuration<std::chrono::milliseconds>(t);
///
/// This function is based upon the gettimeoday function, so can
/// return precisions as small as microseconds.
template <typename T>
T nowAsDuration (bool adjustForTZ = false)
{
    struct timeval now {};
    struct timezone tz {};
    gettimeofday(&now, &tz);

    auto usecs = durationFromTimeval<std::chrono::microseconds>(now);
    if (adjustForTZ)
        usecs -= std::chrono::minutes(tz.tz_minuteswest);
    if constexpr (std::is_same_v<T,std::chrono::microseconds>)
        return usecs;
    return duration_cast<T>(usecs);
}

/// \brief Multiply a duration by a float, returning a duration.
template <typename T>
static constexpr T chronomult(T duration, double f)
{
    return T(std::llround(duration.count() * f));
}
/// \brief Divide a duration by a float, returning a duration.
template <typename T>
static constexpr T chronodivide(T duration, double f)
{
    return T(std::llround(duration.count() / f));
}

#endif // MYTHCHRONO_H
