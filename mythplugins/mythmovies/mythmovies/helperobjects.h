#ifndef HELPEROBJECTS_H_
#define HELPEROBJECTS_H_

#include <q3valuevector.h>

class Theater;

typedef Q3ValueVector<Theater> TheaterVector;

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

typedef Q3ValueVector<Movie> MovieVector;

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
