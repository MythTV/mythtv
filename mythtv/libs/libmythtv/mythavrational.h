#ifndef MYTH_AV_RATIONAL_H
#define MYTH_AV_RATIONAL_H

extern "C"
{
#include "libavutil/rational.h"
}

#include <QString>

/**
C++ wrapper for FFmpeg libavutil AVRational.
*/
class MythAVRational
{
  public:
    explicit MythAVRational(int n = 0, int d = 1) : m_q({n, d}) {}
    explicit MythAVRational(AVRational q) : m_q(q) {}
    /**
    @brief Convert a double to a MythAVRational.

    @note This is not an overload of the constructor since that would create
          calls to ambiguous overloads.
    */
    static MythAVRational fromDouble(double d, int64_t max)
    {
        return MythAVRational(av_d2q(d, max));
    }

    AVRational get_q() const { return m_q; }
    /// @brief Convert the rational number to fixed point.
    long long toFixed(long long base) const
    {
        return (base * m_q.num) / m_q.den;
    }
    double toDouble() const { return av_q2d(m_q); }
    QString toString() const
    {
        return QStringLiteral("%1/%2").arg(QString::number(m_q.num), QString::number(m_q.den));
    }

    bool isNonzero() const { return m_q.num != 0; }
    /// @return True, if the denominator is not zero; 0/0 is indeterminate and x/0 is undefined.
    bool isValid() const { return m_q.den != 0; }

    [[nodiscard]] MythAVRational reduce(int64_t max) const
    {
        bool ignore {false};
        return reduce(max, ignore);
    }
    [[nodiscard]] MythAVRational reduce(int64_t max, bool& exact) const
    {
        MythAVRational q;
        exact = av_reduce(&q.m_q.num, &q.m_q.den, m_q.num, m_q.den, max);
        return q;
    }
    [[nodiscard]] MythAVRational invert() const { return MythAVRational(av_inv_q(m_q)); }

    static int cmp(const MythAVRational& a, const MythAVRational& b) { return av_cmp_q(a.m_q, b.m_q); }

    MythAVRational& operator+=(const MythAVRational& rhs);
    MythAVRational& operator-=(const MythAVRational& rhs);
    MythAVRational& operator*=(const MythAVRational& rhs);
    MythAVRational& operator/=(const MythAVRational& rhs);
  private:
    AVRational m_q {.num = 0, .den = 1};
};

inline bool operator==(const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) == 0; }
inline bool operator!=(const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) != 0; }
inline bool operator< (const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) <  0; }
inline bool operator> (const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) >  0; }
inline bool operator<=(const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) <= 0; }
inline bool operator>=(const MythAVRational& lhs, const MythAVRational& rhs) { return MythAVRational::cmp(lhs, rhs) >= 0; }

inline MythAVRational operator+(const MythAVRational& lhs, const MythAVRational& rhs)
{
    return MythAVRational(av_add_q(lhs.get_q(), rhs.get_q()));
}

inline MythAVRational operator-(const MythAVRational& lhs, const MythAVRational& rhs)
{
    return MythAVRational(av_sub_q(lhs.get_q(), rhs.get_q()));
}

inline MythAVRational operator*(const MythAVRational& lhs, const MythAVRational& rhs)
{
    return MythAVRational(av_mul_q(lhs.get_q(), rhs.get_q()));
}

inline MythAVRational operator/(const MythAVRational& lhs, const MythAVRational& rhs)
{
    return MythAVRational(av_div_q(lhs.get_q(), rhs.get_q()));
}

inline MythAVRational& MythAVRational::operator+=(const MythAVRational& rhs) { return *this = operator+(*this, rhs); }
inline MythAVRational& MythAVRational::operator-=(const MythAVRational& rhs) { return *this = operator-(*this, rhs); }
inline MythAVRational& MythAVRational::operator*=(const MythAVRational& rhs) { return *this = operator*(*this, rhs); }
inline MythAVRational& MythAVRational::operator/=(const MythAVRational& rhs) { return *this = operator/(*this, rhs); }
#endif // MYTH_AV_RATIONAL_H
