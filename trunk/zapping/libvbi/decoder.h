/*
 *  Raw VBI Decoder
 *
 *  Copyright (C) 2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: decoder.h,v 1.1 2000-12-11 04:13:38 mschimek Exp $ */

/*
    Only device specific code includes this file.
 */

#include "../common/types.h"
#include "sliced.h"

#define MOD_NRZ_LSB_ENDIAN	0
#define MOD_NRZ_MSB_ENDIAN	1
#define MOD_BIPHASE_LSB_ENDIAN	2
#define MOD_BIPHASE_MSB_ENDIAN	3

struct bit_slicer {

	/* private */

	unsigned int	cri;
	unsigned int	cri_mask;
	int		thresh;
	int		cri_bytes;
	int		cri_rate;
	int		oversampling_rate;
	int		phase_shift;
	int		step;
	unsigned int	frc;
	int		frc_bits;
	int		payload;
	int		lsb_endian;
};

extern void		init_bit_slicer(struct bit_slicer *d, int raw_bytes, int sampling_rate, int cri_rate, int bit_rate, unsigned int cri_frc, int cri_bits, int frc_bits, int payload, int modulation);
extern bool		bit_slicer(struct bit_slicer *d, unsigned char *raw, unsigned char *buf);

/* Data services */

struct vbi_decoder {

	/* sampling parameters */

	int			scanning;		/* 525, 625 */
	int			sampling_rate;		/* Hz */
	int			samples_per_line;
	int			offset;			/* 0H, samples */
	int			start[2];		/* ITU-T numbering */
	int			count[2];		/* field lines */
	bool			interlaced;
	bool			synchronous;

	/* private */

#define MAX_JOBS		8
#define MAX_WAYS		4

	unsigned int		services;
	int			num_jobs;

	char *			pattern;
	struct vbi_decoder_job {
		unsigned int		id;
		int			offset;
		struct bit_slicer 	slicer;
	}			jobs[MAX_JOBS];
};

extern void		init_vbi_decoder(struct vbi_decoder *);
extern void		uninit_vbi_decoder(struct vbi_decoder *);
extern unsigned int	add_vbi_services(struct vbi_decoder *, unsigned int services, int strict);
extern unsigned int	remove_vbi_services(struct vbi_decoder *, unsigned int services);
extern void		reset_vbi_decoder(struct vbi_decoder *);
extern unsigned int	qualify_vbi_sampling(struct vbi_decoder *, int *max_rate, unsigned int services);
extern int		vbi_decoder(struct vbi_decoder *vbi, unsigned char *raw, vbi_sliced *out);
