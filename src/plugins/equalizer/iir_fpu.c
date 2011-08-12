/*
 *   PCM time-domain equalizer
 *
 *   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users sourceforge net>
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
 *   $Id: iir_fpu.c,v 1.4 2006/01/15 00:26:32 liebremx Exp $
 */

#include <stdlib.h>
#include "iir.h"
#include "iir_fpu.h"

#ifdef BENCHMARK
#include <stdio.h>
#include "benchmark.h"
#endif /* BENCHMARK */

static sXYData data_history[EQ_MAX_BANDS][EQ_CHANNELS] __attribute__((aligned));
static sXYData data_history2[EQ_MAX_BANDS][EQ_CHANNELS] __attribute__((aligned));
float gain[EQ_MAX_BANDS][EQ_CHANNELS] __attribute__((aligned));
/* random noise */
sample_t dither[256];
int di;

void set_gain(int index, int chn, float val)
{
  gain[index][chn] = val;
}

void clean_history(void)
{
  int n;
  /* Zero the history arrays */
  memset(data_history, 0, sizeof(sXYData) * EQ_MAX_BANDS * EQ_CHANNELS);
  memset(data_history2, 0, sizeof(sXYData) * EQ_MAX_BANDS * EQ_CHANNELS);
  /* this is only needed if we use fpu code and there's no other place for
  the moment to init the dither array*/
  for (n = 0; n < 256; n++) {
      dither[n] = (rand() % 4) - 2;
  }
  di = 0;
}

__inline__ int iir(void *d, int length, int nch, int extra_filtering)
{
/*  FTZ_ON; */
  short *data = d;
  /* Indexes for the history arrays
   * These have to be kept between calls to this function
   * hence they are static */
  static int i = 2, j = 1, k = 0;	

  int index, band, channel;
  int tempint, halflength;
  sample_t out[EQ_CHANNELS], pcm[EQ_CHANNELS];

#ifdef BENCHMARK
  start_counter();
#endif /* BENCHMARK */

  /**
   * IIR filter equation is
   * y[n] = 2 * (alpha*(x[n]-x[n-2]) + gamma*y[n-1] - beta*y[n-2])
   *
   * NOTE: The 2 factor was introduced in the coefficients to save
   * 			a multiplication
   *
   * This algorithm cascades two filters to get nice filtering
   * at the expense of extra CPU cycles
   */
  /* 16bit, 2 bytes per sample, so divide by two the length of
   * the buffer (length is in bytes)
   */
  halflength = (length >> 1);
  for (index = 0; index < halflength; index+=nch)
  {
    /* For each channel */
    for (channel = 0; channel < nch; channel++)
    {
      pcm[channel] = data[index+channel];
      /* Preamp gain */
      pcm[channel] *= preamp[channel];

      /* add random noise */
      pcm[channel] += dither[di];

      out[channel] = 0.;
      /* For each band */
      for (band = 0; band < band_count; band++)
      {
        /* Store Xi(n) */
        data_history[band][channel].x[i] = pcm[channel];
        /* Calculate and store Yi(n) */
        data_history[band][channel].y[i] =
          (
           /* 		= alpha * [x(n)-x(n-2)] */
           iir_cf[band].alpha * ( data_history[band][channel].x[i]
             -  data_history[band][channel].x[k])
           /* 		+ gamma * y(n-1) */
           + iir_cf[band].gamma * data_history[band][channel].y[j]
           /* 		- beta * y(n-2) */
           - iir_cf[band].beta * data_history[band][channel].y[k]
          );
        /*
         * The multiplication by 2.0 was 'moved' into the coefficients to save
         * CPU cycles here */
        /* Apply the gain  */
        out[channel] +=  data_history[band][channel].y[i]*gain[band][channel]; /* * 2.0; */
      } /* For each band */

      if (extra_filtering)
      {
        /* Filter the sample again */
        for (band = 0; band < band_count; band++)
        {
          /* Store Xi(n) */
          data_history2[band][channel].x[i] = out[channel];
          /* Calculate and store Yi(n) */
          data_history2[band][channel].y[i] =
            (
             /* y(n) = alpha * [x(n)-x(n-2)] */
             iir_cf[band].alpha * (data_history2[band][channel].x[i]
               -  data_history2[band][channel].x[k])
             /* 	    + gamma * y(n-1) */
             + iir_cf[band].gamma * data_history2[band][channel].y[j]
             /* 		- beta * y(n-2) */
             - iir_cf[band].beta * data_history2[band][channel].y[k]
            );
          /* Apply the gain */
          out[channel] +=  data_history2[band][channel].y[i]*gain[band][channel];
        } /* For each band */
      }

      /* Volume stuff
         Scale down original PCM sample and add it to the filters
         output. This substitutes the multiplication by 0.25
         Go back to use the floating point multiplication before the
         conversion to give more dynamic range
         */
      out[channel] += pcm[channel]*0.25;

      /* remove random noise */
      out[channel] -= dither[di]*0.25;

      /* Round and convert to integer */
#ifdef ARCH_PPC
      tempint = round_ppc(out[channel]);
#else
#ifdef ARCH_X86
      tempint = round_trick(out[channel]);
#else
      tempint = (int)out[channel];
#endif
#endif

      /* Limit the output */
      if (tempint < -32768)
        data[index+channel] = -32768;
      else if (tempint > 32767)
        data[index+channel] = 32767;
      else
        data[index+channel] = tempint;
    } /* For each channel */

    /* Wrap around the indexes */
    i = (i+1)%3;
    j = (j+1)%3;
    k = (k+1)%3;
    /* random noise index */
    di = (di + 1) % 256;

  }/* For each pair of samples */

#ifdef BENCHMARK
  timex += get_counter();
  blength += length;
  if (count++ == 1024)
  {
    printf("FLOATING POINT: %f %d\n",timex/1024.0, blength/1024);
    blength = 0;
    timex = 0.;
    count = 0;
  }
#endif /* BENCHMARK */

/*  FTZ_OFF; */
  return length;
}
