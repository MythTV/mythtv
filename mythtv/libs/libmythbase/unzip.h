/****************************************************************************
** Filename: unzip.h
** Last updated [dd/mm/yyyy]: 28/01/2007
**
** pkzip 2.0 decompression.
**
** Some of the code has been inspired by other open source projects,
** (mainly Info-Zip and Gilles Vollant's minizip).
** Compression and decompression actually uses the zlib library.
**
** Copyright (C) 2007-2008 Angius Fabrizio. All rights reserved.
**
** This file is part of the OSDaB project (http://osdab.sourceforge.net/).
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See the file LICENSE.GPL that came with this software distribution or
** visit http://www.gnu.org/copyleft/gpl.html for GPL licensing information.
**
**********************************************************************/

#ifndef OSDAB_UNZIP__H
#define OSDAB_UNZIP__H

#include <QtGlobal>
#include <QMap>
#include <QDateTime>

#include <zlib.h>

#include "mythbaseexp.h"

class UnzipPrivate;
class QIODevice;
class QFile;
class QDir;
class QStringList;
class QString;


class MBASE_PUBLIC UnZip
{
public:
	enum ErrorCode
	{
		Ok,
		ZlibInit,
		ZlibError,
		OpenFailed,
		PartiallyCorrupted,
		Corrupted,
		WrongPassword,
		NoOpenArchive,
		FileNotFound,
		ReadFailed,
		WriteFailed,
		SeekFailed,
		CreateDirFailed,
		InvalidDevice,
		InvalidArchive,
		HeaderConsistencyError,

		Skip, SkipAll // internal use only
	};

	enum ExtractionOption
	{
		//! Extracts paths (default)
		ExtractPaths = 0x0001,
		//! Ignores paths and extracts all the files to the same directory
		SkipPaths = 0x0002
	};
	Q_DECLARE_FLAGS(ExtractionOptions, ExtractionOption)

	enum CompressionMethod
	{
		NoCompression, Deflated, UnknownCompression
	};

	enum FileType
	{
		File, Directory
	};

	struct ZipEntry
	{
		ZipEntry();

		QString filename;
		QString comment;

		quint32 compressedSize;
		quint32 uncompressedSize;
		quint32 crc32;

		QDateTime lastModified;

		CompressionMethod compression;
		FileType type;

		bool encrypted;
	};

	UnZip();
	virtual ~UnZip();

	bool isOpen() const;

	ErrorCode openArchive(const QString& filename);
	ErrorCode openArchive(QIODevice* device);
	void closeArchive();

	QString archiveComment() const;

	QString formatError(UnZip::ErrorCode c) const;

	bool contains(const QString& file) const;

	QStringList fileList() const;
	QList<ZipEntry> entryList() const;

	ErrorCode extractAll(const QString& dirname, ExtractionOptions options = ExtractPaths);
	ErrorCode extractAll(const QDir& dir, ExtractionOptions options = ExtractPaths);

	ErrorCode extractFile(const QString& filename, const QString& dirname, ExtractionOptions options = ExtractPaths);
	ErrorCode extractFile(const QString& filename, const QDir& dir, ExtractionOptions options = ExtractPaths);
	ErrorCode extractFile(const QString& filename, QIODevice* device, ExtractionOptions options = ExtractPaths);

	ErrorCode extractFiles(const QStringList& filenames, const QString& dirname, ExtractionOptions options = ExtractPaths);
	ErrorCode extractFiles(const QStringList& filenames, const QDir& dir, ExtractionOptions options = ExtractPaths);

	void setPassword(const QString& pwd);

private:
	UnzipPrivate* d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(UnZip::ExtractionOptions)

#endif // OSDAB_UNZIP__H
