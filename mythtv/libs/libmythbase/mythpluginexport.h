#ifndef MYTH_PLUGIN_EXPORT_H_
#define MYTH_PLUGIN_EXPORT_H_

#include <QtGlobal>

#ifdef MPLUGIN_API
# define MPLUGIN_PUBLIC Q_DECL_EXPORT
#else
# define MPLUGIN_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTH_PLUGIN_EXPORT_H_
