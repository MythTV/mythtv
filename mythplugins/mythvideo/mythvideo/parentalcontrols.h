#ifndef PARENTALCONTROLS_H_
#define PARENTALCONTROLS_H_

#include <QStringList>

class ParentalLevel : public QObject
{
    Q_OBJECT
  public:
    enum Level { plNone = 0, plLowest = 1, plLow = 2, plMedium = 3,
                 plHigh = 4 };

  public:
    ParentalLevel(Level pl = plNone);
    explicit ParentalLevel(int pl);
    ParentalLevel(const ParentalLevel &rhs);
    ParentalLevel &operator=(const ParentalLevel &rhs);
    ParentalLevel &operator=(Level pl);
    ParentalLevel &operator++();
    ParentalLevel &operator+=(int amount);
    ParentalLevel &operator--();
    ParentalLevel &operator-=(int amount);

    Level GetLevel() const;
    void  SetLevel(Level pl);

    void reset() { m_hitlimit = false; }
    bool good() const { return !m_hitlimit; }

  protected slots:
    bool verifyPassword(const QString &password="");

  signals:
    void LevelChanged();

  private:
    void checkPassword();

    Level m_currentLevel;
    Level m_previousLevel;
    bool m_hitlimit;
    QStringList m_passwords;
};

bool operator!=(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator==(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator<(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator>(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator<=(const ParentalLevel &lhs, const ParentalLevel &rhs);
bool operator>=(const ParentalLevel &lhs, const ParentalLevel &rhs);

#endif // PARENTALCONTROLS_H_
