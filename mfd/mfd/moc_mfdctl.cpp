/****************************************************************************
** MFDCTL meta object code from reading C++ file 'mfdctl.h'
**
** Created: Fri Oct 3 21:43:51 2003
**      by: The Qt MOC ($Id$)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "mfdctl.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.2.0b1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *MFDCTL::className() const
{
    return "MFDCTL";
}

QMetaObject *MFDCTL::metaObj = 0;
static QMetaObjectCleanUp cleanUp_MFDCTL( "MFDCTL", &MFDCTL::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString MFDCTL::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MFDCTL", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString MFDCTL::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MFDCTL", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* MFDCTL::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QObject::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"MFDCTL", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_MFDCTL.setMetaObject( metaObj );
    return metaObj;
}

void* MFDCTL::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "MFDCTL" ) )
	return this;
    return QObject::qt_cast( clname );
}

bool MFDCTL::qt_invoke( int _id, QUObject* _o )
{
    return QObject::qt_invoke(_id,_o);
}

bool MFDCTL::qt_emit( int _id, QUObject* _o )
{
    return QObject::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool MFDCTL::qt_property( int id, int f, QVariant* v)
{
    return QObject::qt_property( id, f, v);
}

bool MFDCTL::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
