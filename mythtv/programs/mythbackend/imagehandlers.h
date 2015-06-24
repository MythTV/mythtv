//! \file
//! \brief Encapsulates BE

#ifndef IMAGEHANDLERS_H
#define IMAGEHANDLERS_H

#include <QStringList>

//! Processes BE requests regarding images
class ImageHandler
{
public:

    static QStringList HandleRename(QString, QString);
    static QStringList HandleDelete(QString);
    static QStringList HandleGetMetadata(QString);
    static QStringList HandleMove(QString, QString);
    static QStringList HandleHide(bool, QString);
    static QStringList HandleTransform(int, QString);
    static QStringList HandleDirs(QStringList);
    static QStringList HandleCover(int, int);
    static QStringList HandleIgnore(QString);
};

#endif // IMAGEHANDLERS_H
