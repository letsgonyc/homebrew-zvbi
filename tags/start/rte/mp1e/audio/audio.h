/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: audio.h,v 1.1.1.1 2001-08-07 22:09:51 garetxe Exp $ */

#include <pthread.h>
#include "../common/fifo.h"
#include "../common/bstream.h"
#include "mpeg.h"

struct pcm_context {
	fifo2		fifo;
	producer	producer;

	int		sampling_rate;
	bool		stereo;
};

#define BLKSIZE 1024
#define HBLKSIZE 513
#define CBANDS 63
#define e_save_new e_save_oldest

extern struct audio_seg
{
	struct bs_rec		out;

	/* Buffers */

	int			sb_samples[2][3][12][SBLIMIT];	// channel, scale group, sample
	char			bit_alloc[2][SBLIMIT];
	struct {
		unsigned char		mant, exp;
	}			scalar[2][3][SBLIMIT];		// channel, scale group
	union {
		unsigned int		scf[2][SBLIMIT];	// scaling factors
		mmx_t			fb_temp[17];		// filterbank temp
	}			sf;
	char			scfsi[2][SBLIMIT];		// scale factor selector information
	float			mnr[2][SBLIMIT];
	int			sblimit;
	int			sum_nbal;

	/* Tables */

	unsigned char		sb_group[SBLIMIT];
	unsigned char		bits[4][MAX_BA_INDICES];
	struct {
		unsigned char		quant, shift;
	}			qs[4][MAX_BA_INDICES];
	unsigned char		nbal[SBLIMIT];
	unsigned short		bit_incr[8][MAX_BA_INDICES];
	float			mnr_incr[8][SBLIMIT];
	unsigned char		steps[4];
	unsigned char		packed[4];

	/* Psycho buffers */

	FLOAT			h_save[2][3][BLKSIZE];
	FLOAT			e_save[2][2][HBLKSIZE];
	struct {
		FLOAT			e, c;
	}			grouped[CBANDS];
	FLOAT			nb[HBLKSIZE];
	FLOAT			sum_energy[SBLIMIT];

	/* Psycho tables */

	char			partition[HBLKSIZE];		// frequency line to cband mapping
	struct {
		char			off, cnt;
	}			s_limits[CBANDS];
	FLOAT			s_packed[2048];			// packed spreading function
	FLOAT			absthres[HBLKSIZE];		// absolute thresholds, linear
	FLOAT			xnorm[CBANDS];
	FLOAT			p1[CBANDS];
	FLOAT			p2[CBANDS];
	FLOAT			p3[CBANDS - 20];
	FLOAT			p4[CBANDS - 20];

	int			ch;
	FLOAT *			e_save_old;
	FLOAT *			e_save_oldest;
	FLOAT *			h_save_new;
	FLOAT *			h_save_old;
	FLOAT *			h_save_oldest;
	int			psycho_loops;

	unsigned char		wrap[(SAMPLES_PER_FRAME + 512 - 32) * 4]
		__attribute__ ((aligned (CACHE_LINE)));

	/* Misc */

	consumer		cons;
	buffer2 *		ibuf;
	unsigned char		*p, *o;
	int			left, offs;
	double			time;

	unsigned int		header_template;
	double			frame_period;
	int			sampling_freq;
	int			bits_per_frame;
	int			spf_rest, spf_lag;

	int			audio_frame_count;
} aseg;

/* psycho.c */

extern void		psycho_init(struct audio_seg *, int, int);
extern void		psycho(struct audio_seg *, short *, float *, int);

/* fft.c */

extern void		fft_step_1(short *, FLOAT *);
extern void		fft_step_2(short *, FLOAT *);

/* filter.c */

extern void		subband_filter_init(struct audio_seg *mp2);
extern void		mmx_window_mono(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mmx_window_left(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mmx_window_right(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mmx_filterbank(int *, mmx_t *) __attribute__ ((regparm (2)));

static inline void
mmx_filter_mono(struct audio_seg *mp2, short *p, int *samples)
{
	int j;

	mp2->sf.fb_temp[0].d[0] = 0L;
	mp2->sf.fb_temp[0].d[1] = -1L;
	mp2->sf.fb_temp[1].d[0] = 32768L;
	mp2->sf.fb_temp[1].d[1] = 32768L;

	for (j = 0; j < 3 * SCALE_BLOCK; j++, p += 32, samples += 32) {
		mmx_window_mono(p, mp2->sf.fb_temp);
		mmx_filterbank(samples, mp2->sf.fb_temp);
	}

	emms();
}

static inline void
mmx_filter_stereo(struct audio_seg *mp2, short *p, int *samples)
{
	int j;

	mp2->sf.fb_temp[0].d[0] = 0L;
	mp2->sf.fb_temp[0].d[1] = -1L;
	mp2->sf.fb_temp[1].d[0] = 32768L;
	mp2->sf.fb_temp[1].d[1] = 32768L;

	for (j = 0; j < 3 * SCALE_BLOCK; j++, p += 32 * 2, samples += 32) {
		/*
		 *  Subband window code could be optimized,
		 *  I've just adapted the mono version.
		 */
		mmx_window_left(p, mp2->sf.fb_temp);
		mmx_filterbank(samples, mp2->sf.fb_temp);

		mmx_window_right(p, mp2->sf.fb_temp);
		mmx_filterbank(samples + 3 * SCALE_BLOCK * 32, mp2->sf.fb_temp);
	}

	emms();
}