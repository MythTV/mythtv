#ifndef LIBDVDNAV_VM_SERIALIZE_H
#define LIBDVDNAV_VM_SERIALIZE_H

#include "vm.h"

char *vm_serialize_dvd_state(const dvd_state_t *state);
int vm_deserialize_dvd_state(const char* serialized, dvd_state_t *state);

#endif /* LIBDVDNAV_VM_SERIALIZE_H */
