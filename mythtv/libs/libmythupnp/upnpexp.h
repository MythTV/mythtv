#ifndef UPNPEXP_H_
#define UPNPEXP_H_

#if (__GNUC__ >= 4)
#define UPNP_HIDDEN __attribute__((visibility("hidden")))
#define UPNP_PUBLIC __attribute__((visibility("default")))
#else
#define UPNP_HIDDEN
#ifdef _MSC_VER
	#ifdef UPNP_API
		#define UPNP_PUBLIC __declspec( dllexport )
	#else
		#define UPNP_PUBLIC __declspec( dllimport )
	#endif
#else
	#define UPNP_PUBLIC
#endif
#endif

#endif // UPNPEXP_H_
