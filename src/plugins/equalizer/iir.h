/*
 *   PCM time-domain equalizer
 *
 *   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users.sourceforge.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: iir.h,v 1.13 2006/01/15 00:26:32 liebremx Exp $
 */
#ifndef IIR_H
#define IIR_H

#include <string.h>
#include "iir_cfs.h"

/*
 * Flush-to-zero to avoid flooding the CPU with underflow exceptions 
 */
#ifdef SSE_MATH
#define FTZ 0x8000
#define FTZ_ON { \
    unsigned int mxcsr; \
  __asm__  __volatile__ ("stmxcsr %0" : "=m" (*&mxcsr)); \
  mxcsr |= FTZ; \
  __asm__  __volatile__ ("ldmxcsr %0" : : "m" (*&mxcsr)); \
}
#define FTZ_OFF { \
    unsigned int mxcsr; \
  __asm__  __volatile__ ("stmxcsr %0" : "=m" (*&mxcsr)); \
  mxcsr &= ~FTZ; \
  __asm__  __volatile__ ("ldmxcsr %0" : : "m" (*&mxcsr)); \
}
#else
#define FTZ_ON
#define FTZ_OFF
#endif

/*
 * Function prototypes
 */
void init_iir(void);
void config_iir(int srate, int bands, int original);
void clean_history(void);
void set_gain(int index, int chn, float val);
void set_preamp(int chn, float val);


__inline__ int iir(void *d, int length, int nch, int extra_filtering);

#ifdef ARCH_X86
__inline__ int round_trick(float floatvalue_to_round);
#endif
#ifdef ARCH_PPC
__inline__ int round_ppc(float x);
#endif

#define EQ_CHANNELS 2
#define EQ_MAX_BANDS 31

extern float preamp[EQ_CHANNELS];
extern sIIRCoefficients *iir_cf;
extern int rate;
extern int band_count;

#ifdef BENCHMARK
extern double timex;
extern int count;
extern unsigned int blength;
#endif

#endif /* #define IIR_H */

