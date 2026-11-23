/*
 * Copyright (C) 2000, 2001, 2002 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                                Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBDVDREAD_IFO_READ_H
#define LIBDVDREAD_IFO_READ_H

#include <dvdread/ifo_types.h>
#include <dvdread/attributes.h>
#include <dvdread/dvd_reader.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * handle = ifoOpen(dvd, title);
 *
 * Opens an IFO and reads in all the data for the IFO file corresponding to the
 * given title.  If title 0 is given, the video manager IFO file is read.
 * Returns a handle to a completely parsed structure.
 */
DVDREAD_API ifo_handle_t *ifoOpen(dvd_reader_t *, int );

/**
 * handle = ifoOpenVMGI(dvd);
 *
 * Opens an IFO and reads in _only_ the vmgi_mat data.  This call can be used
 * together with the calls below to read in each segment of the IFO file on
 * demand. If the dvd_reader opened an DVD-Audio Disc, this will open the AMGI
 * if the dvd_reader opened a DVD-VR disc, this will open the RTAV_VMGI
 */
DVDREAD_API ifo_handle_t *ifoOpenVMGI(dvd_reader_t *);

/**
 * handle = ifoOpenVTSI(dvd, title);
 *
 * Opens an IFO and reads in _only_ the vtsi_mat data.  This call can be used
 * together with the calls below to read in each segment of the IFO file on
 * demand. If the dvd_reader opened an DVD-Audio Disc, this will open the ATSI
 */
DVDREAD_API ifo_handle_t *ifoOpenVTSI(dvd_reader_t *, int);

/**
 * ifoClose(ifofile);
 * Cleans up the IFO information.  This will free all data allocated for the
 * substructures.
 */
DVDREAD_API void ifoClose(ifo_handle_t *);

/**
 * The following functions are for reading only part of the VMGI/VTSI files.
 * Returns 1 if the data was successfully read and 0 on error.
 */

/**
 * okay = ifoRead_PLT_MAIT(ifofile);
 *
 * Read in the Parental Management Information table, filling the
 * ifofile->ptl_mait structure and its substructures.  This data is only
 * located in the video manager information file.  This fills the
 * ifofile->ptl_mait structure and all its substructures.
 */
DVDREAD_API int ifoRead_PTL_MAIT(ifo_handle_t *);

/**
 * okay = ifoRead_VTS_ATRT(ifofile);
 *
 * Read in the attribute table for the main menu vob, filling the
 * ifofile->vts_atrt structure and its substructures.  Only located in the
 * video manager information file.  This fills in the ifofile->vts_atrt
 * structure and all its substructures.
 */
DVDREAD_API int ifoRead_VTS_ATRT(ifo_handle_t *);

/**
 * okay = ifoRead_TT_SRPT(ifofile);
 *
 * Reads the title info for the main menu, filling the ifofile->tt_srpt
 * structure and its substructures.  This data is only located in the video
 * manager information file.  This structure is mandatory in the IFO file.
 */
DVDREAD_API int ifoRead_TT_SRPT(ifo_handle_t *);

/**
 * okay = ifoRead_VTS_PTT_SRPT(ifofile);
 *
 * Reads in the part of title search pointer table, filling the
 * ifofile->vts_ptt_srpt structure and its substructures.  This data is only
 * located in the video title set information file.  This structure is
 * mandatory, and must be included in the VTSI file.
 */
DVDREAD_API int ifoRead_VTS_PTT_SRPT(ifo_handle_t *);

/**
 * okay = ifoRead_FP_PGC(ifofile);
 *
 * Reads in the first play program chain data, filling the
 * ifofile->first_play_pgc structure.  This data is only located in the video
 * manager information file (VMGI).  This structure is optional.
 */
DVDREAD_API int ifoRead_FP_PGC(ifo_handle_t *);

/**
 * okay = ifoRead_PGCIT(ifofile);
 *
 * Reads in the program chain information table for the video title set.  Fills
 * in the ifofile->vts_pgcit structure and its substructures, which includes
 * the data for each program chain in the set.  This data is only located in
 * the video title set information file.  This structure is mandatory, and must
 * be included in the VTSI file.
 */
DVDREAD_API int ifoRead_PGCIT(ifo_handle_t *);

/**
 * okay = ifoRead_PGCI_UT(ifofile);
 *
 * Reads in the menu PGCI unit table for the menu VOB.  For the video manager,
 * this corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgi_pgci_ut structure and all its substructures.  For
 * VTSI files, this fills the ifofile->vtsm_pgci_ut structure.
 */
DVDREAD_API int ifoRead_PGCI_UT(ifo_handle_t *);

/**
 * okay = ifoRead_VTS_TMAPT(ifofile);
 *
 * Reads in the VTS Time Map Table, this data is only located in the video
 * title set information file.  This fills the ifofile->vts_tmapt structure
 * and all its substructures.  When present enables VOBU level time-based
 * seeking for One_Sequential_PGC_Titles.
 */
DVDREAD_API int ifoRead_VTS_TMAPT(ifo_handle_t *);

/**
 * okay = ifoRead_C_ADT(ifofile);
 *
 * Reads in the cell address table for the menu VOB.  For the video manager,
 * this corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgm_c_adt structure and all its substructures.  For VTSI
 * files, this fills the ifofile->vtsm_c_adt structure.
 */
DVDREAD_API int ifoRead_C_ADT(ifo_handle_t *);

/**
 * okay = ifoRead_TITLE_C_ADT(ifofile);
 *
 * Reads in the cell address table for the video title set corresponding to
 * this IFO file.  This data is only located in the video title set information
 * file.  This structure is mandatory, and must be included in the VTSI file.
 * This call fills the ifofile->vts_c_adt structure and its substructures.
 */
DVDREAD_API int ifoRead_TITLE_C_ADT(ifo_handle_t *);

/**
 * okay = ifoRead_VOBU_ADMAP(ifofile);
 *
 * Reads in the VOBU address map for the menu VOB.  For the video manager, this
 * corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgm_vobu_admap structure and all its substructures.  For
 * VTSI files, this fills the ifofile->vtsm_vobu_admap structure.
 */
DVDREAD_API int ifoRead_VOBU_ADMAP(ifo_handle_t *);

/**
 * okay = ifoRead_TITLE_VOBU_ADMAP(ifofile);
 *
 * Reads in the VOBU address map for the associated video title set.  This data
 * is only located in the video title set information file.  This structure is
 * mandatory, and must be included in the VTSI file.  Fills the
 * ifofile->vts_vobu_admap structure and its substructures.
 */
DVDREAD_API int ifoRead_TITLE_VOBU_ADMAP(ifo_handle_t *);

/**
 * okay = ifoRead_TXTDT_MGI(ifofile);
 *
 * Reads in the text data strings for the DVD.  Fills the ifofile->txtdt_mgi
 * structure and all its substructures.  This data is only located in the video
 * manager information file.  This structure is mandatory, and must be included
 * in the VMGI file.
 */
DVDREAD_API int ifoRead_TXTDT_MGI(ifo_handle_t *);

/**
 * okay = ifoRead_TT(ifofile);
 *
 * Reads the Title Track Table in ATS IFO's.
 * This structure. This structure is mandatory, and must be included
 * in the AMGI file.
 */
DVDREAD_API int ifoRead_TT(ifo_handle_t *);

/**
 * okay = ifoRead_TIF(ifofile, table_nr);
 *
 * Reads one of the two title search pointer tables of AUDIO_TS.IFO.
 * Table 1 is the ATT_SRPT, which on a hybrid disc also lists the titles in
 * the video zone. Table 2 is the AOTT_SRPT, which lists audio zone titles only.
 * Without video titles both tables list the same titles. Both tables are
 * mandatory in the AMGI file.
 */
DVDREAD_API int ifoRead_TIF(ifo_handle_t *, int);

/**
 * handle = ifoOpen_SAMG(ifofile);
 *
 * the SAMG menu contains all the information for all the ATS and AMG
 * But only for audio titles, and without any menu information
 * is loaded in ifoOpen as a part of the audio manager
 * Can be for simple audio playback
 *
 */
DVDREAD_API ifo_handle_t *ifoOpenSAMG(dvd_reader_t *ctx);

/**
 * handle = ifoOpenASVS(ifofile);
 *
 * the ASVS ifo contains the sector addresses of each still frame
 * in Audio_sv.vob, to access audio_sv.vob load menu > 0 in dvdopenvob
 * is loaded in ifoOpen as a part of the audio manager
 *
 */
DVDREAD_API ifo_handle_t *ifoOpenASVS(dvd_reader_t *ctx);

/**
 * okay = ifoRead_PGCI(ifofile);
 *
 * Reads the Program Chain Information Table (pgci).
 * This structure contains the technical definitions for the recordings, including
 * video attributes (aspect ratio, resolution), audio stream counts, and formats.
 * Use this data to configure the decoder element (ES format) before playback starts.
 */
DVDREAD_API int ifoRead_PGCI(ifo_handle_t *ifofile);

/**
 * okay = ifoRead_PGC_GI(ifofile);
 *
 * Reads the Program Chain General Information (pgc_gi).
 * This is the master database of all physical video clips ("Programs") on the disc.
 * It provides the raw byte offsets, durations, and timestamps (PGTM) for every
 * recording. This is required to locate content sectors and to generate "Title 0"
 * (the raw/original timeline).
 */
DVDREAD_API int ifoRead_PGC_GI(ifo_handle_t *ifofile);

/**
 * okay = ifoRead_UD_PGCIT(ifofile);
 *
 * Reads the Program Set General Information (ud_pgcit).
 * This defines the user-created "Titles" (Playlists) and their text labels.
 * It maps logical groupings of content to the physical programs found in pgc_gi.
 * Use this to build the main navigation menu and "Next/Prev" chapter logic
 * intended by the user.
 */
DVDREAD_API int ifoRead_UD_PGCIT(ifo_handle_t *ifofile);

/**
 * The following functions are used for freeing parsed sections of the
 * ifo_handle_t structure and the allocated substructures.  The free calls
 * below are safe:  they will not mind if you attempt to free part of an IFO
 * file which was not read in or which does not exist.
 */
DVDREAD_API void ifoFree_PTL_MAIT(ifo_handle_t *);
DVDREAD_API void ifoFree_VTS_ATRT(ifo_handle_t *);
DVDREAD_API void ifoFree_TT_SRPT(ifo_handle_t *);
DVDREAD_API void ifoFree_VTS_PTT_SRPT(ifo_handle_t *);
DVDREAD_API void ifoFree_FP_PGC(ifo_handle_t *);
DVDREAD_API void ifoFree_PGCIT(ifo_handle_t *);
DVDREAD_API void ifoFree_PGCI_UT(ifo_handle_t *);
DVDREAD_API void ifoFree_VTS_TMAPT(ifo_handle_t *);
DVDREAD_API void ifoFree_C_ADT(ifo_handle_t *);
DVDREAD_API void ifoFree_TITLE_C_ADT(ifo_handle_t *);
DVDREAD_API void ifoFree_VOBU_ADMAP(ifo_handle_t *);
DVDREAD_API void ifoFree_TITLE_VOBU_ADMAP(ifo_handle_t *);
DVDREAD_API void ifoFree_TXTDT_MGI(ifo_handle_t *);
DVDREAD_API void ifoFree_TT(ifo_handle_t *);
DVDREAD_API void ifoFree_PGC_GI(ifo_handle_t *);
DVDREAD_API void ifoFree_UD_PGCIT(ifo_handle_t *);

#ifdef __cplusplus
};
#endif
#endif /* LIBDVDREAD_IFO_READ_H */
