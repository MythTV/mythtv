#ifndef _TUNE_H
#define _TUNE_H

#ifdef NEWSTRUCT
  #include <linux/dvb/frontend.h>
#else

// The following defines make the "OLDSTRUCT" driver more compatible with NEWSTRUCT.

  #include <ost/frontend.h>

#define fe_status_t FrontendStatus
#define fe_spectral_inversion_t SpectralInversion
#define fe_modulation_t Modulation
#define fe_code_rate_t CodeRate
#define fe_transmit_mode_t TransmitMode
#define fe_guard_interval_t GuardInterval
#define fe_bandwidth_t BandWidth
#define fe_sec_voltage_t SecVoltage
#define dmx_pes_filter_params dmxPesFilterParams
#define dmx_sct_filter_params dmxSctFilterParams
#define dmx_pes_type_t dmxPesType_t
#endif

#include "dvb_defaults.h"

#ifdef __cplusplus
extern "C" {
#endif

int tune_it(int fd_frontend, int fd_sec, unsigned int freq, unsigned int srate, char pol, int tone, fe_spectral_inversion_t specInv, unsigned int diseqc,fe_modulation_t modulation,fe_code_rate_t HP_CodeRate,fe_code_rate_t LP_CodeRate,fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth, fe_hierarchy_t hierarchy);

#ifdef __cplusplus
}
#endif

#endif
