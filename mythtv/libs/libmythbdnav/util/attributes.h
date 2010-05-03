#ifndef LIBBLURAY_ATTRIBUTES_H_
#define LIBBLURAY_ATTRIBUTES_H_

#ifdef __cplusplus
extern "C" {
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3 )
#  define BD_ATTR_FORMAT_PRINTF(format,var) __attribute__((__format__(__printf__,format,var)))
#  define BD_ATTR_MALLOC                    __attribute__((__malloc__))
#else
#  define BD_ATTR_FORMAT_PRINTF(format,var)
#  define BD_ATTR_MALLOC
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBBLURAY_ATTRIBUTES_H_ */
