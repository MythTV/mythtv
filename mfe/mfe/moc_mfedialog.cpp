/****************************************************************************
** MfeDialog meta object code from reading C++ file 'mfedialog.h'
**
** Created: Mon Mar 15 14:41:21 2004
**      by: The Qt MOC ($Id$)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "mfedialog.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *MfeDialog::className() const
{
    return "MfeDialog";
}

QMetaObject *MfeDialog::metaObj = 0;
static QMetaObjectCleanUp cleanUp_MfeDialog( "MfeDialog", &MfeDialog::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString MfeDialog::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MfeDialog", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString MfeDialog::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MfeDialog", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* MfeDialog::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = MythThemedDialog::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ "node_int", &static_QUType_int, 0, QUParameter::In },
	{ "attributes", &static_QUType_ptr, "IntVector", QUParameter::In }
    };
    static const QUMethod slot_0 = {"handleTreeListSignals", 2, param_slot_0 };
    static const QUParameter param_slot_1[] = {
	{ "which_mfd", &static_QUType_int, 0, QUParameter::In },
	{ "name", &static_QUType_QString, 0, QUParameter::In },
	{ "host", &static_QUType_QString, 0, QUParameter::In },
	{ "found", &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_1 = {"mfdDiscovered", 4, param_slot_1 };
    static const QUParameter param_slot_2[] = {
	{ "which_mfd", &static_QUType_int, 0, QUParameter::In },
	{ "paused", &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_2 = {"paused", 2, param_slot_2 };
    static const QUParameter param_slot_3[] = {
	{ "which_mfd", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_3 = {"stopped", 1, param_slot_3 };
    static const QUParameter param_slot_4[] = {
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_4 = {"playing", 9, param_slot_4 };
    static const QUParameter param_slot_5[] = {
	{ 0, &static_QUType_int, 0, QUParameter::In },
	{ 0, &static_QUType_ptr, "GenericTree", QUParameter::In }
    };
    static const QUMethod slot_5 = {"changeMetadata", 2, param_slot_5 };
    static const QUMethod slot_6 = {"cycleMfd", 0, 0 };
    static const QUMethod slot_7 = {"stopAudio", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "handleTreeListSignals(int,IntVector*)", &slot_0, QMetaData::Public },
	{ "mfdDiscovered(int,QString,QString,bool)", &slot_1, QMetaData::Public },
	{ "paused(int,bool)", &slot_2, QMetaData::Public },
	{ "stopped(int)", &slot_3, QMetaData::Public },
	{ "playing(int,int,int,int,int,int,int,int,int)", &slot_4, QMetaData::Public },
	{ "changeMetadata(int,GenericTree*)", &slot_5, QMetaData::Public },
	{ "cycleMfd()", &slot_6, QMetaData::Public },
	{ "stopAudio()", &slot_7, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"MfeDialog", parentObject,
	slot_tbl, 8,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_MfeDialog.setMetaObject( metaObj );
    return metaObj;
}

void* MfeDialog::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "MfeDialog" ) )
	return this;
    return MythThemedDialog::qt_cast( clname );
}

bool MfeDialog::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: handleTreeListSignals((int)static_QUType_int.get(_o+1),(IntVector*)static_QUType_ptr.get(_o+2)); break;
    case 1: mfdDiscovered((int)static_QUType_int.get(_o+1),(QString)static_QUType_QString.get(_o+2),(QString)static_QUType_QString.get(_o+3),(bool)static_QUType_bool.get(_o+4)); break;
    case 2: paused((int)static_QUType_int.get(_o+1),(bool)static_QUType_bool.get(_o+2)); break;
    case 3: stopped((int)static_QUType_int.get(_o+1)); break;
    case 4: playing((int)static_QUType_int.get(_o+1),(int)static_QUType_int.get(_o+2),(int)static_QUType_int.get(_o+3),(int)static_QUType_int.get(_o+4),(int)static_QUType_int.get(_o+5),(int)static_QUType_int.get(_o+6),(int)static_QUType_int.get(_o+7),(int)static_QUType_int.get(_o+8),(int)static_QUType_int.get(_o+9)); break;
    case 5: changeMetadata((int)static_QUType_int.get(_o+1),(GenericTree*)static_QUType_ptr.get(_o+2)); break;
    case 6: cycleMfd(); break;
    case 7: stopAudio(); break;
    default:
	return MythThemedDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool MfeDialog::qt_emit( int _id, QUObject* _o )
{
    return MythThemedDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool MfeDialog::qt_property( int id, int f, QVariant* v)
{
    return MythThemedDialog::qt_property( id, f, v);
}

bool MfeDialog::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
