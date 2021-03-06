/*--------------------------------------------------------------------
 *    The MB-system:	mb_coor_scale.c	1/21/93
 *    $Id$
 *
 *    Copyright (c) 1993-2017 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, CA 95039
 *    and Dale N. Chayes (dale@ldeo.columbia.edu)
 *      Lamont-Doherty Earth Observatory
 *      Palisades, NY 10964
 *
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/*
 * The function mb_coor_scale.c returns scaling factors
 * to turn longitude and latitude differences into distances in meters.
 * This function is based on code by James Charters (Scripps Institution
 * of Oceanography).
 *
 * Author:	D. W. Caress
 * Date:	January 21, 1993
 *
 *
 */

/* standard include files */
#include <stdio.h>
#include <math.h>
#include <string.h>

/* mbio include files */
#include "mb_status.h"
#include "mb_io.h"
#include "mb_define.h"

/* ellipsoid coefficients from World Geodetic System Ellipsoid of 1972
 * - see Bowditch (H.O. 9 -- American Practical Navigator). */
#define C1 111412.84
#define C2 -93.5
#define C3 0.118
#define C4 111132.92
#define C5 -559.82
#define C6 1.175
#define C7 0.0023

static char rcs_id[] = "$Id$";

/*--------------------------------------------------------------------*/
int mb_coor_scale(int verbose, double latitude, double *mtodeglon, double *mtodeglat) {
	char *function_name = "mb_coor_scale";
	int status;
	double radlat;

	/* print input debug statements */
	if (verbose >= 2) {
		fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", function_name);
		fprintf(stderr, "dbg2  Revision id: %s\n", rcs_id);
		fprintf(stderr, "dbg2  Input arguments:\n");
		fprintf(stderr, "dbg2       verbose: %d\n", verbose);
		fprintf(stderr, "dbg2       latitude: %f\n", latitude);
	}

	/* check that the latitude value is sensible */
	if (fabs(latitude) <= 90.0) {
		radlat = DTR * latitude;
		*mtodeglon = 1. / fabs(C1 * cos(radlat) + C2 * cos(3 * radlat) + C3 * cos(5 * radlat));
		*mtodeglat = 1. / fabs(C4 + C5 * cos(2 * radlat) + C6 * cos(4 * radlat) + C7 * cos(6 * radlat));
		status = MB_SUCCESS;
	}

	/* set error flag if needed */
	else {
		status = MB_FAILURE;
	}

	/* print output debug statements */
	if (verbose >= 2) {
		fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", function_name);
		fprintf(stderr, "dbg2  Revision id: %s\n", rcs_id);
		fprintf(stderr, "dbg2  Return arguments:\n");
		fprintf(stderr, "dbg2       mtodeglon: %g\n", *mtodeglon);
		fprintf(stderr, "dbg2       mtodeglat: %g\n", *mtodeglat);
		fprintf(stderr, "dbg2  Return status:\n");
		fprintf(stderr, "dbg2       status:    %d\n", status);
	}

	return (status);
}
/*--------------------------------------------------------------------*/
int mb_apply_lonflip(int verbose, int lonflip, double *longitude) {
	char *function_name = "mb_apply_lonflip";
	int status = MB_SUCCESS;

	/* print input debug statements */
	if (verbose >= 2) {
		fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", function_name);
		fprintf(stderr, "dbg2  Revision id: %s\n", rcs_id);
		fprintf(stderr, "dbg2  Input arguments:\n");
		fprintf(stderr, "dbg2       verbose: %d\n", verbose);
		fprintf(stderr, "dbg2       lonflip:   %d\n", lonflip);
		fprintf(stderr, "dbg2       longitude: %f\n", *longitude);
	}

	/* apply lonflip */
	if (lonflip < 0) {
		if (*longitude > 0.)
			*longitude = *longitude - 360.;
		else if (*longitude < -360.)
			*longitude = *longitude + 360.;
	}
	else if (lonflip == 0) {
		if (*longitude > 180.)
			*longitude = *longitude - 360.;
		else if (*longitude < -180.)
			*longitude = *longitude + 360.;
	}
	else {
		if (*longitude > 360.)
			*longitude = *longitude - 360.;
		else if (*longitude < 0.)
			*longitude = *longitude + 360.;
	}

	/* print output debug statements */
	if (verbose >= 2) {
		fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", function_name);
		fprintf(stderr, "dbg2  Revision id: %s\n", rcs_id);
		fprintf(stderr, "dbg2  Return arguments:\n");
		fprintf(stderr, "dbg2       longitude: %f\n", *longitude);
		fprintf(stderr, "dbg2  Return status:\n");
		fprintf(stderr, "dbg2       status:    %d\n", status);
	}

	return (status);
}
/*--------------------------------------------------------------------*/
