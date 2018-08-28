/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluid_conv.h"
#include "auto_gen_array.h"
#include "auto_gen_math.h"

#define FLUID_CENTS_HZ_SIZE     1200
#define FLUID_VEL_CB_SIZE       128
#define FLUID_CB_AMP_SIZE       1441
#define FLUID_PAN_SIZE          1002

/* conversion tables */
static fluid_real_t fluid_ct2hz_tab[FLUID_CENTS_HZ_SIZE];
static fluid_real_t fluid_cb2amp_tab[FLUID_CB_AMP_SIZE];
static fluid_real_t fluid_concave_tab[FLUID_VEL_CB_SIZE];
static fluid_real_t fluid_convex_tab[FLUID_VEL_CB_SIZE];

#define FLUID_PAN_TAB(_i)   FSIN((M_PI / 2.0 / (FLUID_PAN_SIZE - 1.0))*(_i))

static TABLE_CONST fluid_real_t fluid_pan_tab[FLUID_PAN_SIZE] = { TABLE_INIT(AUTO_GEN_ARRAY_1002(FLUID_PAN_TAB)) };

/*
 * void fluid_synth_init
 *
 * Does all the initialization for this module.
 */
void
fluid_conversion_config(void)
{
    int i;
    double x;

    for(i = 0; i < FLUID_CENTS_HZ_SIZE; i++)
    {
        fluid_ct2hz_tab[i] = (fluid_real_t) pow(2.0, (double) i / 1200.0);
    }

    /* centibels to amplitude conversion
     * Note: SF2.01 section 8.1.3: Initial attenuation range is
     * between 0 and 144 dB. Therefore a negative attenuation is
     * not allowed.
     */
    for(i = 0; i < FLUID_CB_AMP_SIZE; i++)
    {
        fluid_cb2amp_tab[i] = (fluid_real_t) pow(10.0, (double) i / -200.0);
    }

    /* initialize the conversion tables (see fluid_mod.c
       fluid_mod_get_value cases 4 and 8) */

    /* concave unipolar positive transform curve */
    fluid_concave_tab[0] = 0.0;
    fluid_concave_tab[FLUID_VEL_CB_SIZE - 1] = 1.0;

    /* convex unipolar positive transform curve */
    fluid_convex_tab[0] = 0;
    fluid_convex_tab[FLUID_VEL_CB_SIZE - 1] = 1.0;

    /* There seems to be an error in the specs. The equations are
       implemented according to the pictures on SF2.01 page 73. */

    for(i = 1; i < FLUID_VEL_CB_SIZE - 1; i++)
    {
        // Original code:
        // x = (-200.0 / FLUID_PEAK_ATTENUATION) * log((i * i) / (fluid_real_t)((FLUID_VEL_CB_SIZE - 1) * (FLUID_VEL_CB_SIZE - 1))) / M_LN10;
        x = (-200.0 / FLUID_PEAK_ATTENUATION) * 2 * log((fluid_real_t)i / (fluid_real_t)(FLUID_VEL_CB_SIZE - 1)) / M_LN10;
        fluid_convex_tab[i] = (fluid_real_t)(1.0 - x);
        fluid_concave_tab[(FLUID_VEL_CB_SIZE - 1) - i] = (fluid_real_t) x;
    }

#ifndef ENABLE_CONST_TABLES
    /* initialize the pan conversion table */
    x = M_PI / 2.0 / (FLUID_PAN_SIZE - 1.0);

    for(i = 0; i < FLUID_PAN_SIZE; i++)
    {
        fluid_pan_tab[i] = (fluid_real_t) sin(i * x);
    }
#endif
}

/*
 * fluid_ct2hz
 */
fluid_real_t
fluid_ct2hz_real(fluid_real_t cents)
{
    if(cents < 0)
    {
        return (fluid_real_t) 1.0;
    }
    else if(cents < 900)
    {
        return (fluid_real_t) 6.875 * fluid_ct2hz_tab[(int)(cents + 300)];
    }
    else if(cents < 2100)
    {
        return (fluid_real_t) 13.75 * fluid_ct2hz_tab[(int)(cents - 900)];
    }
    else if(cents < 3300)
    {
        return (fluid_real_t) 27.5 * fluid_ct2hz_tab[(int)(cents - 2100)];
    }
    else if(cents < 4500)
    {
        return (fluid_real_t) 55.0 * fluid_ct2hz_tab[(int)(cents - 3300)];
    }
    else if(cents < 5700)
    {
        return (fluid_real_t) 110.0 * fluid_ct2hz_tab[(int)(cents - 4500)];
    }
    else if(cents < 6900)
    {
        return (fluid_real_t) 220.0 * fluid_ct2hz_tab[(int)(cents - 5700)];
    }
    else if(cents < 8100)
    {
        return (fluid_real_t) 440.0 * fluid_ct2hz_tab[(int)(cents - 6900)];
    }
    else if(cents < 9300)
    {
        return (fluid_real_t) 880.0 * fluid_ct2hz_tab[(int)(cents - 8100)];
    }
    else if(cents < 10500)
    {
        return (fluid_real_t) 1760.0 * fluid_ct2hz_tab[(int)(cents - 9300)];
    }
    else if(cents < 11700)
    {
        return (fluid_real_t) 3520.0 * fluid_ct2hz_tab[(int)(cents - 10500)];
    }
    else if(cents < 12900)
    {
        return (fluid_real_t) 7040.0 * fluid_ct2hz_tab[(int)(cents - 11700)];
    }
    else if(cents < 14100)
    {
        return (fluid_real_t) 14080.0 * fluid_ct2hz_tab[(int)(cents - 12900)];
    }
    else
    {
        return (fluid_real_t) 1.0; /* some loony trying to make you deaf */
    }
}

/*
 * fluid_ct2hz
 */
fluid_real_t
fluid_ct2hz(fluid_real_t cents)
{
    /* Filter fc limit: SF2.01 page 48 # 8 */
    if(cents >= 13500)
    {
        cents = 13500;             /* 20 kHz */
    }
    else if(cents < 1500)
    {
        cents = 1500;              /* 20 Hz */
    }

    return fluid_ct2hz_real(cents);
}

/*
 * fluid_cb2amp
 *
 * in: a value between 0 and 1440, 0 is no attenuation
 * out: a value between 1 and 0
 */
fluid_real_t
fluid_cb2amp(fluid_real_t cb)
{
    /*
     * cb: an attenuation in 'centibels' (1/10 dB)
     * SF2.01 page 49 # 48 limits it to 144 dB.
     * 96 dB is reasonable for 16 bit systems, 144 would make sense for 24 bit.
     */

    /* minimum attenuation: 0 dB */
    if(cb < 0)
    {
        return 1.0;
    }

    if(cb >= FLUID_CB_AMP_SIZE)
    {
        return 0.0;
    }

    return fluid_cb2amp_tab[(int) cb];
}

/*
 * fluid_tc2sec_delay
 */
fluid_real_t
fluid_tc2sec_delay(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 21, 23, 25, 33
     * SF2.01 section 8.1.3 items 21, 23, 25, 33
     *
     * The most negative number indicates a delay of 0. Range is limited
     * from -12000 to 5000 */
    if(tc <= -32768.0f)
    {
        return (fluid_real_t) 0.0f;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0f;
    }

    if(tc > 5000.0f)
    {
        tc = (fluid_real_t) 5000.0f;
    }

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec_attack
 */
fluid_real_t
fluid_tc2sec_attack(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 26, 34
     * SF2.01 section 8.1.3 items 26, 34
     * The most negative number indicates a delay of 0
     * Range is limited from -12000 to 8000 */
    if(tc <= -32768.)
    {
        return (fluid_real_t) 0.0;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0;
    };

    if(tc > 8000.)
    {
        tc = (fluid_real_t) 8000.0;
    };

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec
 */
fluid_real_t
fluid_tc2sec(fluid_real_t tc)
{
    /* No range checking here! */
    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec_release
 */
fluid_real_t
fluid_tc2sec_release(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 30, 38
     * SF2.01 section 8.1.3 items 30, 38
     * No 'most negative number' rule here!
     * Range is limited from -12000 to 8000 */
    if(tc <= -32768.)
    {
        return (fluid_real_t) 0.0;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0;
    };

    if(tc > 8000.)
    {
        tc = (fluid_real_t) 8000.0;
    };

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_act2hz
 *
 * Convert from absolute cents to Hertz
 */
fluid_real_t
fluid_act2hz(fluid_real_t c)
{
    return (fluid_real_t)(8.176 * pow(2.0, (double) c / 1200.0));
}

/*
 * fluid_hz2ct
 *
 * Convert from Hertz to cents
 */
fluid_real_t
fluid_hz2ct(fluid_real_t f)
{
    return (fluid_real_t)(6900 + 1200 * log(f / 440.0) / M_LN2);
}

/*
 * fluid_pan
 */
fluid_real_t
fluid_pan(fluid_real_t c, int left)
{
    if(left)
    {
        c = -c;
    }

    if(c <= -500)
    {
        return (fluid_real_t) 0.0;
    }
    else if(c >= 500)
    {
        return (fluid_real_t) 1.0;
    }
    else
    {
        return fluid_pan_tab[(int)(c + 500)];
    }
}

/*
 * Return the amount of attenuation based on the balance for the specified
 * channel. If balance is negative (turned toward left channel, only the right
 * channel is attenuated. If balance is positive, only the left channel is
 * attenuated.
 *
 * @params balance left/right balance, range [-960;960] in absolute centibels
 * @return amount of attenuation [0.0;1.0]
 */
fluid_real_t fluid_balance(fluid_real_t balance, int left)
{
    /* This is the most common case */
    if(balance == 0)
    {
        return 1.0f;
    }

    if((left && balance < 0) || (!left && balance > 0))
    {
        return 1.0f;
    }

    if(balance < 0)
    {
        balance = -balance;
    }

    return fluid_cb2amp(balance);
}

/*
 * fluid_concave
 */
fluid_real_t
fluid_concave(fluid_real_t val)
{
    if(val < 0)
    {
        return 0;
    }
    else if(val >= FLUID_VEL_CB_SIZE)
    {
        return 1;
    }

    return fluid_concave_tab[(int) val];
}

/*
 * fluid_convex
 */
fluid_real_t
fluid_convex(fluid_real_t val)
{
    if(val < 0)
    {
        return 0;
    }
    else if(val >= FLUID_VEL_CB_SIZE)
    {
        return 1;
    }

    return fluid_convex_tab[(int) val];
}
