/**
 * Modified by: sascha_lammers@gmx.de
 */

#pragma once

#include "julian.h"

/*   PHASEHUNT	--  Find time of phases of the moon which surround the
		    current date.  Five phases are found, starting and
		    ending with the new moons which bound the  current
		    lunation.  */
void phaseHunt(double sdate, double phases[5]);

/*   PHASESHORT -- calculates values stored in MoonPhaseType

*/
struct MoonPhaseType;

void phaseShort(double pDate, MoonPhaseType &moon);

