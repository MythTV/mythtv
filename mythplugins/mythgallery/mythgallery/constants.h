/* ============================================================
 * File  : constants.h
 * Description : 
 * 

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */


#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

static const QString IMAGE_FILENAMES(
	"*.jpg *.JPG *.jpeg *.JPEG *.png *.PNG "
	"*.tif *.TIF *.tiff *.TIFF *.bmp *.BMP *.gif *.GIF");
static const QString MOVIE_FILENAMES(
	"*.avi *.AVI *.mpg *.MPG *.mpeg *.MPEG *.mov *.MOV *.wmv *.WMV");
static const QString MEDIA_FILENAMES =
	IMAGE_FILENAMES + QString(" ") + MOVIE_FILENAMES;

#endif /* CONSTANTS_H */
