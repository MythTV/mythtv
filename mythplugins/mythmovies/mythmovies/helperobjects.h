#ifndef HELPEROBJECTS_H_
#define HELPEROBJECTS_H_

#include <qvaluevector.h>

class Theater;

typedef QValueVector<Theater> TheaterVector;

class Movie
{
  public:
    QString rating;
    QString name;
    QString runningTime;
    QString showTimes;
    TheaterVector theaters;
    Movie()
    {
        rating = "";
        name = "";
        runningTime = "";
        showTimes = "";
    }
};

typedef QValueVector<Movie> MovieVector;

class Theater
{
  public:
    QString name;
    QString address;
    MovieVector movies;
    QString showTimes;
    Theater()
    {
        name = "";
        address = "";
    }
};

#endif
