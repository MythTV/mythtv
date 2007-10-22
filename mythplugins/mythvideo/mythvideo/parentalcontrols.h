#ifndef PARENTALCONTROLS_H_
#define PARENTALCONTROLS_H_

class ParentalLevel
{
  public:
    enum Level { plNone = 0, plLowest = 1, plLow = 2, plMedium = 3,
                 plHigh = 4 };

  public:
    ParentalLevel(Level pl);
    explicit ParentalLevel(int pl);
    ParentalLevel(const ParentalLevel &rhs);
    ParentalLevel &operator=(const ParentalLevel &rhs);
    ParentalLevel &operator=(Level pl);

    ParentalLevel &operator++();

    ParentalLevel &operator+=(int amount);

    ParentalLevel &operator--();

    ParentalLevel &operator-=(int amount);

    Level GetLevel() const;

    void reset() { m_hitlimit = false; }
    bool good() const { return !m_hitlimit; }

  private:
    Level m_pl;
    bool m_hitlimit;
};

bool operator!=(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator==(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator<(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator>(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator<=(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator>=(const ParentalLevel &lhs, const ParentalLevel &rhs);

bool checkParentPassword(ParentalLevel::Level to_level,
                         ParentalLevel::Level current = ParentalLevel::plNone);

#endif // PARENTALCONTROLS_H_
