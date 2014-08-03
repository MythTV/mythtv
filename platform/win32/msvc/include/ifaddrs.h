// This allows Visual Studio to compule without having to update mythconfig.h
// Ultimately, we should generate a mythconfig.h file specific to VS & windows

#ifdef HAVE_GETIFADDRS
#undef HAVE_GETIFADDRS
#endif
