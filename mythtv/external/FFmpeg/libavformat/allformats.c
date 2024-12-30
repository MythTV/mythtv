/*
 * Register all the formats and protocols
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "libavformat/internal.h"
#include "avformat.h"
#include "demux.h"
#include "mux.h"

/* (de)muxers */
extern const FFOutputFormat ff_a64_muxer;
extern const FFInputFormat  ff_aa_demuxer;
extern const FFInputFormat  ff_aac_demuxer;
extern const FFInputFormat  ff_aax_demuxer;
extern const FFInputFormat  ff_ac3_demuxer;
extern const FFOutputFormat ff_ac3_muxer;
extern const FFInputFormat  ff_ac4_demuxer;
extern const FFOutputFormat ff_ac4_muxer;
extern const FFInputFormat  ff_ace_demuxer;
extern const FFInputFormat  ff_acm_demuxer;
extern const FFInputFormat  ff_act_demuxer;
extern const FFInputFormat  ff_adf_demuxer;
extern const FFInputFormat  ff_adp_demuxer;
extern const FFInputFormat  ff_ads_demuxer;
extern const FFOutputFormat ff_adts_muxer;
extern const FFInputFormat  ff_adx_demuxer;
extern const FFOutputFormat ff_adx_muxer;
extern const FFInputFormat  ff_aea_demuxer;
extern const FFOutputFormat ff_aea_muxer;
extern const FFInputFormat  ff_afc_demuxer;
extern const FFInputFormat  ff_aiff_demuxer;
extern const FFOutputFormat ff_aiff_muxer;
extern const FFInputFormat  ff_aix_demuxer;
extern const FFInputFormat  ff_alp_demuxer;
extern const FFOutputFormat ff_alp_muxer;
extern const FFInputFormat  ff_amr_demuxer;
extern const FFOutputFormat ff_amr_muxer;
extern const FFInputFormat  ff_amrnb_demuxer;
extern const FFInputFormat  ff_amrwb_demuxer;
extern const FFOutputFormat ff_amv_muxer;
extern const FFInputFormat  ff_anm_demuxer;
extern const FFInputFormat  ff_apac_demuxer;
extern const FFInputFormat  ff_apc_demuxer;
extern const FFInputFormat  ff_ape_demuxer;
extern const FFInputFormat  ff_apm_demuxer;
extern const FFOutputFormat ff_apm_muxer;
extern const FFInputFormat  ff_apng_demuxer;
extern const FFOutputFormat ff_apng_muxer;
extern const FFInputFormat  ff_aptx_demuxer;
extern const FFOutputFormat ff_aptx_muxer;
extern const FFInputFormat  ff_aptx_hd_demuxer;
extern const FFOutputFormat ff_aptx_hd_muxer;
extern const FFInputFormat  ff_aqtitle_demuxer;
extern const FFInputFormat  ff_argo_asf_demuxer;
extern const FFOutputFormat ff_argo_asf_muxer;
extern const FFInputFormat  ff_argo_brp_demuxer;
extern const FFInputFormat  ff_argo_cvg_demuxer;
extern const FFOutputFormat ff_argo_cvg_muxer;
extern const FFInputFormat  ff_asf_demuxer;
extern const FFOutputFormat ff_asf_muxer;
extern const FFInputFormat  ff_asf_o_demuxer;
extern const FFInputFormat  ff_ass_demuxer;
extern const FFOutputFormat ff_ass_muxer;
extern const FFInputFormat  ff_ast_demuxer;
extern const FFOutputFormat ff_ast_muxer;
extern const FFOutputFormat ff_asf_stream_muxer;
extern const FFInputFormat  ff_au_demuxer;
extern const FFOutputFormat ff_au_muxer;
extern const FFInputFormat  ff_av1_demuxer;
extern const FFInputFormat  ff_avi_demuxer;
extern const FFOutputFormat ff_avi_muxer;
extern const FFOutputFormat ff_avif_muxer;
extern const FFInputFormat  ff_avisynth_demuxer;
extern const FFOutputFormat ff_avm2_muxer;
extern const FFInputFormat  ff_avr_demuxer;
extern const FFInputFormat  ff_avs_demuxer;
extern const FFInputFormat  ff_avs2_demuxer;
extern const FFOutputFormat ff_avs2_muxer;
extern const FFInputFormat  ff_avs3_demuxer;
extern const FFOutputFormat ff_avs3_muxer;
extern const FFInputFormat  ff_bethsoftvid_demuxer;
extern const FFInputFormat  ff_bfi_demuxer;
extern const FFInputFormat  ff_bintext_demuxer;
extern const FFInputFormat  ff_bink_demuxer;
extern const FFInputFormat  ff_binka_demuxer;
extern const FFInputFormat  ff_bit_demuxer;
extern const FFOutputFormat ff_bit_muxer;
extern const FFInputFormat  ff_bitpacked_demuxer;
extern const FFInputFormat  ff_bmv_demuxer;
extern const FFInputFormat  ff_bfstm_demuxer;
extern const FFInputFormat  ff_brstm_demuxer;
extern const FFInputFormat  ff_boa_demuxer;
extern const FFInputFormat  ff_bonk_demuxer;
extern const FFInputFormat  ff_c93_demuxer;
extern const FFInputFormat  ff_caf_demuxer;
extern const FFOutputFormat ff_caf_muxer;
extern const FFInputFormat  ff_cavsvideo_demuxer;
extern const FFOutputFormat ff_cavsvideo_muxer;
extern const FFInputFormat  ff_cdg_demuxer;
extern const FFInputFormat  ff_cdxl_demuxer;
extern const FFInputFormat  ff_cine_demuxer;
extern const FFInputFormat  ff_codec2_demuxer;
extern const FFOutputFormat ff_codec2_muxer;
extern const FFInputFormat  ff_codec2raw_demuxer;
extern const FFOutputFormat ff_codec2raw_muxer;
extern const FFInputFormat  ff_concat_demuxer;
extern const FFOutputFormat ff_crc_muxer;
extern const FFInputFormat  ff_dash_demuxer;
extern const FFOutputFormat ff_dash_muxer;
extern const FFInputFormat  ff_data_demuxer;
extern const FFOutputFormat ff_data_muxer;
extern const FFInputFormat  ff_daud_demuxer;
extern const FFOutputFormat ff_daud_muxer;
extern const FFInputFormat  ff_dcstr_demuxer;
extern const FFInputFormat  ff_derf_demuxer;
extern const FFInputFormat  ff_dfa_demuxer;
extern const FFInputFormat  ff_dfpwm_demuxer;
extern const FFOutputFormat ff_dfpwm_muxer;
extern const FFInputFormat  ff_dhav_demuxer;
extern const FFInputFormat  ff_dirac_demuxer;
extern const FFOutputFormat ff_dirac_muxer;
extern const FFInputFormat  ff_dnxhd_demuxer;
extern const FFOutputFormat ff_dnxhd_muxer;
extern const FFInputFormat  ff_dsf_demuxer;
extern const FFInputFormat  ff_dsicin_demuxer;
extern const FFInputFormat  ff_dss_demuxer;
extern const FFInputFormat  ff_dts_demuxer;
extern const FFOutputFormat ff_dts_muxer;
extern const FFInputFormat  ff_dtshd_demuxer;
extern const FFInputFormat  ff_dv_demuxer;
extern const FFOutputFormat ff_dv_muxer;
extern const FFInputFormat  ff_dvbsub_demuxer;
extern const FFInputFormat  ff_dvbtxt_demuxer;
extern const FFInputFormat  ff_dvdvideo_demuxer;
extern const FFInputFormat  ff_dxa_demuxer;
extern const FFInputFormat  ff_ea_demuxer;
extern const FFInputFormat  ff_ea_cdata_demuxer;
extern const FFInputFormat  ff_eac3_demuxer;
extern const FFOutputFormat ff_eac3_muxer;
extern const FFInputFormat  ff_epaf_demuxer;
extern const FFInputFormat  ff_evc_demuxer;
extern const FFOutputFormat ff_evc_muxer;
extern const FFOutputFormat ff_f4v_muxer;
extern const FFInputFormat  ff_ffmetadata_demuxer;
extern const FFOutputFormat ff_ffmetadata_muxer;
extern const FFOutputFormat ff_fifo_muxer;
extern const FFInputFormat  ff_filmstrip_demuxer;
extern const FFOutputFormat ff_filmstrip_muxer;
extern const FFInputFormat  ff_fits_demuxer;
extern const FFOutputFormat ff_fits_muxer;
extern const FFInputFormat  ff_flac_demuxer;
extern const FFOutputFormat ff_flac_muxer;
extern const FFInputFormat  ff_flic_demuxer;
extern const FFInputFormat  ff_flv_demuxer;
extern const FFOutputFormat ff_flv_muxer;
extern const FFInputFormat  ff_live_flv_demuxer;
extern const FFInputFormat  ff_fourxm_demuxer;
extern const FFOutputFormat ff_framecrc_muxer;
extern const FFOutputFormat ff_framehash_muxer;
extern const FFOutputFormat ff_framemd5_muxer;
extern const FFInputFormat  ff_frm_demuxer;
extern const FFInputFormat  ff_fsb_demuxer;
extern const FFInputFormat  ff_fwse_demuxer;
extern const FFInputFormat  ff_g722_demuxer;
extern const FFOutputFormat ff_g722_muxer;
extern const FFInputFormat  ff_g723_1_demuxer;
extern const FFOutputFormat ff_g723_1_muxer;
extern const FFInputFormat  ff_g726_demuxer;
extern const FFOutputFormat ff_g726_muxer;
extern const FFInputFormat  ff_g726le_demuxer;
extern const FFOutputFormat ff_g726le_muxer;
extern const FFInputFormat  ff_g729_demuxer;
extern const FFInputFormat  ff_gdv_demuxer;
extern const FFInputFormat  ff_genh_demuxer;
extern const FFInputFormat  ff_gif_demuxer;
extern const FFOutputFormat ff_gif_muxer;
extern const FFInputFormat  ff_gsm_demuxer;
extern const FFOutputFormat ff_gsm_muxer;
extern const FFInputFormat  ff_gxf_demuxer;
extern const FFOutputFormat ff_gxf_muxer;
extern const FFInputFormat  ff_h261_demuxer;
extern const FFOutputFormat ff_h261_muxer;
extern const FFInputFormat  ff_h263_demuxer;
extern const FFOutputFormat ff_h263_muxer;
extern const FFInputFormat  ff_h264_demuxer;
extern const FFOutputFormat ff_h264_muxer;
extern const FFOutputFormat ff_hash_muxer;
extern const FFInputFormat  ff_hca_demuxer;
extern const FFInputFormat  ff_hcom_demuxer;
extern const FFOutputFormat ff_hds_muxer;
extern const FFInputFormat  ff_hevc_demuxer;
extern const FFOutputFormat ff_hevc_muxer;
extern const FFInputFormat  ff_hls_demuxer;
extern const FFOutputFormat ff_hls_muxer;
extern const FFInputFormat  ff_hnm_demuxer;
extern const FFInputFormat  ff_iamf_demuxer;
extern const FFOutputFormat ff_iamf_muxer;
extern const FFInputFormat  ff_ico_demuxer;
extern const FFOutputFormat ff_ico_muxer;
extern const FFInputFormat  ff_idcin_demuxer;
extern const FFInputFormat  ff_idf_demuxer;
extern const FFInputFormat  ff_iff_demuxer;
extern const FFInputFormat  ff_ifv_demuxer;
extern const FFInputFormat  ff_ilbc_demuxer;
extern const FFOutputFormat ff_ilbc_muxer;
extern const FFInputFormat  ff_image2_demuxer;
extern const FFOutputFormat ff_image2_muxer;
extern const FFInputFormat  ff_image2pipe_demuxer;
extern const FFOutputFormat ff_image2pipe_muxer;
extern const FFInputFormat  ff_image2_alias_pix_demuxer;
extern const FFInputFormat  ff_image2_brender_pix_demuxer;
extern const FFInputFormat  ff_imf_demuxer;
extern const FFInputFormat  ff_ingenient_demuxer;
extern const FFInputFormat  ff_ipmovie_demuxer;
extern const FFOutputFormat ff_ipod_muxer;
extern const FFInputFormat  ff_ipu_demuxer;
extern const FFInputFormat  ff_ircam_demuxer;
extern const FFOutputFormat ff_ircam_muxer;
extern const FFOutputFormat ff_ismv_muxer;
extern const FFInputFormat  ff_iss_demuxer;
extern const FFInputFormat  ff_iv8_demuxer;
extern const FFInputFormat  ff_ivf_demuxer;
extern const FFOutputFormat ff_ivf_muxer;
extern const FFInputFormat  ff_ivr_demuxer;
extern const FFInputFormat  ff_jacosub_demuxer;
extern const FFOutputFormat ff_jacosub_muxer;
extern const FFInputFormat  ff_jv_demuxer;
extern const FFInputFormat  ff_jpegxl_anim_demuxer;
extern const FFInputFormat  ff_kux_demuxer;
extern const FFInputFormat  ff_kvag_demuxer;
extern const FFOutputFormat ff_kvag_muxer;
extern const FFInputFormat  ff_laf_demuxer;
extern const FFOutputFormat ff_latm_muxer;
extern const FFInputFormat  ff_lc3_demuxer;
extern const FFOutputFormat ff_lc3_muxer;
extern const FFInputFormat  ff_lmlm4_demuxer;
extern const FFInputFormat  ff_loas_demuxer;
extern const FFInputFormat  ff_luodat_demuxer;
extern const FFInputFormat  ff_lrc_demuxer;
extern const FFOutputFormat ff_lrc_muxer;
extern const FFInputFormat  ff_lvf_demuxer;
extern const FFInputFormat  ff_lxf_demuxer;
extern const FFInputFormat  ff_m4v_demuxer;
extern const FFOutputFormat ff_m4v_muxer;
extern const FFInputFormat  ff_mca_demuxer;
extern const FFInputFormat  ff_mcc_demuxer;
extern const FFOutputFormat ff_md5_muxer;
extern const FFInputFormat  ff_matroska_demuxer;
extern const FFOutputFormat ff_matroska_muxer;
extern const FFOutputFormat ff_matroska_audio_muxer;
extern const FFInputFormat  ff_mgsts_demuxer;
extern const FFInputFormat  ff_microdvd_demuxer;
extern const FFOutputFormat ff_microdvd_muxer;
extern const FFInputFormat  ff_mjpeg_demuxer;
extern const FFOutputFormat ff_mjpeg_muxer;
extern const FFInputFormat  ff_mjpeg_2000_demuxer;
extern const FFInputFormat  ff_mlp_demuxer;
extern const FFOutputFormat ff_mlp_muxer;
extern const FFInputFormat  ff_mlv_demuxer;
extern const FFInputFormat  ff_mm_demuxer;
extern const FFInputFormat  ff_mmf_demuxer;
extern const FFOutputFormat ff_mmf_muxer;
extern const FFInputFormat  ff_mods_demuxer;
extern const FFInputFormat  ff_moflex_demuxer;
extern const FFInputFormat  ff_mov_demuxer;
extern const FFOutputFormat ff_mov_muxer;
extern const FFOutputFormat ff_mp2_muxer;
extern const FFInputFormat  ff_mp3_demuxer;
extern const FFOutputFormat ff_mp3_muxer;
extern const FFOutputFormat ff_mp4_muxer;
extern const FFInputFormat  ff_mpc_demuxer;
extern const FFInputFormat  ff_mpc8_demuxer;
extern const FFOutputFormat ff_mpeg1system_muxer;
extern const FFOutputFormat ff_mpeg1vcd_muxer;
extern const FFOutputFormat ff_mpeg1video_muxer;
extern const FFOutputFormat ff_mpeg2dvd_muxer;
extern const FFOutputFormat ff_mpeg2svcd_muxer;
extern const FFOutputFormat ff_mpeg2video_muxer;
extern const FFOutputFormat ff_mpeg2vob_muxer;
extern const FFInputFormat  ff_mpegps_demuxer;
extern const FFInputFormat  ff_mpegts_demuxer;
extern const FFInputFormat  ff_mythtv_mpegts_demuxer;
extern const FFOutputFormat ff_mpegts_muxer;
extern const FFInputFormat  ff_mpegtsraw_demuxer;
extern const FFInputFormat  ff_mythtv_mpegtsraw_demuxer;
extern const FFInputFormat  ff_mpegvideo_demuxer;
extern const FFInputFormat  ff_mpjpeg_demuxer;
extern const FFOutputFormat ff_mpjpeg_muxer;
extern const FFInputFormat  ff_mpl2_demuxer;
extern const FFInputFormat  ff_mpsub_demuxer;
extern const FFInputFormat  ff_msf_demuxer;
extern const FFInputFormat  ff_msnwc_tcp_demuxer;
extern const FFInputFormat  ff_msp_demuxer;
extern const FFInputFormat  ff_mtaf_demuxer;
extern const FFInputFormat  ff_mtv_demuxer;
extern const FFInputFormat  ff_musx_demuxer;
extern const FFInputFormat  ff_mv_demuxer;
extern const FFInputFormat  ff_mvi_demuxer;
extern const FFInputFormat  ff_mxf_demuxer;
extern const FFOutputFormat ff_mxf_muxer;
extern const FFOutputFormat ff_mxf_d10_muxer;
extern const FFOutputFormat ff_mxf_opatom_muxer;
extern const FFInputFormat  ff_mxg_demuxer;
extern const FFInputFormat  ff_nc_demuxer;
extern const FFInputFormat  ff_nistsphere_demuxer;
extern const FFInputFormat  ff_nsp_demuxer;
extern const FFInputFormat  ff_nsv_demuxer;
extern const FFOutputFormat ff_null_muxer;
extern const FFInputFormat  ff_nut_demuxer;
extern const FFOutputFormat ff_nut_muxer;
extern const FFInputFormat  ff_nuv_demuxer;
extern const FFInputFormat  ff_obu_demuxer;
extern const FFOutputFormat ff_obu_muxer;
extern const FFOutputFormat ff_oga_muxer;
extern const FFInputFormat  ff_ogg_demuxer;
extern const FFOutputFormat ff_ogg_muxer;
extern const FFOutputFormat ff_ogv_muxer;
extern const FFInputFormat  ff_oma_demuxer;
extern const FFOutputFormat ff_oma_muxer;
extern const FFOutputFormat ff_opus_muxer;
extern const FFInputFormat  ff_osq_demuxer;
extern const FFInputFormat  ff_paf_demuxer;
extern const FFInputFormat  ff_pcm_alaw_demuxer;
extern const FFOutputFormat ff_pcm_alaw_muxer;
extern const FFInputFormat  ff_pcm_mulaw_demuxer;
extern const FFOutputFormat ff_pcm_mulaw_muxer;
extern const FFInputFormat  ff_pcm_vidc_demuxer;
extern const FFOutputFormat ff_pcm_vidc_muxer;
extern const FFInputFormat  ff_pcm_f64be_demuxer;
extern const FFOutputFormat ff_pcm_f64be_muxer;
extern const FFInputFormat  ff_pcm_f64le_demuxer;
extern const FFOutputFormat ff_pcm_f64le_muxer;
extern const FFInputFormat  ff_pcm_f32be_demuxer;
extern const FFOutputFormat ff_pcm_f32be_muxer;
extern const FFInputFormat  ff_pcm_f32le_demuxer;
extern const FFOutputFormat ff_pcm_f32le_muxer;
extern const FFInputFormat  ff_pcm_s32be_demuxer;
extern const FFOutputFormat ff_pcm_s32be_muxer;
extern const FFInputFormat  ff_pcm_s32le_demuxer;
extern const FFOutputFormat ff_pcm_s32le_muxer;
extern const FFInputFormat  ff_pcm_s24be_demuxer;
extern const FFOutputFormat ff_pcm_s24be_muxer;
extern const FFInputFormat  ff_pcm_s24le_demuxer;
extern const FFOutputFormat ff_pcm_s24le_muxer;
extern const FFInputFormat  ff_pcm_s16be_demuxer;
extern const FFOutputFormat ff_pcm_s16be_muxer;
extern const FFInputFormat  ff_pcm_s16le_demuxer;
extern const FFOutputFormat ff_pcm_s16le_muxer;
extern const FFInputFormat  ff_pcm_s8_demuxer;
extern const FFOutputFormat ff_pcm_s8_muxer;
extern const FFInputFormat  ff_pcm_u32be_demuxer;
extern const FFOutputFormat ff_pcm_u32be_muxer;
extern const FFInputFormat  ff_pcm_u32le_demuxer;
extern const FFOutputFormat ff_pcm_u32le_muxer;
extern const FFInputFormat  ff_pcm_u24be_demuxer;
extern const FFOutputFormat ff_pcm_u24be_muxer;
extern const FFInputFormat  ff_pcm_u24le_demuxer;
extern const FFOutputFormat ff_pcm_u24le_muxer;
extern const FFInputFormat  ff_pcm_u16be_demuxer;
extern const FFOutputFormat ff_pcm_u16be_muxer;
extern const FFInputFormat  ff_pcm_u16le_demuxer;
extern const FFOutputFormat ff_pcm_u16le_muxer;
extern const FFInputFormat  ff_pcm_u8_demuxer;
extern const FFOutputFormat ff_pcm_u8_muxer;
extern const FFInputFormat  ff_pdv_demuxer;
extern const FFInputFormat  ff_pjs_demuxer;
extern const FFInputFormat  ff_pmp_demuxer;
extern const FFInputFormat  ff_pp_bnk_demuxer;
extern const FFOutputFormat ff_psp_muxer;
extern const FFInputFormat  ff_pva_demuxer;
extern const FFInputFormat  ff_pvf_demuxer;
extern const FFInputFormat  ff_qcp_demuxer;
extern const FFInputFormat  ff_qoa_demuxer;
extern const FFInputFormat  ff_r3d_demuxer;
extern const FFInputFormat  ff_rawvideo_demuxer;
extern const FFOutputFormat ff_rawvideo_muxer;
extern const FFInputFormat  ff_rcwt_demuxer;
extern const FFOutputFormat ff_rcwt_muxer;
extern const FFInputFormat  ff_realtext_demuxer;
extern const FFInputFormat  ff_redspark_demuxer;
extern const FFInputFormat  ff_rka_demuxer;
extern const FFInputFormat  ff_rl2_demuxer;
extern const FFInputFormat  ff_rm_demuxer;
extern const FFOutputFormat ff_rm_muxer;
extern const FFInputFormat  ff_roq_demuxer;
extern const FFOutputFormat ff_roq_muxer;
extern const FFInputFormat  ff_rpl_demuxer;
extern const FFInputFormat  ff_rsd_demuxer;
extern const FFInputFormat  ff_rso_demuxer;
extern const FFOutputFormat ff_rso_muxer;
extern const FFInputFormat  ff_rtp_demuxer;
extern const FFOutputFormat ff_rtp_muxer;
extern const FFOutputFormat ff_rtp_mpegts_muxer;
extern const FFInputFormat  ff_rtsp_demuxer;
extern const FFOutputFormat ff_rtsp_muxer;
extern const FFInputFormat  ff_s337m_demuxer;
extern const FFInputFormat  ff_sami_demuxer;
extern const FFInputFormat  ff_sap_demuxer;
extern const FFOutputFormat ff_sap_muxer;
extern const FFInputFormat  ff_sbc_demuxer;
extern const FFOutputFormat ff_sbc_muxer;
extern const FFInputFormat  ff_sbg_demuxer;
extern const FFInputFormat  ff_scc_demuxer;
extern const FFOutputFormat ff_scc_muxer;
extern const FFInputFormat  ff_scd_demuxer;
extern const FFInputFormat  ff_sdns_demuxer;
extern const FFInputFormat  ff_sdp_demuxer;
extern const FFInputFormat  ff_sdr2_demuxer;
extern const FFInputFormat  ff_sds_demuxer;
extern const FFInputFormat  ff_sdx_demuxer;
extern const FFInputFormat  ff_segafilm_demuxer;
extern const FFOutputFormat ff_segafilm_muxer;
extern const FFOutputFormat ff_segment_muxer;
extern const FFOutputFormat ff_stream_segment_muxer;
extern const FFInputFormat  ff_ser_demuxer;
extern const FFInputFormat  ff_sga_demuxer;
extern const FFInputFormat  ff_shorten_demuxer;
extern const FFInputFormat  ff_siff_demuxer;
extern const FFInputFormat  ff_simbiosis_imx_demuxer;
extern const FFInputFormat  ff_sln_demuxer;
extern const FFInputFormat  ff_smacker_demuxer;
extern const FFInputFormat  ff_smjpeg_demuxer;
extern const FFOutputFormat ff_smjpeg_muxer;
extern const FFOutputFormat ff_smoothstreaming_muxer;
extern const FFInputFormat  ff_smush_demuxer;
extern const FFInputFormat  ff_sol_demuxer;
extern const FFInputFormat  ff_sox_demuxer;
extern const FFOutputFormat ff_sox_muxer;
extern const FFOutputFormat ff_spx_muxer;
extern const FFInputFormat  ff_spdif_demuxer;
extern const FFOutputFormat ff_spdif_muxer;
extern const FFInputFormat  ff_srt_demuxer;
extern const FFOutputFormat ff_srt_muxer;
extern const FFInputFormat  ff_str_demuxer;
extern const FFInputFormat  ff_stl_demuxer;
extern const FFOutputFormat ff_streamhash_muxer;
extern const FFInputFormat  ff_subviewer1_demuxer;
extern const FFInputFormat  ff_subviewer_demuxer;
extern const FFInputFormat  ff_sup_demuxer;
extern const FFOutputFormat ff_sup_muxer;
extern const FFInputFormat  ff_svag_demuxer;
extern const FFInputFormat  ff_svs_demuxer;
extern const FFInputFormat  ff_swf_demuxer;
extern const FFOutputFormat ff_swf_muxer;
extern const FFInputFormat  ff_tak_demuxer;
extern const FFOutputFormat ff_tee_muxer;
extern const FFInputFormat  ff_tedcaptions_demuxer;
extern const FFOutputFormat ff_tg2_muxer;
extern const FFOutputFormat ff_tgp_muxer;
extern const FFInputFormat  ff_thp_demuxer;
extern const FFInputFormat  ff_threedostr_demuxer;
extern const FFInputFormat  ff_tiertexseq_demuxer;
extern const FFOutputFormat ff_mkvtimestamp_v2_muxer;
extern const FFInputFormat  ff_tmv_demuxer;
extern const FFInputFormat  ff_truehd_demuxer;
extern const FFOutputFormat ff_truehd_muxer;
extern const FFInputFormat  ff_tta_demuxer;
extern const FFOutputFormat ff_tta_muxer;
extern const FFOutputFormat ff_ttml_muxer;
extern const FFInputFormat  ff_txd_demuxer;
extern const FFInputFormat  ff_tty_demuxer;
extern const FFInputFormat  ff_ty_demuxer;
extern const FFOutputFormat ff_uncodedframecrc_muxer;
extern const FFInputFormat  ff_usm_demuxer;
extern const FFInputFormat  ff_v210_demuxer;
extern const FFInputFormat  ff_v210x_demuxer;
extern const FFInputFormat  ff_vag_demuxer;
extern const FFInputFormat  ff_vc1_demuxer;
extern const FFOutputFormat ff_vc1_muxer;
extern const FFInputFormat  ff_vc1t_demuxer;
extern const FFOutputFormat ff_vc1t_muxer;
extern const FFInputFormat  ff_vividas_demuxer;
extern const FFInputFormat  ff_vivo_demuxer;
extern const FFInputFormat  ff_vmd_demuxer;
extern const FFInputFormat  ff_vobsub_demuxer;
extern const FFInputFormat  ff_voc_demuxer;
extern const FFOutputFormat ff_voc_muxer;
extern const FFInputFormat  ff_vpk_demuxer;
extern const FFInputFormat  ff_vplayer_demuxer;
extern const FFInputFormat  ff_vqf_demuxer;
extern const FFInputFormat  ff_vvc_demuxer;
extern const FFOutputFormat ff_vvc_muxer;
extern const FFInputFormat  ff_w64_demuxer;
extern const FFOutputFormat ff_w64_muxer;
extern const FFInputFormat  ff_wady_demuxer;
extern const FFInputFormat  ff_wavarc_demuxer;
extern const FFInputFormat  ff_wav_demuxer;
extern const FFOutputFormat ff_wav_muxer;
extern const FFInputFormat  ff_wc3_demuxer;
extern const FFOutputFormat ff_webm_muxer;
extern const FFInputFormat  ff_webm_dash_manifest_demuxer;
extern const FFOutputFormat ff_webm_dash_manifest_muxer;
extern const FFOutputFormat ff_webm_chunk_muxer;
extern const FFOutputFormat ff_webp_muxer;
extern const FFInputFormat  ff_webvtt_demuxer;
extern const FFOutputFormat ff_webvtt_muxer;
extern const FFInputFormat  ff_wsaud_demuxer;
extern const FFOutputFormat ff_wsaud_muxer;
extern const FFInputFormat  ff_wsd_demuxer;
extern const FFInputFormat  ff_wsvqa_demuxer;
extern const FFInputFormat  ff_wtv_demuxer;
extern const FFOutputFormat ff_wtv_muxer;
extern const FFInputFormat  ff_wve_demuxer;
extern const FFInputFormat  ff_wv_demuxer;
extern const FFOutputFormat ff_wv_muxer;
extern const FFInputFormat  ff_xa_demuxer;
extern const FFInputFormat  ff_xbin_demuxer;
extern const FFInputFormat  ff_xmd_demuxer;
extern const FFInputFormat  ff_xmv_demuxer;
extern const FFInputFormat  ff_xvag_demuxer;
extern const FFInputFormat  ff_xwma_demuxer;
extern const FFInputFormat  ff_yop_demuxer;
extern const FFInputFormat  ff_yuv4mpegpipe_demuxer;
extern const FFOutputFormat ff_yuv4mpegpipe_muxer;
/* image demuxers */
extern const FFInputFormat  ff_image_bmp_pipe_demuxer;
extern const FFInputFormat  ff_image_cri_pipe_demuxer;
extern const FFInputFormat  ff_image_dds_pipe_demuxer;
extern const FFInputFormat  ff_image_dpx_pipe_demuxer;
extern const FFInputFormat  ff_image_exr_pipe_demuxer;
extern const FFInputFormat  ff_image_gem_pipe_demuxer;
extern const FFInputFormat  ff_image_gif_pipe_demuxer;
extern const FFInputFormat  ff_image_hdr_pipe_demuxer;
extern const FFInputFormat  ff_image_j2k_pipe_demuxer;
extern const FFInputFormat  ff_image_jpeg_pipe_demuxer;
extern const FFInputFormat  ff_image_jpegls_pipe_demuxer;
extern const FFInputFormat  ff_image_jpegxl_pipe_demuxer;
extern const FFInputFormat  ff_image_pam_pipe_demuxer;
extern const FFInputFormat  ff_image_pbm_pipe_demuxer;
extern const FFInputFormat  ff_image_pcx_pipe_demuxer;
extern const FFInputFormat  ff_image_pfm_pipe_demuxer;
extern const FFInputFormat  ff_image_pgmyuv_pipe_demuxer;
extern const FFInputFormat  ff_image_pgm_pipe_demuxer;
extern const FFInputFormat  ff_image_pgx_pipe_demuxer;
extern const FFInputFormat  ff_image_phm_pipe_demuxer;
extern const FFInputFormat  ff_image_photocd_pipe_demuxer;
extern const FFInputFormat  ff_image_pictor_pipe_demuxer;
extern const FFInputFormat  ff_image_png_pipe_demuxer;
extern const FFInputFormat  ff_image_ppm_pipe_demuxer;
extern const FFInputFormat  ff_image_psd_pipe_demuxer;
extern const FFInputFormat  ff_image_qdraw_pipe_demuxer;
extern const FFInputFormat  ff_image_qoi_pipe_demuxer;
extern const FFInputFormat  ff_image_sgi_pipe_demuxer;
extern const FFInputFormat  ff_image_svg_pipe_demuxer;
extern const FFInputFormat  ff_image_sunrast_pipe_demuxer;
extern const FFInputFormat  ff_image_tiff_pipe_demuxer;
extern const FFInputFormat  ff_image_vbn_pipe_demuxer;
extern const FFInputFormat  ff_image_webp_pipe_demuxer;
extern const FFInputFormat  ff_image_xbm_pipe_demuxer;
extern const FFInputFormat  ff_image_xpm_pipe_demuxer;
extern const FFInputFormat  ff_image_xwd_pipe_demuxer;

/* external libraries */
extern const FFOutputFormat ff_chromaprint_muxer;
extern const FFInputFormat  ff_libgme_demuxer;
extern const FFInputFormat  ff_libmodplug_demuxer;
extern const FFInputFormat  ff_libopenmpt_demuxer;
extern const FFInputFormat  ff_vapoursynth_demuxer;

#include "libavformat/muxer_list.c"
#include "libavformat/demuxer_list.c"

static atomic_uintptr_t indev_list_intptr  = 0;
static atomic_uintptr_t outdev_list_intptr = 0;

const AVOutputFormat *av_muxer_iterate(void **opaque)
{
    static const uintptr_t size = sizeof(muxer_list)/sizeof(muxer_list[0]) - 1;
    uintptr_t i = (uintptr_t)*opaque;
    const FFOutputFormat *f = NULL;
    uintptr_t tmp;

    if (i < size) {
        f = muxer_list[i];
    } else if (tmp = atomic_load_explicit(&outdev_list_intptr, memory_order_relaxed)) {
        const FFOutputFormat *const *outdev_list = (const FFOutputFormat *const *)tmp;
        f = outdev_list[i - size];
    }

    if (f) {
        *opaque = (void*)(i + 1);
        return &f->p;
    }
    return NULL;
}

const AVInputFormat *av_demuxer_iterate(void **opaque)
{
    static const uintptr_t size = sizeof(demuxer_list)/sizeof(demuxer_list[0]) - 1;
    uintptr_t i = (uintptr_t)*opaque;
    const FFInputFormat *f = NULL;
    uintptr_t tmp;

    if (i < size) {
        f = demuxer_list[i];
    } else if (tmp = atomic_load_explicit(&indev_list_intptr, memory_order_relaxed)) {
        const FFInputFormat *const *indev_list = (const FFInputFormat *const *)tmp;
        f = indev_list[i - size];
    }

    if (f) {
        *opaque = (void*)(i + 1);
        return &f->p;
    }
    return NULL;
}

void avpriv_register_devices(const FFOutputFormat * const o[], const FFInputFormat * const i[])
{
    atomic_store_explicit(&outdev_list_intptr, (uintptr_t)o, memory_order_relaxed);
    atomic_store_explicit(&indev_list_intptr,  (uintptr_t)i, memory_order_relaxed);
}
