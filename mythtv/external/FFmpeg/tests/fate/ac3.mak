FATE_AC3 += fate-ac3-2.0
fate-ac3-2.0: CMD = pcm -i $(SAMPLES)/ac3/monsters_inc_2.0_192_small.ac3
fate-ac3-2.0: CMP = oneoff
fate-ac3-2.0: REF = $(SAMPLES)/ac3/monsters_inc_2.0_192_small.pcm

FATE_AC3 += fate-ac3-5.1
fate-ac3-5.1: CMD = pcm -i $(SAMPLES)/ac3/monsters_inc_5.1_448_small.ac3
fate-ac3-5.1: CMP = oneoff
fate-ac3-5.1: REF = $(SAMPLES)/ac3/monsters_inc_5.1_448_small.pcm

FATE_AC3 += fate-eac3-1
fate-eac3-1: CMD = pcm -i $(SAMPLES)/eac3/csi_miami_5.1_256_spx_small.eac3
fate-eac3-1: CMP = oneoff
fate-eac3-1: REF = $(SAMPLES)/eac3/csi_miami_5.1_256_spx_small.pcm

FATE_AC3 += fate-eac3-2
fate-eac3-2: CMD = pcm -i $(SAMPLES)/eac3/csi_miami_stereo_128_spx_small.eac3
fate-eac3-2: CMP = oneoff
fate-eac3-2: REF = $(SAMPLES)/eac3/csi_miami_stereo_128_spx_small.pcm

FATE_AC3 += fate-eac3-3
fate-eac3-3: CMD = pcm -i $(SAMPLES)/eac3/matrix2_commentary1_stereo_192_small.eac3
fate-eac3-3: CMP = oneoff
fate-eac3-3: REF = $(SAMPLES)/eac3/matrix2_commentary1_stereo_192_small.pcm

FATE_AC3 += fate-eac3-4
fate-eac3-4: CMD = pcm -i $(SAMPLES)/eac3/serenity_english_5.1_1536_small.eac3
fate-eac3-4: CMP = oneoff
fate-eac3-4: REF = $(SAMPLES)/eac3/serenity_english_5.1_1536_small.pcm

FATE_AC3 += fate-ac3-encode
fate-ac3-encode: CMD = enc_dec_pcm ac3 wav s16le $(REF) -c:a ac3 -b:a 128k
fate-ac3-encode: CMP = stddev
fate-ac3-encode: REF = $(SAMPLES)/audio-reference/luckynight_2ch_44kHz_s16.wav
fate-ac3-encode: CMP_SHIFT = -1024
fate-ac3-encode: CMP_TARGET = 399.62
fate-ac3-encode: SIZE_TOLERANCE = 488
fate-ac3-encode: FUZZ = 3

FATE_AC3 += fate-eac3-encode
fate-eac3-encode: CMD = enc_dec_pcm eac3 wav s16le $(REF) -c:a eac3 -b:a 128k
fate-eac3-encode: CMP = stddev
fate-eac3-encode: REF = $(SAMPLES)/audio-reference/luckynight_2ch_44kHz_s16.wav
fate-eac3-encode: CMP_SHIFT = -1024
fate-eac3-encode: CMP_TARGET = 514.02
fate-eac3-encode: SIZE_TOLERANCE = 488
fate-eac3-encode: FUZZ = 3

FATE_SAMPLES_AVCONV += $(FATE_AC3)
fate-ac3: $(FATE_AC3)
