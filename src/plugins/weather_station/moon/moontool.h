/**
 * Modified by: sascha_lammers@gmx.de
 */

#pragma once

#include "julian.h"

/*   PHASEHUNT	--  Find time of phases of the moon which surround the
		    current date.  Five phases are found, starting and
		    ending with the new moons which bound the  current
		    lunation.  */
void phasehunt(double sdate, double phases[5]);

/*  PHASE  --  Calculate phase of moon as a fraction:

    The  argument  is  the  time  for  which  the  phase is requested,
    expressed as a Julian date and fraction.  Returns  the  terminator
    phase  angle  as a percentage of a full circle (i.e., 0 to 1), and
    stores into pointer arguments  the	illuminated  fraction  of  the
    Moon's  disc, the Moon's age in days and fraction, the distance of
    the Moon from the centre of the Earth, and	the  angular  diameter
    subtended  by the Moon as seen by an observer at the centre of the
    Earth.
*/


//  double  *pphase;		      /* Illuminated fraction */
//  double  *mage;		      /* Age of moon in days */
//  double  *dist;		      /* Distance in kilometres */
//  double  *angdia;		      /* Angular diameter in degrees */
//  double  *sudist;		      /* Distance to Sun */
//  double  *suangdia;                  /* Sun's angular diameter */

double phase(double pdate, double *pphase, double *mage, double *dist, double *angdia, double *sudist, double *suangdia);

// reduced information
double phase_short(double pdate, double *pphase, double *mage);

