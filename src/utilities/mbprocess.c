/*--------------------------------------------------------------------
 *    The MB-system:	mbprocess.c	3/31/93
 *    $Id: mbprocess.c,v 4.0 2000-03-08 00:04:28 caress Exp $
 *
 *    Copyright (c) 1993, 1994 by 
 *    D. W. Caress (caress@lamont.ldgo.columbia.edu)
 *    and D. N. Chayes (dale@lamont.ldgo.columbia.edu)
 *    Lamont-Doherty Earth Observatory
 *    Palisades, NY  10964
 *
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/*
 * mbprocess is a tool for processing swath sonar bathymetry data.  
 * This program performs a number of functions, including:
 *   - merging navigation
 *   - recalculating bathymetry from travel time and angle data
 *     by raytracing through a layered water sound velocity model.
 *   - applying changes to ship draft, roll bias and pitch bias
 *   - applying bathymetry edits from edit mask files or edit save
 *     files.
 * The parameters controlling mbprocess are included in an ascii
 * parameter file with the following possible entries:
 *   FORMAT format                  # sets format id\n\
 *   INFILE file                    # sets input file path
 *   OUTFILE file                   # sets output file path
 *   DRAFT draft                    # sets draft value (m)
 *   DRAFTOFFSET offset             # sets value added to draft (m)
 *   ROLLBIAS                       # sets roll bias (degrees)
 *   ROLLBIASPORT                   # sets port roll bias (degrees)
 *   ROLLBIASSTBD                   # sets starboard roll bias (degrees)
 *   PITCHBIAS                      # sets pitch bias
 *   NAVFILE file                   # sets navigation file path
 *   NAVSPLINE                      # sets spline navigation interpolation
 *   HEADING                        # sets heading to course made good\n\
 *   HEADINGOFFSET offset           # sets value added to heading (degree)\n\
 *   SVPFILE file                   # sets svp file path
 *   SSV                            # sets ssv value (m/s)
 *   SSVOFFSET                      # sets value added to ssv (m/s)
 *   UNCORRECTED                    # sets raytraced bathymetry to "uncorrected" values
 *   EDITSAVEFILE                   # sets edit save file path (from mbedit)
 *   EDITMASKFILE                   # sets edit mask file path (from mbmask)
 * The data format and the input and output data files can be
 * specified using command line options. If no parameter file is 
 * specified (using the -P option) but an input file is specified
 * (with the -I option), then mbprocess will look for a parameter
 * file with the path inputfile.par, where inputfile is the input
 * file path.\n";
 *
 * Author:	D. W. Caress
 * Date:	January 4, 2000
 *
 * $Log: not supported by cvs2svn $
 *
 */

/* standard include files */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/* mbio include files */
#include "../../include/mb_format.h"
#include "../../include/mb_status.h"
#include "../../include/mb_define.h"

/* mbprocess defines */
#define MBP_FILENAMESIZE	256
#define MBP_BATHRECALC_OFF	0
#define MBP_BATHRECALC_RAYTRACE	1
#define MBP_BATHRECALC_ROTATE	2
#define MBP_ROLLBIAS_OFF	0
#define MBP_ROLLBIAS_SINGLE	1
#define MBP_ROLLBIAS_DOUBLE	1
#define MBP_PITCHBIAS_OFF	0
#define MBP_PITCHBIAS_ON	1
#define MBP_DRAFT_OFF		0
#define MBP_DRAFT_OFFSET	1
#define MBP_DRAFT_SET		2
#define MBP_NAV_OFF		0
#define MBP_NAV_ON		1
#define MBP_NAV_LINEAR		0
#define MBP_NAV_SPLINE		1
#define MBP_HEADING_OFF         0
#define MBP_HEADING_CALC        1
#define MBP_HEADING_OFFSET      2
#define MBP_SSV_OFF		0
#define MBP_SSV_OFFSET		1
#define MBP_SSV_SET		2
#define MBP_SSV_CORRECT		1
#define MBP_SSV_INCORRECT	2
#define MBP_SVP_OFF		0
#define MBP_SVP_ON		1
#define MBP_EDIT_OFF		0
#define MBP_EDIT_ON		1
#define	MBP_EDIT_FLAG		1
#define	MBP_EDIT_UNFLAG		2
#define	MBP_EDIT_ZERO		3
#define MBP_MASK_OFF		0
#define MBP_MASK_ON		1

/*--------------------------------------------------------------------*/

main (argc, argv)
int argc;
char **argv; 
{
	/* id variables */
	static char rcs_id[] = "$Id: mbprocess.c,v 4.0 2000-03-08 00:04:28 caress Exp $";
	static char program_name[] = "mbprocess";
	static char help_message[] =  "mbprocess is a tool for processing swath sonar bathymetry data.\n\
This program performs a number of functions, including:\n\
  - merging navigation\n\
  - recalculating bathymetry from travel time and angle data\n\
    by raytracing through a layered water sound velocity model.\n\
  - applying changes to ship draft, roll bias and pitch bias\n\
  - applying bathymetry edits from edit mask files or edit save\n\
    files.\n\
The parameters controlling mbprocess are included in an ascii\n\
parameter file with the following possible entries:\n\
  FORMAT format                  # sets format id\n\
  INFILE file                    # sets input file path\n\
  OUTFILE file                   # sets output file path\n\
  DRAFT draft                    # sets draft value (m)\n\
  DRAFTOFFSET offset             # sets value added to draft (m)\n\
  ROLLBIAS                       # sets roll bias (degrees)\n\
  ROLLBIASPORT                   # sets port roll bias (degrees)\n\
  ROLLBIASSTBD                   # sets starboard roll bias (degrees)\n\
  PITCHBIAS                      # sets pitch bias\n\
  NAVFILE file                   # sets navigation file path\n\
  NAVFORMAT format               # sets navigation file format\n\
  NAVSPLINE                      # sets spline navigation interpolation\n\
  HEADING                        # sets heading to course made good\n\
  HEADINGOFFSET offset           # sets value added to heading (degree)\n\
  SVPFILE file                   # sets svp file path\n\
  SSV                            # sets ssv value (m/s)\n\
  SSVOFFSET                      # sets value added to ssv (m/s)\n\
  UNCORRECTED                    # sets raytraced bathymetry to uncorrected values\n\
  EDITSAVEFILE                   # sets edit save file path (from mbedit)\n\
  EDITMASKFILE                   # sets edit mask file path (from mbmask)\n\
The data format and the input and output data files can be\n\
specified using command line options. If no parameter file is \n\
specified (using the -P option) but an input file is specified\n\
(with the -I option), then mbprocess will look for a parameter\n\
file with the path inputfile.par, where inputfile is the input\n\
file path.\n";
	static char usage_message[] = "mbprocess [-Fformat  \n\t-Iinfile -Ooutfile -Pparfile -V -H]";

	/* parsing variables */
	extern char *optarg;
	extern int optkind;
	int	errflg = 0;
	int	c;
	int	help = 0;
	int	flag = 0;

	/* MBIO status variables */
	int	status = MB_SUCCESS;
	int	verbose = 0;
	int	error = MB_ERROR_NO_ERROR;
	char	*message = NULL;

	/* MBIO read and write control parameters */
	int	format = 0;
	int	format_num;
	int	pings;
	int	lonflip;
	double	bounds[4];
	int	btime_i[7];
	int	etime_i[7];
	double	btime_d;
	double	etime_d;
	double	speedmin;
	double	timegap;
	int	beams_bath;
	int	beams_amp;
	int	pixels_ss;
	char	*imbio_ptr = NULL;
	char	*ombio_ptr = NULL;

	/* mbio read and write values */
	char	*store_ptr = NULL;
	int	kind;
	int	time_i[7];
	double	time_d;
	double	navlon;
	double	navlat;
	double	speed;
	double	heading;
	double	distance;
	int	nbath;
	int	namp;
	int	nss;
	char	*beamflag = NULL;
	double	*bath = NULL;
	double	*bathacrosstrack = NULL;
	double	*bathalongtrack = NULL;
	double	*amp = NULL;
	double	*ss = NULL;
	double	*ssacrosstrack = NULL;
	double	*ssalongtrack = NULL;
	int	idata = 0;
	int	icomment = 0;
	int	odata = 0;
	int	ocomment = 0;
	char	comment[MBP_FILENAMESIZE];

	/* time, user, host variables */
	time_t	right_now;
	char	date[25], user[MBP_FILENAMESIZE], *user_ptr, host[MBP_FILENAMESIZE];
	
	/* parameter controls */
	int	mbp_parfile_specified = MB_NO;
	char	mbp_parfile[MBP_FILENAMESIZE];
	int	mbp_format_specified = MB_NO;
	int	mbp_format;
	int	mbp_ifile_specified = MB_NO;
	char	mbp_ifile[MBP_FILENAMESIZE];
	int	mbp_ofile_specified = MB_NO;
	char	mbp_ofile[MBP_FILENAMESIZE];
	int	mbp_fileroot_specified = MB_NO;
	char	mbp_fileroot[MBP_FILENAMESIZE];
	int	mbp_bathrecalc_mode = MBP_BATHRECALC_OFF;
	int	mbp_rollbias_mode = MBP_ROLLBIAS_OFF;
	double	mbp_rollbias;
	double	mbp_rollbias_port;
	double	mbp_rollbias_stbd;
	int	mbp_pitchbias_mode = MBP_PITCHBIAS_OFF;
	double	mbp_pitchbias;
	int	mbp_draft_mode = MBP_DRAFT_OFF;
	double	mbp_draft;
	int	mbp_ssv_mode = MBP_SSV_OFF;
	double	mbp_ssv;
	int	mbp_svp_mode = MBP_SVP_OFF;
	char	mbp_svpfile[MBP_FILENAMESIZE];
	int	mbp_uncorrected = MB_NO;
	int	mbp_nav_mode = MBP_NAV_OFF;
	char	mbp_navfile[MBP_FILENAMESIZE];
	int	mbp_nav_format;
	int	mbp_nav_algorithm = MBP_NAV_LINEAR;
	int	mbp_heading_mode = MBP_HEADING_OFF;
	double	mbp_headingbias;
	int	mbp_edit_mode = MBP_EDIT_OFF;
	char	mbp_editfile[MBP_FILENAMESIZE];
	int	mbp_mask_mode = MBP_MASK_OFF;
	char	mbp_maskfile[MBP_FILENAMESIZE];
	
	/* processing variables */
	FILE	*tfp;
	struct stat file_status;
	int	fstat;
	int	nnav = 0;
	int	size, nchar, len, nget, nav_ok;
	int	time_j[5], stime_i[7], ftime_i[7];
	double	sec, hr;
	char	*bufftmp;
	char	NorS[2], EorW[2];
	double	mlon, llon, mlat, llat;
	int	degree, minute, time_set;
	double	dminute;
	double	second;
	double	splineflag;
	double	*ntime, *nlon, *nlat, *nlonspl, *nlatspl;
	int	itime;
	double	mtodeglon, mtodeglat;
	double	del_time, dx, dy, dist;
	int	intstat;
	double	heading_old = 0.0;
	int	nsvp = 0;
	double	*depth = NULL;
	double	*velocity = NULL;
	double	*velocity_sum = NULL;
	char	*rt_svp;
	double	ssv;
	int	nedit = 0;
	double	*edit_time_d;
	int	*edit_beam;
	int	*edit_action;
	double	draft, depth_offset_use, static_shift;
	double	ttime, range;
	double	xx, zz, vsum, vavg;
	double	alpha, beta, theta, phi;
	int	ray_stat;
	double	*ttimes = NULL;
	double	*angles = NULL;
	double	*angles_forward = NULL;
	double	*angles_null = NULL;
	double	*heave = NULL;
	double	*alongtrack_offset = NULL;

	/* ssv handling variables */
	int	ssv_mode = MBP_SSV_CORRECT;
	int	ssv_prelimpass = MB_YES;
	double	ssv_default;
	double	ssv_start;
	
	int	stat_status;
	struct stat statbuf;
	char	buffer[MBP_FILENAMESIZE], dummy[MBP_FILENAMESIZE], *result;
	int	nbeams;
	int	i, j, k, l, m, n, mm;
	
	char	*ctime();
	char	*getenv();

	/* get current default values */
	status = mb_defaults(verbose,&mbp_format,&pings,&lonflip,bounds,
		btime_i,etime_i,&speedmin,&timegap);

	/* reset all defaults */
	pings = 1;
	lonflip = 0;
	bounds[0] = -360.;
	bounds[1] = 360.;
	bounds[2] = -90.;
	bounds[3] = 90.;
	btime_i[0] = 1962;
	btime_i[1] = 2;
	btime_i[2] = 21;
	btime_i[3] = 10;
	btime_i[4] = 30;
	btime_i[5] = 0;
	btime_i[6] = 0;
	etime_i[0] = 2062;
	etime_i[1] = 2;
	etime_i[2] = 21;
	etime_i[3] = 10;
	etime_i[4] = 30;
	etime_i[5] = 0;
	etime_i[6] = 0;
	speedmin = 0.0;
	timegap = 1000000000.0;

	/* set default input and output */
	strcpy (mbp_parfile, "\0");
	strcpy (mbp_ifile, "\0");
	strcpy (mbp_ofile, "\0");

	/* process argument list */
	while ((c = getopt(argc, argv, "VvHhF:f:I:i:O:o:P:p:")) != -1)
	  switch (c) 
		{
		case 'H':
		case 'h':
			help++;
			break;
		case 'V':
		case 'v':
			verbose++;
			break;
		case 'F':
		case 'f':
			sscanf (optarg,"%d", &mbp_format);
			mbp_format_specified = MB_YES;
			flag++;
			break;
		case 'I':
		case 'i':
			mbp_ifile_specified = MB_YES;
			sscanf (optarg,"%s", mbp_ifile);
			flag++;
			break;
		case 'O':
		case 'o':
			mbp_ofile_specified = MB_YES;
			sscanf (optarg,"%s", mbp_ofile);
			flag++;
			break;
		case 'P':
		case 'p':
			sscanf (optarg,"%s", mbp_parfile);
			mbp_parfile_specified = MB_YES;
			flag++;
			break;
		case '?':
			errflg++;
		}

	/* if error flagged then print it and exit */
	if (errflg)
		{
		fprintf(stderr,"usage: %s\n", usage_message);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		error = MB_ERROR_BAD_USAGE;
		exit(error);
		}

	/* print starting message */
	if (verbose == 1 || help)
		{
		fprintf(stderr,"\nProgram %s\n",program_name);
		fprintf(stderr,"Version %s\n",rcs_id);
		fprintf(stderr,"MB-System Version %s\n",MB_VERSION);
		}
		
	/* figure out if parameter file available */
	if (mbp_parfile_specified == MB_NO
	    && mbp_ifile_specified == MB_YES)
	    {
	    strcpy(mbp_parfile, mbp_ifile);
	    strcat(mbp_parfile, ".par");
	    if (stat(mbp_parfile, &statbuf) == 0)
		{
		mbp_parfile_specified = MB_YES;
		}
	    }

	/* quit if no parameter file available */
	if (mbp_parfile_specified == MB_NO)
	    {
	    fprintf(stderr,"\nProgram <%s> requires a parameter file.\n",program_name);
	    fprintf(stderr,"The parameter file may be specified with the -P option\n");
	    fprintf(stderr,"or it may exist as 'infile.par', where the input file\n");
	    fprintf(stderr,"'infile' is specified with the -I option.\n");
	    fprintf(stderr,"\nProgram <%s> Terminated\n",
		    program_name);
	    error = MB_ERROR_OPEN_FAIL;
	    exit(error);
	    }
	    
	/* parse the parameter file if available */
	if (mbp_parfile_specified == MB_YES)
	    {
	    if ((tfp = fopen(mbp_parfile, "r")) == NULL) 
		    {
		    error = MB_ERROR_OPEN_FAIL;
		    fprintf(stderr,"\nUnable to Open Parameter File <%s> for reading\n",mbp_parfile);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }
	    while ((result = fgets(buffer,MBP_FILENAMESIZE,tfp)) == buffer)
		{
		if (buffer[0] != '#')
		    {
		    if (strncmp(buffer, "FORMAT", 6) == 0
			&& mbp_format_specified == MB_NO)
			{
			sscanf(buffer, "%s %d", dummy, &mbp_format);
			mbp_format_specified = MB_YES;
			}
		    else if (strncmp(buffer, "INFILE", 6) == 0
			&& mbp_ifile_specified == MB_NO)
			{
			sscanf(buffer, "%s %s", dummy, mbp_ifile);
			mbp_ifile_specified = MB_YES;
			}
		    else if (strncmp(buffer, "OUTFILE", 7) == 0
			&& mbp_ofile_specified == MB_NO)
			{
			sscanf(buffer, "%s %s", dummy, mbp_ofile);
			mbp_ofile_specified = MB_YES;
			}
		    else if (strncmp(buffer, "DRAFTOFFSET", 11) == 0
			&& mbp_draft_mode == MBP_DRAFT_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_draft);
			mbp_draft_mode = MBP_DRAFT_OFFSET;
			}
		    else if (strncmp(buffer, "DRAFT", 5) == 0
			&& mbp_draft_mode == MBP_DRAFT_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_draft);
			mbp_draft_mode = MBP_DRAFT_SET;
			}
		    else if (strncmp(buffer, "ROLLBIASPORT", 12) == 0
			&& (mbp_rollbias_mode == MBP_ROLLBIAS_OFF
			    || mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE))
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_rollbias_port);
			mbp_rollbias_mode = MBP_ROLLBIAS_DOUBLE;
			}
		    else if (strncmp(buffer, "ROLLBIASSTBD", 12) == 0
			&& (mbp_rollbias_mode == MBP_ROLLBIAS_OFF
			    || mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE))
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_rollbias_stbd);
			mbp_rollbias_mode = MBP_ROLLBIAS_DOUBLE;
			}
		    else if (strncmp(buffer, "ROLLBIAS", 8) == 0
			&& mbp_rollbias_mode == MBP_ROLLBIAS_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_rollbias);
			mbp_rollbias_mode = MBP_ROLLBIAS_SINGLE;
			}
		    else if (strncmp(buffer, "PITCHBIAS", 9) == 0
			&& mbp_pitchbias_mode == MBP_PITCHBIAS_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_pitchbias);
			mbp_pitchbias_mode = MBP_PITCHBIAS_ON;
			}
		    else if (strncmp(buffer, "NAVFILE", 7) == 0
			&& mbp_nav_mode == MBP_NAV_OFF)
			{
			sscanf(buffer, "%s %s", dummy, mbp_navfile);
			mbp_nav_mode = MBP_NAV_ON;
			}
		    else if (strncmp(buffer, "NAVFORMAT", 9) == 0)
			{
			sscanf(buffer, "%s %d", dummy, &mbp_nav_format);
			}
		    else if (strncmp(buffer, "NAVSPLINE", 9) == 0)
			{
			mbp_nav_algorithm = MBP_NAV_SPLINE;
			}
		    else if (strncmp(buffer, "HEADING", 7) == 0)
			{
			mbp_heading_mode = MBP_HEADING_CALC;
			}
		    else if (strncmp(buffer, "HEADINGOFFSET", 13) == 0)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_headingbias);
			mbp_heading_mode = MBP_HEADING_OFFSET;
			}
		    else if (strncmp(buffer, "SSVOFFSET", 11) == 0
			&& mbp_ssv_mode == MBP_SSV_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_ssv);
			mbp_ssv_mode = MBP_SSV_OFFSET;
			}
		    else if (strncmp(buffer, "SSV", 5) == 0
			&& mbp_ssv_mode == MBP_SSV_OFF)
			{
			sscanf(buffer, "%s %lf", dummy, &mbp_ssv);
			mbp_ssv_mode = MBP_SSV_SET;
			}
		    else if (strncmp(buffer, "SVP", 3) == 0
			&& mbp_svp_mode == MBP_SVP_OFF)
			{
			sscanf(buffer, "%s %s", dummy, mbp_svpfile);
			mbp_svp_mode = MBP_SVP_ON;
			}
		    else if (strncmp(buffer, "UNCORRECTED", 3) == 0
			&& mbp_uncorrected == MB_NO)
			{
			mbp_uncorrected = MB_YES;
			}
		    else if (strncmp(buffer, "EDITSAVEFILE", 12) == 0
			&& mbp_edit_mode == MBP_EDIT_OFF)
			{
			sscanf(buffer, "%s %s", dummy, mbp_editfile);
			mbp_edit_mode = MBP_EDIT_ON;
			}
		    else if (strncmp(buffer, "EDITMASKFILE", 12) == 0
			&& mbp_mask_mode == MBP_MASK_OFF)
			{
			sscanf(buffer, "%s %s", dummy, mbp_maskfile);
			mbp_mask_mode = MBP_MASK_ON;
			}

		    }
		}
	    fclose(tfp);
	    }

	/* quit if no input file specified */
	if (mbp_ifile_specified == MB_NO)
	    {
	    fprintf(stderr,"\nProgram <%s> requires an input data file.\n",program_name);
	    fprintf(stderr,"The input file may be specified with the -I option\n");
	    fprintf(stderr,"or it may be set in a parameter file specified with the -P option.\n");
	    fprintf(stderr,"\nProgram <%s> Terminated\n",
		    program_name);
	    error = MB_ERROR_OPEN_FAIL;
	    exit(error);
	    }
	    
	/* figure out data format if required */
	if (mbp_format_specified == MB_NO)
	    {
	    status = mb_get_format(verbose, mbp_ifile, mbp_fileroot, &mbp_format, &error);
	    if (status == MB_SUCCESS && mbp_format > 0)
		{
		mbp_format_specified = MB_YES;
		mbp_fileroot_specified = MB_YES;
		}
	    else
		{
		fprintf(stderr,"\nProgram <%s> unable to guess input data format.\n",program_name);
		fprintf(stderr,"The format may be specified with the -F option\n");
		fprintf(stderr,"or it may be set in a parameter file specified with the -P option.\n");
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		error = MB_ERROR_BAD_FORMAT;
		exit(error);
		}
	    }
	    
	/* figure out output filename if required */
	if (mbp_ofile_specified == MB_NO)
	    {
	    if (mbp_fileroot_specified == MB_NO)
		status = mb_get_format(verbose, mbp_ifile, mbp_fileroot, &format, &error);
	    if (strstr(mbp_fileroot, "_") == NULL)
		sprintf(mbp_ofile, "%s_p.mb%d", mbp_fileroot, mbp_format);
	    else
		sprintf(mbp_ofile, "%sp.mb%d", mbp_fileroot, mbp_format);
	    mbp_ofile_specified = MB_YES;
	    }
	    
	/* figure out bathymetry recalculation mode */
	if (mbp_svp_mode == MBP_SVP_ON)
	    mbp_bathrecalc_mode = MBP_BATHRECALC_RAYTRACE;
	else if (mbp_svp_mode == MBP_SVP_OFF
	    && (mbp_rollbias_mode != MBP_ROLLBIAS_OFF
		|| mbp_pitchbias_mode != MBP_PITCHBIAS_OFF
		|| mbp_draft_mode == MBP_DRAFT_OFFSET))
	    mbp_bathrecalc_mode = MBP_BATHRECALC_ROTATE;

	/* check for format with travel time data */
	if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
	    {
	    status = mb_format(verbose,&mbp_format,&format_num,&error);
	    if (mb_traveltime_table[format_num] != MB_YES)
		{
		fprintf(stderr,"\nProgram <%s> requires travel time data to recalculate\n",program_name);
		fprintf(stderr,"bathymetry from travel times and angles.\n");
		fprintf(stderr,"Format %d is unacceptable because it does not inlude travel time data.\n",mbp_format);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		error = MB_ERROR_BAD_FORMAT;
		exit(error);
		}
	    }

	/* print starting debug statements */
	if (verbose >= 2)
	    {
	    fprintf(stderr,"\ndbg2  Program <%s>\n",program_name);
	    fprintf(stderr,"dbg2  Version %s\n",rcs_id);
	    fprintf(stderr,"dbg2  MB-system Version %s\n",MB_VERSION);
	    fprintf(stderr,"\ndbg2  MB-System Control Parameters:\n");
	    fprintf(stderr,"dbg2       verbose:         %d\n",verbose);
	    fprintf(stderr,"dbg2       help:            %d\n",help);
	    fprintf(stderr,"dbg2       format:          %d\n",format);
	    fprintf(stderr,"dbg2       pings:           %d\n",pings);
	    fprintf(stderr,"dbg2       lonflip:         %d\n",lonflip);
	    fprintf(stderr,"dbg2       bounds[0]:       %f\n",bounds[0]);
	    fprintf(stderr,"dbg2       bounds[1]:       %f\n",bounds[1]);
	    fprintf(stderr,"dbg2       bounds[2]:       %f\n",bounds[2]);
	    fprintf(stderr,"dbg2       bounds[3]:       %f\n",bounds[3]);
	    fprintf(stderr,"dbg2       btime_i[0]:      %d\n",btime_i[0]);
	    fprintf(stderr,"dbg2       btime_i[1]:      %d\n",btime_i[1]);
	    fprintf(stderr,"dbg2       btime_i[2]:      %d\n",btime_i[2]);
	    fprintf(stderr,"dbg2       btime_i[3]:      %d\n",btime_i[3]);
	    fprintf(stderr,"dbg2       btime_i[4]:      %d\n",btime_i[4]);
	    fprintf(stderr,"dbg2       btime_i[5]:      %d\n",btime_i[5]);
	    fprintf(stderr,"dbg2       btime_i[6]:      %d\n",btime_i[6]);
	    fprintf(stderr,"dbg2       etime_i[0]:      %d\n",etime_i[0]);
	    fprintf(stderr,"dbg2       etime_i[1]:      %d\n",etime_i[1]);
	    fprintf(stderr,"dbg2       etime_i[2]:      %d\n",etime_i[2]);
	    fprintf(stderr,"dbg2       etime_i[3]:      %d\n",etime_i[3]);
	    fprintf(stderr,"dbg2       etime_i[4]:      %d\n",etime_i[4]);
	    fprintf(stderr,"dbg2       etime_i[5]:      %d\n",etime_i[5]);
	    fprintf(stderr,"dbg2       etime_i[6]:      %d\n",etime_i[6]);
	    fprintf(stderr,"dbg2       speedmin:        %f\n",speedmin);
	    fprintf(stderr,"dbg2       timegap:         %f\n",timegap);
	    fprintf(stderr,"dbg2       verbose:         %d\n",verbose);
	    fprintf(stderr,"\ndbg2  Processing Parameters:\n");
	    if (mbp_parfile_specified == MB_YES)
		fprintf(stderr,"dbg2       parameter file:  %s\n",mbp_parfile);
	    if (mbp_format_specified == MB_YES)
		fprintf(stderr,"dbg2       format:          %d\n",mbp_format);
	    if (mbp_ifile_specified == MB_YES)
		fprintf(stderr,"dbg2       input file:      %s\n",mbp_ifile);
	    if (mbp_ifile_specified == MB_YES)
		fprintf(stderr,"dbg2       output file:     %s\n",mbp_ofile);
	    if (mbp_bathrecalc_mode == MBP_BATHRECALC_OFF)
		fprintf(stderr,"dbg2       Bathymetry not recalculated.\n");
	    else if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
		{
		fprintf(stderr,"dbg2       Bathymetry recalculated by raytracing.\n");
		if (mbp_rollbias_mode == MBP_ROLLBIAS_OFF)
		    fprintf(stderr,"dbg2       roll bias:       OFF\n");
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE)
		    fprintf(stderr,"dbg2       roll bias:       %f deg\n", mbp_rollbias);
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE)
		    {
		    fprintf(stderr,"dbg2       port roll bias:  %f deg\n", mbp_rollbias_port);
		    fprintf(stderr,"dbg2       port roll stbd:  %f deg\n", mbp_rollbias_stbd);
		    }
		if (mbp_pitchbias_mode == MBP_PITCHBIAS_OFF)
		    fprintf(stderr,"dbg2       pitch bias:      OFF\n");
		else if (mbp_pitchbias_mode == MBP_PITCHBIAS_ON)
		    fprintf(stderr,"dbg2       pitch bias:      %f deg\n", mbp_pitchbias);
		if (mbp_draft_mode == MBP_DRAFT_OFF)
		    fprintf(stderr,"dbg2       draft:           OFF\n");
		else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
		    fprintf(stderr,"dbg2       offset draft:    %f m\n", mbp_draft);
		else if (mbp_draft_mode == MBP_DRAFT_SET)
		    fprintf(stderr,"dbg2       set draft:       %f m\n", mbp_draft);
		if (mbp_ssv_mode == MBP_SSV_OFF)
		    fprintf(stderr,"dbg2       ssv:             OFF\n");
		else if (mbp_ssv_mode == MBP_SSV_OFFSET)
		    fprintf(stderr,"dbg2       offset ssv:      %f m/s\n", mbp_ssv);
		else if (mbp_ssv_mode == MBP_SSV_SET)
		    fprintf(stderr,"dbg2       set ssv:         %f m/s\n", mbp_ssv);
		if (mbp_svp_mode == MBP_SVP_OFF)
		    fprintf(stderr,"dbg2       svp:             OFF\n");
		else if (mbp_svp_mode == MBP_SVP_ON)
		    fprintf(stderr,"dbg2       svp file:        %s\n", mbp_svpfile);
		if (mbp_uncorrected == MB_NO)
		    fprintf(stderr,"dbg2       bathymetry mode: CORRECTED\n");
		else if (mbp_uncorrected == MB_YES)
		    fprintf(stderr,"dbg2       bathymetry mode: UNCORRECTED\n");
		}
	    else if (mbp_bathrecalc_mode == MBP_BATHRECALC_ROTATE)
		{
		fprintf(stderr,"dbg2       Bathymetry recalculated by rigid rotation.\n");
		if (mbp_rollbias_mode == MBP_ROLLBIAS_OFF)
		    fprintf(stderr,"dbg2       roll bias:       OFF\n");
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE)
		    fprintf(stderr,"dbg2       roll bias:       %f deg\n", mbp_rollbias);
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE)
		    {
		    fprintf(stderr,"dbg2       port roll bias:  %f deg\n", mbp_rollbias_port);
		    fprintf(stderr,"dbg2       port roll stbd:  %f deg\n", mbp_rollbias_stbd);
		    }
		if (mbp_pitchbias_mode == MBP_PITCHBIAS_OFF)
		    fprintf(stderr,"dbg2       pitch bias:      OFF\n");
		else if (mbp_pitchbias_mode == MBP_PITCHBIAS_ON)
		    fprintf(stderr,"dbg2       pitch bias:      %f deg\n", mbp_pitchbias);
		if (mbp_draft_mode == MBP_DRAFT_OFF)
		    fprintf(stderr,"dbg2       draft:           OFF\n");
		else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
		    fprintf(stderr,"dbg2       offset draft:    %f m\n", mbp_draft);
		}
	    if (mbp_nav_mode == MBP_NAV_OFF)
		fprintf(stderr,"dbg2       merge navigation:OFF\n");
	    else if (mbp_nav_mode == MBP_NAV_ON)
		{
		fprintf(stderr,"dbg2       navigation file:      %s\n", mbp_navfile);
		fprintf(stderr,"dbg2       navigation format:    %d\n", mbp_nav_format);
		if (mbp_nav_algorithm == MBP_NAV_LINEAR)
		    fprintf(stderr,"dbg2       navigation algorithm: linear interpolation\n");
		else if (mbp_nav_algorithm == MBP_NAV_SPLINE)
		    fprintf(stderr,"dbg2       navigation algorithm: spline interpolation\n");
		}
	    if (mbp_edit_mode == MBP_EDIT_OFF)
		fprintf(stderr,"dbg2       merge bath edit: OFF\n");
	    else if (mbp_edit_mode == MBP_EDIT_ON)
		fprintf(stderr,"dbg2       bathy edit file: %s\n", mbp_editfile);
	    if (mbp_mask_mode == MBP_MASK_OFF)
		fprintf(stderr,"dbg2       merge bath mask: OFF\n");
	    else if (mbp_mask_mode == MBP_MASK_ON)
		fprintf(stderr,"dbg2       bathy mask file: %s\n", mbp_maskfile);
	    }

	/* print starting debug statements */
	if (verbose == 1)
	    {
	    fprintf(stderr,"\nProgram <%s>\n",program_name);
	    fprintf(stderr,"Version %s\n",rcs_id);
	    fprintf(stderr,"MB-system Version %s\n",MB_VERSION);
	    fprintf(stderr,"\nProcessing Parameters:\n");
	    if (mbp_parfile_specified == MB_YES)
		fprintf(stderr,"     parameter file:  %s\n",mbp_parfile);
	    if (mbp_format_specified == MB_YES)
		fprintf(stderr,"     format:          %d\n",mbp_format);
	    if (mbp_ifile_specified == MB_YES)
		fprintf(stderr,"     input file:      %s\n",mbp_ifile);
	    if (mbp_ifile_specified == MB_YES)
		fprintf(stderr,"     output file:     %s\n",mbp_ofile);
	    if (mbp_bathrecalc_mode == MBP_BATHRECALC_OFF)
		fprintf(stderr,"     Bathymetry not recalculated.\n");
	    else if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
		{
		fprintf(stderr,"     Bathymetry recalculated by raytracing.\n");
		if (mbp_rollbias_mode == MBP_ROLLBIAS_OFF)
		    fprintf(stderr,"     roll bias:       OFF\n");
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE)
		    fprintf(stderr,"     roll bias:       %f deg\n", mbp_rollbias);
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE)
		    {
		    fprintf(stderr,"     port roll bias:  %f deg\n", mbp_rollbias_port);
		    fprintf(stderr,"     port roll stbd:  %f deg\n", mbp_rollbias_stbd);
		    }
		if (mbp_pitchbias_mode == MBP_PITCHBIAS_OFF)
		    fprintf(stderr,"     pitch bias:      OFF\n");
		else if (mbp_pitchbias_mode == MBP_PITCHBIAS_ON)
		    fprintf(stderr,"     pitch bias:      %f deg\n", mbp_pitchbias);
		if (mbp_draft_mode == MBP_DRAFT_OFF)
		    fprintf(stderr,"     draft:           OFF\n");
		else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
		    fprintf(stderr,"     offset draft:    %f m\n", mbp_draft);
		else if (mbp_draft_mode == MBP_DRAFT_SET)
		    fprintf(stderr,"     set draft:       %f m\n", mbp_draft);
		if (mbp_ssv_mode == MBP_SSV_OFF)
		    fprintf(stderr,"     ssv:             OFF\n");
		else if (mbp_ssv_mode == MBP_SSV_OFFSET)
		    fprintf(stderr,"     offset ssv:      %f m/s\n", mbp_ssv);
		else if (mbp_ssv_mode == MBP_SSV_SET)
		    fprintf(stderr,"     set ssv:         %f m/s\n", mbp_ssv);
		if (mbp_svp_mode == MBP_SVP_OFF)
		    fprintf(stderr,"     svp:             OFF\n");
		else if (mbp_svp_mode == MBP_SVP_ON)
		    fprintf(stderr,"     svp file:        %s\n", mbp_svpfile);
		if (mbp_uncorrected == MB_NO)
		    fprintf(stderr,"     bathymetry mode: CORRECTED\n");
		else if (mbp_uncorrected == MB_YES)
		    fprintf(stderr,"     bathymetry mode: UNCORRECTED\n");
		}
	    else if (mbp_bathrecalc_mode == MBP_BATHRECALC_ROTATE)
		{
		fprintf(stderr,"     Bathymetry recalculated by rigid rotation.\n");
		if (mbp_rollbias_mode == MBP_ROLLBIAS_OFF)
		    fprintf(stderr,"     roll bias:       OFF\n");
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE)
		    fprintf(stderr,"     roll bias:       %f deg\n", mbp_rollbias);
		else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE)
		    {
		    fprintf(stderr,"     port roll bias:  %f deg\n", mbp_rollbias_port);
		    fprintf(stderr,"     port roll stbd:  %f deg\n", mbp_rollbias_stbd);
		    }
		if (mbp_pitchbias_mode == MBP_PITCHBIAS_OFF)
		    fprintf(stderr,"     pitch bias:      OFF\n");
		else if (mbp_pitchbias_mode == MBP_PITCHBIAS_ON)
		    fprintf(stderr,"     pitch bias:      %f deg\n", mbp_pitchbias);
		if (mbp_draft_mode == MBP_DRAFT_OFF)
		    fprintf(stderr,"     draft:           OFF\n");
		else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
		    fprintf(stderr,"     offset draft:    %f m\n", mbp_draft);
		}
	    if (mbp_nav_mode == MBP_NAV_OFF)
		fprintf(stderr,"     merge navigation:OFF\n");
	    else if (mbp_nav_mode == MBP_NAV_ON)
		{
		fprintf(stderr,"     navigation file:      %s\n", mbp_navfile);
		fprintf(stderr,"     navigation format:    %d\n", mbp_nav_format);
		if (mbp_nav_algorithm == MBP_NAV_LINEAR)
		    fprintf(stderr,"     navigation algorithm: linear interpolation\n");
		else if (mbp_nav_algorithm == MBP_NAV_SPLINE)
		    fprintf(stderr,"     navigation algorithm: spline interpolation\n");
		}
	    if (mbp_edit_mode == MBP_EDIT_OFF)
		fprintf(stderr,"     merge bath edit: OFF\n");
	    else if (mbp_edit_mode == MBP_EDIT_ON)
		fprintf(stderr,"     bathy edit file: %s\n", mbp_editfile);
	    if (mbp_mask_mode == MBP_MASK_OFF)
		fprintf(stderr,"     merge bath mask: OFF\n");
	    else if (mbp_mask_mode == MBP_MASK_ON)
		fprintf(stderr,"     bathy mask file: %s\n", mbp_maskfile);
	    }

	/* if help desired then print it and exit */
	if (help)
	    {
	    fprintf(stderr,"MB-System Version %s\n",MB_VERSION);
	    fprintf(stderr,"\n%s\n",help_message);
	    fprintf(stderr,"\nusage: %s\n", usage_message);
	    exit(error);
	    }

	/*--------------------------------------------
	  get svp
	  --------------------------------------------*/

	/* if raytracing to be done get svp */
	if (mbp_svp_mode == MBP_SVP_ON)
	    {
	    /* count the data points in the svp file */
	    nsvp = 0;
	    if ((tfp = fopen(mbp_svpfile, "r")) == NULL) 
		    {
		    error = MB_ERROR_OPEN_FAIL;
		    fprintf(stderr,"\nUnable to Open Velocity Profile File <%s> for reading\n",mbp_svpfile);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }
	    while ((result = fgets(buffer,MBP_FILENAMESIZE,tfp)) == buffer)
		    if (buffer[0] != '#')
			nsvp++;
	    fclose(tfp);
	    
	    /* allocate arrays for svp */
	    if (nsvp > 1)
		{
		size = (nsvp+1)*sizeof(double);
		status = mb_malloc(verbose,size,&depth,&error);
		status = mb_malloc(verbose,size,&velocity,&error);
		status = mb_malloc(verbose,size,&velocity_sum,&error);
	
		/* if error initializing memory then quit */
		if (error != MB_ERROR_NO_ERROR)
		    {
		    mb_error(verbose,error,&message);
		    fprintf(stderr,"\nMBIO Error allocating data arrays:\n%s\n",message);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }		    
		}
	
	    /* if no svp data then quit */
	    else
		{
		error = MB_ERROR_BAD_DATA;
		fprintf(stderr,"\nUnable to read data from SVP file <%s>\n",mbp_svpfile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}		    
		
	    /* read the data points in the svp file */
	    nsvp = 0;
	    if ((tfp = fopen(mbp_svpfile, "r")) == NULL) 
		{
		error = MB_ERROR_OPEN_FAIL;
		fprintf(stderr,"\nUnable to Open Velocity Profile File <%s> for reading\n",mbp_svpfile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}
	    while ((result = fgets(buffer,MBP_FILENAMESIZE,tfp)) == buffer)
		{
		if (buffer[0] != '#')
		    {
		    mm = sscanf(buffer,"%lf %lf",&depth[nsvp],&velocity[nsvp]);
		
		    /* output some debug values */
		    if (verbose >= 5 && mm == 2)
			    {
			    fprintf(stderr,"\ndbg5  New velocity value read in program <%s>\n",program_name);
			    fprintf(stderr,"dbg5       depth[%d]: %f  velocity[%d]: %f\n",
				    nsvp,depth[nsvp],nsvp,velocity[nsvp]);
			    }
		    if (mm == 2)
			nsvp++;
		    }
		}
	    fclose(tfp);

	    /* if velocity profile doesn't extend to 12000 m depth
		    extend it to that depth */
	    if (depth[nsvp-1] < 12000.0)
		    {
		    depth[nsvp] = 12000.0;
		    velocity[nsvp] = velocity[nsvp-1];
		    nsvp++;
		    }
    
	    /* get velocity sums */
	    velocity_sum[0] = 0.5*(velocity[1] + velocity[0])
		    *(depth[1] - depth[0]);
	    for (i=1;i<nsvp-1;i++)
		    {
		    velocity_sum[i] = velocity_sum[i-1] 
			+ 0.5*(velocity[i+1] + velocity[i])
			*(depth[i+1] - depth[i]);
		    }
	    }

	/*--------------------------------------------
	  get nav
	  --------------------------------------------*/

	/* if nav merging to be done get nav */
	if (mbp_nav_mode == MBP_NAV_ON)
	    {
	    /* set max number of characters to be read at a time */
	    if (mbp_nav_format == 8)
		    nchar = 96;
	    else
		    nchar = 128;

	    /* count the data points in the nav file */
	    nnav = 0;
	    if ((tfp = fopen(mbp_navfile, "r")) == NULL) 
		    {
		    error = MB_ERROR_OPEN_FAIL;
		    fprintf(stderr,"\nUnable to Open Velocity Profile File <%s> for reading\n",mbp_navfile);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }
	    while ((result = fgets(buffer,nchar,tfp)) == buffer)
		    nnav++;
	    fclose(tfp);
	    
	    /* allocate arrays for nav */
	    if (nnav > 1)
		{
		size = (nnav+1)*sizeof(double);
		status = mb_malloc(verbose,nnav*sizeof(double),&ntime,&error);
		status = mb_malloc(verbose,nnav*sizeof(double),&nlon,&error);
		status = mb_malloc(verbose,nnav*sizeof(double),&nlat,&error);
		status = mb_malloc(verbose,nnav*sizeof(double),&nlonspl,&error);
		status = mb_malloc(verbose,nnav*sizeof(double),&nlatspl,&error);
	
		/* if error initializing memory then quit */
		if (error != MB_ERROR_NO_ERROR)
		    {
		    mb_error(verbose,error,&message);
		    fprintf(stderr,"\nMBIO Error allocating data arrays:\n%s\n",message);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }		    
		}
	
	    /* if no nav data then quit */
	    else
		{
		error = MB_ERROR_BAD_DATA;
		fprintf(stderr,"\nUnable to read data from navigation file <%s>\n",mbp_navfile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}		    
		
	    /* read the data points in the nav file */
	    nnav = 0;
	    if ((tfp = fopen(mbp_navfile, "r")) == NULL) 
		{
		error = MB_ERROR_OPEN_FAIL;
		fprintf(stderr,"\nUnable to Open navigation File <%s> for reading\n",mbp_navfile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}
	    while ((result = fgets(buffer,nchar,tfp)) == buffer)
		{
		/* deal with nav in form: time_d lon lat */
		nav_ok = MB_NO;
		if (mbp_nav_format == 1)
			{
			nget = sscanf(buffer,"%lf %lf %lf",
				&ntime[nnav],&nlon[nnav],&nlat[nnav]);
			if (nget == 3)
				nav_ok = MB_YES;
			}

		/* deal with nav in form: yr mon day hour min sec lon lat */
		else if (mbp_nav_format == 2)
			{
			nget = sscanf(buffer,"%d %d %d %d %d %lf %lf %lf",
				&time_i[0],&time_i[1],&time_i[2],
				&time_i[3],&time_i[4],&sec,
				&nlon[nnav],&nlat[nnav]);
			time_i[5] = (int) sec;
			time_i[6] = 1000000*(sec - time_i[5]);
			mb_get_time(verbose,time_i,&time_d);
			ntime[nnav] = time_d;
			if (nget == 8)
				nav_ok = MB_YES;
			}

		/* deal with nav in form: yr jday hour min sec lon lat */
		else if (mbp_nav_format == 3)
			{
			nget = sscanf(buffer,"%d %d %d %d %lf %lf %lf",
				&time_j[0],&time_j[1],&hr,
				&time_j[2],&sec,
				&nlon[nnav],&nlat[nnav]);
			time_j[2] = time_j[2] + 60*hr;
			time_j[3] = (int) sec;
			time_j[4] = 1000000*(sec - time_j[3]);
			mb_get_itime(verbose,time_j,time_i);
			mb_get_time(verbose,time_i,&time_d);
			ntime[nnav] = time_d;
			if (nget == 7)
				nav_ok = MB_YES;
			}

		/* deal with nav in form: yr jday daymin sec lon lat */
		else if (mbp_nav_format == 4)
			{
			nget = sscanf(buffer,"%d %d %d %lf %lf %lf",
				&time_j[0],&time_j[1],&time_j[2],
				&sec,
				&nlon[nnav],&nlat[nnav]);
			time_j[3] = (int) sec;
			time_j[4] = 1000000*(sec - time_j[3]);
			mb_get_itime(verbose,time_j,time_i);
			mb_get_time(verbose,time_i,&time_d);
			ntime[nnav] = time_d;
			if (nget == 6)
				nav_ok = MB_YES;
			}

		/* deal with nav in L-DEO processed nav format */
		else if (mbp_nav_format == 5)
			{
			strncpy(dummy,"\0",128);
			time_j[0] = atoi(strncpy(dummy,buffer,2));
			mb_fix_y2k(verbose, time_j[0], &time_j[0]);
			strncpy(dummy,"\0",128);
			time_j[1] = atoi(strncpy(dummy,buffer+3,3));
			strncpy(dummy,"\0",128);
			hr = atoi(strncpy(dummy,buffer+7,2));
			strncpy(dummy,"\0",128);
			time_j[2] = atoi(strncpy(dummy,buffer+10,2))
				+ 60*hr;
			strncpy(dummy,"\0",128);
			time_j[3] = atof(strncpy(dummy,buffer+13,3));
			time_j[4] = 0;
			mb_get_itime(verbose,time_j,time_i);
			mb_get_time(verbose,time_i,&time_d);
			ntime[nnav] = time_d;

			strncpy(NorS,"\0",sizeof(NorS));
			strncpy(NorS,buffer+20,1);
			strncpy(dummy,"\0",128);
			mlat = atof(strncpy(dummy,buffer+21,3));
			strncpy(dummy,"\0",128);
			llat = atof(strncpy(dummy,buffer+24,8));
			strncpy(EorW,"\0",sizeof(EorW));
			strncpy(EorW,buffer+33,1);
			strncpy(dummy,"\0",128);
			mlon = atof(strncpy(dummy,buffer+34,4));
			strncpy(dummy,"\0",128);
			llon = atof(strncpy(dummy,buffer+38,8));
			nlon[nnav] = mlon + llon/60.;
			if (strncmp(EorW,"W",1) == 0) 
				nlon[nnav] = -nlon[nnav];
			nlat[nnav] = mlat + llat/60.;
			if (strncmp(NorS,"S",1) == 0) 
				nlat[nnav] = -nlat[nnav];
			nav_ok = MB_YES;
			}

		/* deal with nav in real and pseudo NMEA 0183 format */
		else if (mbp_nav_format == 6 || mbp_nav_format == 7)
			{
			/* check if real sentence */
			len = strlen(buffer);
			if (strncmp(buffer,"$",1) == 0)
			    {
			    if (strncmp(&buffer[3],"DAT",3) == 0
				&& len > 15)
				{
				time_set = MB_NO;
				strncpy(dummy,"\0",128);
				time_i[0] = atoi(strncpy(dummy,buffer+7,4));
				time_i[1] = atoi(strncpy(dummy,buffer+11,2));
				time_i[2] = atoi(strncpy(dummy,buffer+13,2));
				}
			    else if ((strncmp(&buffer[3],"ZDA",3) == 0
				    || strncmp(&buffer[3],"UNX",3) == 0)
				    && len > 14)
				{
				time_set = MB_NO;
				/* find start of ",hhmmss.ss" */
				if ((bufftmp = strchr(buffer, ',')) != NULL)
				    {
				    strncpy(dummy,"\0",128);
				    time_i[3] = atoi(strncpy(dummy,bufftmp+1,2));
				    strncpy(dummy,"\0",128);
				    time_i[4] = atoi(strncpy(dummy,bufftmp+3,2));
				    strncpy(dummy,"\0",128);
				    time_i[5] = atoi(strncpy(dummy,bufftmp+5,2));
				    if (bufftmp[7] == '.')
					{
					strncpy(dummy,"\0",128);
					time_i[6] = 10000*
					    atoi(strncpy(dummy,bufftmp+8,2));
					}
				    else
					time_i[6] = 0;
				    /* find start of ",dd,mm,yyyy" */
				    if ((bufftmp = strchr(&bufftmp[1], ',')) != NULL)
					{
					strncpy(dummy,"\0",128);
					time_i[2] = atoi(strncpy(dummy,bufftmp+1,2));
					strncpy(dummy,"\0",128);
					time_i[1] = atoi(strncpy(dummy,bufftmp+4,2));
					strncpy(dummy,"\0",128);
					time_i[0] = atoi(strncpy(dummy,bufftmp+7,4));
					time_set = MB_YES;
					}
				    }
				}
			    else if (((mbp_nav_format == 6 && strncmp(&buffer[3],"GLL",3) == 0)
				|| (mbp_nav_format == 7 && strncmp(&buffer[3],"GGA",3) == 0))
				&& time_set == MB_YES && len > 26)
				{
				time_set = MB_NO;
				/* find start of ",ddmm.mm,N,ddmm.mm,E" */
				if ((bufftmp = strchr(buffer, ',')) != NULL)
				    {
				    if (mbp_nav_format == 7)
					bufftmp = strchr(&bufftmp[1], ',');
				    strncpy(dummy,"\0",128);
				    degree = atoi(strncpy(dummy,bufftmp+1,2));
				    strncpy(dummy,"\0",128);
				    dminute = atof(strncpy(dummy,bufftmp+3,5));
				    strncpy(NorS,"\0",sizeof(NorS));
				    strncpy(NorS,bufftmp+9,1);
				    nlat[nnav] = degree + dminute/60.;
				    if (strncmp(NorS,"S",1) == 0) 
					nlat[nnav] = -nlat[nnav];
				    strncpy(dummy,"\0",128);
				    degree = atoi(strncpy(dummy,bufftmp+11,3));
				    strncpy(dummy,"\0",128);
				    dminute = atof(strncpy(dummy,bufftmp+14,5));
				    strncpy(EorW,"\0",sizeof(EorW));
				    strncpy(EorW,bufftmp+20,1);
				    nlon[nnav] = degree + dminute/60.;
				    if (strncmp(EorW,"W",1) == 0) 
					nlon[nnav] = -nlon[nnav];
				    mb_get_time(verbose,time_i,&time_d);
				    ntime[nnav] = time_d;
				    nav_ok = MB_YES;
				    }
				}
			    }
			}

		/* deal with nav in Simrad 90 format */
		else if (mbp_nav_format == 8)
			{
			mb_get_int(&(time_i[2]), buffer+2,  2);
			mb_get_int(&(time_i[1]), buffer+4,  2);
			mb_get_int(&(time_i[0]), buffer+6,  2);
			mb_fix_y2k(verbose, time_i[0], &time_i[0]);
			mb_get_int(&(time_i[3]), buffer+9,  2);
			mb_get_int(&(time_i[4]), buffer+11, 2);
			mb_get_int(&(time_i[5]), buffer+13, 2);
			mb_get_int(&(time_i[6]), buffer+15, 2);
			time_i[6] = 10000 * time_i[6];
			mb_get_time(verbose,time_i,&time_d);
			ntime[nnav] = time_d;

			mb_get_double(&mlat,    buffer+18,   2);
			mb_get_double(&llat, buffer+20,   7);
			NorS[0] = buffer[27];
			nlat[nnav] = mlat + llat/60.0;
			if (NorS[0] == 'S' || NorS[0] == 's')
				nlat[nnav] = -nlat[nnav];
			mb_get_double(&mlon,    buffer+29,   3);
			mb_get_double(&llon, buffer+32,   7);
			EorW[0] = buffer[39];
			nlon[nnav] = mlon + llon/60.0;
			if (EorW[0] == 'W' || EorW[0] == 'w')
				nlon[nnav] = -nlon[nnav];
			nav_ok = MB_YES;
			}

		/* deal with nav in form: yr mon day hour min sec time_d lon lat */
		else if (mbp_nav_format == 9)
			{
			nget = sscanf(buffer,"%d %d %d %d %d %lf %lf %lf %lf",
				&time_i[0],&time_i[1],&time_i[2],
				&time_i[3],&time_i[4],&sec,
				&ntime[nnav],
				&nlon[nnav],&nlat[nnav]);
			if (nget == 9)
				nav_ok = MB_YES;
			}


		/* make sure longitude is defined according to lonflip */
		if (nav_ok == MB_YES)
			{
			if (lonflip == -1 && nlon[nnav] > 0.0)
				nlon[nnav] = nlon[nnav] - 360.0;
			else if (lonflip == 0 && nlon[nnav] < -180.0)
				nlon[nnav] = nlon[nnav] + 360.0;
			else if (lonflip == 0 && nlon[nnav] > 180.0)
				nlon[nnav] = nlon[nnav] - 360.0;
			else if (lonflip == 1 && nlon[nnav] < 0.0)
				nlon[nnav] = nlon[nnav] + 360.0;
			}

		/* output some debug values */
		if (verbose >= 5 && nav_ok == MB_YES)
			{
			fprintf(stderr,"\ndbg5  New navigation point read in program <%s>\n",program_name);
			fprintf(stderr,"dbg5       nav[%d]: %f %f %f\n",
				nnav,ntime[nnav],nlon[nnav],nlat[nnav]);
			}
		else if (verbose >= 5)
			{
			fprintf(stderr,"\ndbg5  Error parsing line in navigation file in program <%s>\n",program_name);
			fprintf(stderr,"dbg5       line: %s\n",buffer);
			}

		/* check for reverses or repeats in time */
		if (nav_ok == MB_YES)
			{
			if (nnav == 0)
				nnav++;
			else if (ntime[nnav] > ntime[nnav-1])
				nnav++;
			else if (nnav > 0 && ntime[nnav] <= ntime[nnav-1] 
				&& verbose >= 5)
				{
				fprintf(stderr,"\ndbg5  Navigation time error in program <%s>\n",program_name);
				fprintf(stderr,"dbg5       nav[%d]: %f %f %f\n",
					nnav-1,ntime[nnav-1],nlon[nnav-1],
					nlat[nnav-1]);
				fprintf(stderr,"dbg5       nav[%d]: %f %f %f\n",
					nnav,ntime[nnav],nlon[nnav],
					nlat[nnav]);
				}
			}
		strncpy(buffer,"\0",sizeof(buffer));
		}
	    fclose(tfp);

		
	    /* check for nav */
	    if (nnav < 2)
		    {
		    fprintf(stderr,"\nNo navigation read from file <%s>\n",mbp_navfile);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }
    
	    /* set up spline interpolation of nav points */
	    splineflag = 1.0e30;
	    spline(ntime-1,nlon-1,nnav,splineflag,splineflag,nlonspl-1);
	    spline(ntime-1,nlat-1,nnav,splineflag,splineflag,nlatspl-1);
    
	    /* get start and finish times of nav */
	    mb_get_date(verbose,ntime[0],stime_i);
	    mb_get_date(verbose,ntime[nnav-1],ftime_i);
    
	    /* give the statistics */
	    if (verbose >= 1)
		    {
		    fprintf(stderr,"\n%d navigation records read\n",nnav);
		    fprintf(stderr,"Nav start time: %4.4d %2.2d %2.2d %2.2d:%2.2d:%2.2d.%6.6d\n",
			    stime_i[0],stime_i[1],stime_i[2],stime_i[3],
			    stime_i[4],stime_i[5],stime_i[6]);
		    fprintf(stderr,"Nav end time:   %4.4d %2.2d %2.2d %2.2d:%2.2d:%2.2d.%6.6d\n",
			    ftime_i[0],ftime_i[1],ftime_i[2],ftime_i[3],
			    ftime_i[4],ftime_i[5],ftime_i[6]);
		    }
	    }

	/*--------------------------------------------
	  get edits
	  --------------------------------------------*/

	/* get edits */
	if (mbp_edit_mode == MBP_EDIT_ON)
	    {
	    /* count the data points in the edit file */
	    nedit = 0;
	    fstat = stat(mbp_editfile, &file_status);
	    if (fstat == 0 
		&& (file_status.st_mode & S_IFMT) != S_IFDIR)
		{
		nedit = file_status.st_size 
			/ (sizeof(double) + 2 * sizeof(int));
		}
	    
	    /* allocate arrays for edit */
	    if (nedit > 0)
		{
		size = nedit *sizeof(double);
		status = mb_malloc(verbose,size,&edit_time_d,&error);
		status = mb_malloc(verbose,size,&edit_beam,&error);
		status = mb_malloc(verbose,size,&edit_action,&error);
	
		/* if error initializing memory then quit */
		if (error != MB_ERROR_NO_ERROR)
		    {
		    mb_error(verbose,error,&message);
		    fprintf(stderr,"\nMBIO Error allocating data arrays:\n%s\n",message);
		    fprintf(stderr,"\nProgram <%s> Terminated\n",
			    program_name);
		    exit(error);
		    }		    
		}
	    }
		
	if (mbp_edit_mode == MBP_EDIT_ON
	    && nedit > 0)
	    {
	    /* read the data points in the edit file */
	    if ((tfp = fopen(mbp_editfile, "r")) == NULL) 
		{
		error = MB_ERROR_OPEN_FAIL;
		fprintf(stderr,"\nUnable to Open Edit Save File <%s> for reading\n",mbp_editfile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}
	    error = MB_ERROR_NO_ERROR;
	    for (i=0;i<nedit && error == MB_ERROR_NO_ERROR;i++)
		{
		if (fread(&edit_time_d[i], sizeof(double), 1, tfp) != 1)
		    {
		    status = MB_FAILURE;
		    error = MB_ERROR_EOF;
		    }
		if (status == MB_SUCCESS
		    && fread(&edit_beam[i], sizeof(int), 1, tfp) != 1)
		    {
		    status = MB_FAILURE;
		    error = MB_ERROR_EOF;
		    }
		if (status == MB_SUCCESS
		    && fread(&edit_action[i], sizeof(int), 1, tfp) != 1)
		    {
		    status = MB_FAILURE;
		    error = MB_ERROR_EOF;
		    }
		}
	    fclose(tfp);
	    }

	/*--------------------------------------------
	  now read the file
	  --------------------------------------------*/

	/* initialize reading the input swath sonar file */
	if ((status = mb_read_init(
		verbose,mbp_ifile,mbp_format,pings,lonflip,bounds,
		btime_i,etime_i,speedmin,timegap,
		&imbio_ptr,&btime_d,&etime_d,
		&beams_bath,&beams_amp,&pixels_ss,&error)) != MB_SUCCESS)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error returned from function <mb_read_init>:\n%s\n",message);
		fprintf(stderr,"\nMultibeam File <%s> not initialized for reading\n",mbp_ifile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}

	/* initialize writing the output swath sonar file */
	if ((status = mb_write_init(
		verbose,mbp_ofile,mbp_format,&ombio_ptr,
		&beams_bath,&beams_amp,&pixels_ss,&error)) != MB_SUCCESS)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error returned from function <mb_write_init>:\n%s\n",message);
		fprintf(stderr,"\nMultibeam File <%s> not initialized for writing\n",mbp_ofile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}

	/* allocate memory for data arrays */
	status = mb_malloc(verbose,beams_bath*sizeof(char),&beamflag,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&bath,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),
				&bathacrosstrack,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),
				&bathalongtrack,&error);
	status = mb_malloc(verbose,beams_amp*sizeof(double),&amp,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),&ss,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),&ssacrosstrack,
				&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),&ssalongtrack,
				&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&ttimes,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&angles,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&angles_forward,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&angles_null,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&heave,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&alongtrack_offset,&error);

	/* if error initializing memory then quit */
	if (error != MB_ERROR_NO_ERROR)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error allocating data arrays:\n%s\n",message);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}
	
	/* read input file until a surface sound velocity value
		is obtained, then close and reopen the file 
		this provides the starting surface sound velocity
		for recalculating the bathymetry */
	ssv_start = 0.0;
	if (ssv_prelimpass == MB_YES)
	    {
	    error = MB_ERROR_NO_ERROR;
	    while (error <= MB_ERROR_NO_ERROR
		&& ssv_start <= 0.0)
		{
		/* read some data */
		error = MB_ERROR_NO_ERROR;
		status = MB_SUCCESS;
		status = mb_get_all(verbose,imbio_ptr,&store_ptr,&kind,
				time_i,&time_d,&navlon,&navlat,&speed,
				&heading,&distance,
				&nbath,&namp,&nss,
				beamflag,bath,amp,bathacrosstrack,bathalongtrack,
				ss,ssacrosstrack,ssalongtrack,
				comment,&error);
				
		if (kind == MB_DATA_DATA 
			&& error <= MB_ERROR_NO_ERROR)
			{
			/* extract travel times */
			status = mb_ttimes(verbose,imbio_ptr,
				store_ptr,&kind,&nbeams,
				ttimes,angles,
				angles_forward,angles_null,
				heave,alongtrack_offset,
				&draft,&ssv,&error);
				
			/* check surface sound velocity */
			if (ssv > 0.0)
				ssv_start = ssv;
			}
		}
	
	    /* close and reopen the input file */
	    status = mb_close(verbose,&imbio_ptr,&error);
	    if ((status = mb_read_init(
		verbose,mbp_ifile,mbp_format,pings,lonflip,bounds,
		btime_i,etime_i,speedmin,timegap,
		&imbio_ptr,&btime_d,&etime_d,
		&beams_bath,&beams_amp,&pixels_ss,&error)) != MB_SUCCESS)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error returned from function <mb_read_init>:\n%s\n",message);
		fprintf(stderr,"\nMultibeam File <%s> not initialized for reading\n",mbp_ifile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(error);
		}
	    }
	if (ssv_start <= 0.0)
		ssv_start = ssv_default;
	
	/* reset error */
	error = MB_ERROR_NO_ERROR;
	status = MB_SUCCESS;

	/* write comments to beginning of output file */
	kind = MB_DATA_COMMENT;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"Bathymetry data generated by program %s",program_name);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"Version %s",rcs_id);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"MB-system Version %s",MB_VERSION);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(date,"\0",25);
	right_now = time((time_t *)0);
	strncpy(date,ctime(&right_now),24);
	if ((user_ptr = getenv("USER")) == NULL)
		user_ptr = getenv("LOGNAME");
	if (user_ptr != NULL)
		strcpy(user,user_ptr);
	else
		strcpy(user, "unknown");
	gethostname(host,MBP_FILENAMESIZE);
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"Run by user <%s> on cpu <%s> at <%s>",
		user,host,date);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;

	if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
	    {
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"Depths and crosstrack distances recalculated from travel times");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  by raytracing through a water velocity profile specified");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  by the user.  The depths have been saved in units of");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    if (mbp_uncorrected == MB_YES)
		    sprintf(comment,"  uncorrected meters (the depth values are adjusted to be");
	    else
		    sprintf(comment,"  corrected meters (the depth values obtained by");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    if (mbp_uncorrected == MB_YES)
		    sprintf(comment,"  consistent with a vertical water velocity of 1500 m/s).");
	    else
		    sprintf(comment,"  raytracing are not adjusted further).");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    }
	    
	else
	    {
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"Depths and crosstrack distances adjusted for roll bias, ");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  and pitch bias.");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    }
	    
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"Control Parameters:");
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"  MBIO data format:   %d",mbp_format);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"  Input file:         %s",mbp_ifile);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"  Output file:        %s",mbp_ofile);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;

	if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
	    {
	    if (ssv_mode == MBP_SSV_CORRECT)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  SSV mode:           original SSV correct");
		status = mb_put_comment(verbose,ombio_ptr,comment,&error);
		if (error == MB_ERROR_NO_ERROR) ocomment++;
		}
	    else
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  SSV mode:           original SSV incorrect");
		status = mb_put_comment(verbose,ombio_ptr,comment,&error);
		if (error == MB_ERROR_NO_ERROR) ocomment++;
		}
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  Default SSV:        %f",ssv_default);
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    if (ssv_prelimpass == MB_YES)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  SSV initial pass:   on");
		status = mb_put_comment(verbose,ombio_ptr,comment,&error);
		if (error == MB_ERROR_NO_ERROR) ocomment++;
		}
	    else
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  SSV initial pass:   off");
		status = mb_put_comment(verbose,ombio_ptr,comment,&error);
		if (error == MB_ERROR_NO_ERROR) ocomment++;
		}

	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  SVP file:               %s",mbp_svpfile);
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"  Input water sound velocity profile:");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    strncpy(comment,"\0",MBP_FILENAMESIZE);
	    sprintf(comment,"    depth (m)   velocity (m/s)");
	    status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	    if (error == MB_ERROR_NO_ERROR) ocomment++;
	    for (i=0;i<nsvp;i++)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"     %10.2f     %10.2f",
			depth[i],velocity[i]);
		status = mb_put_comment(verbose,ombio_ptr,comment,&error);
		if (error == MB_ERROR_NO_ERROR) ocomment++;
		}
	    }

	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"  Roll bias:    %f degrees (starboard: -, port: +)",
		mbp_rollbias);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment,"  Pitch bias:   %f degrees (aft: -, forward: +)",
		mbp_pitchbias);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	if (mbp_draft_mode == MBP_DRAFT_SET)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  Draft set:    %f meters",
			mbp_draft);
		}
	else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  Draft offset: %f meters",
			mbp_draft);
		}
	else if (mbp_draft_mode == MBP_DRAFT_OFF)
		{
		strncpy(comment,"\0",MBP_FILENAMESIZE);
		sprintf(comment,"  Draft:        not altered");
		}
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	strncpy(comment,"\0",MBP_FILENAMESIZE);
	sprintf(comment," ");
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;

	/* set up the raytracing */
	status = mb_rt_init(verbose, nsvp, depth, velocity, &rt_svp, &error);

	/* read and write */
	while (error <= MB_ERROR_NO_ERROR)
		{
		/* read some data */
		error = MB_ERROR_NO_ERROR;
		status = MB_SUCCESS;
		status = mb_get_all(verbose,imbio_ptr,&store_ptr,&kind,
				time_i,&time_d,&navlon,&navlat,&speed,
				&heading,&distance,
				&nbath,&namp,&nss,
				beamflag,bath,amp,
				bathacrosstrack,bathalongtrack,
				ss,ssacrosstrack,ssalongtrack,
				comment,&error);

		/* increment counter */
		if (error <= MB_ERROR_NO_ERROR 
			&& kind == MB_DATA_DATA)
			idata = idata + pings;
		else if (error <= MB_ERROR_NO_ERROR 
			&& kind == MB_DATA_COMMENT)
			icomment++;

		/* time gaps do not matter to mbprocess */
		if (error == MB_ERROR_TIME_GAP)
			{
			status = MB_SUCCESS;
			error = MB_ERROR_NO_ERROR;
			}

		/* non-survey data do not matter to mbprocess */
		if (error == MB_ERROR_OTHER)
			{
			status = MB_SUCCESS;
			error = MB_ERROR_NO_ERROR;
			}

		/* output error messages */
		if (verbose >= 1 && error == MB_ERROR_COMMENT)
			{
			if (icomment == 1)
				fprintf(stderr,"\nComments in Input:\n");
			fprintf(stderr,"%s\n",comment);
			}
		else if (verbose >= 1 && error < MB_ERROR_NO_ERROR
			&& error > MB_ERROR_OTHER)
			{
			mb_error(verbose,error,&message);
			fprintf(stderr,"\nNonfatal MBIO Error:\n%s\n",message);
			fprintf(stderr,"Input Record: %d\n",idata);
			fprintf(stderr,"Time: %d %d %d %d %d %d\n",
				time_i[0],time_i[1],time_i[2],
				time_i[3],time_i[4],time_i[5]);
			}
		else if (verbose >= 1 && error < MB_ERROR_NO_ERROR)
			{
			mb_error(verbose,error,&message);
			fprintf(stderr,"\nNonfatal MBIO Error:\n%s\n",message);
			fprintf(stderr,"Input Record: %d\n",idata);
			}
		else if (verbose >= 1 && error != MB_ERROR_NO_ERROR 
			&& error != MB_ERROR_EOF)
			{
			mb_error(verbose,error,&message);
			fprintf(stderr,"\nFatal MBIO Error:\n%s\n",message);
			fprintf(stderr,"Last Good Time: %d %d %d %d %d %d\n",
				time_i[0],time_i[1],time_i[2],
				time_i[3],time_i[4],time_i[5]);
			}

		/* interpolate the navigation */
		if (error == MB_ERROR_NO_ERROR
			&& mbp_nav_mode == MBP_NAV_ON
			&& (kind == MB_DATA_DATA
			    || kind == MB_DATA_NAV))
			{
			if (mbp_nav_algorithm == MBP_NAV_SPLINE
			    && time_d >= ntime[0] 
			    && time_d <= ntime[nnav-1])
			    {
			    intstat = splint(ntime-1,nlon-1,nlonspl-1,
				    nnav,time_d,&navlon,&itime);
			    intstat = splint(ntime-1,nlat-1,nlatspl-1,
				    nnav,time_d,&navlat,&itime);
			    }
			else
			    {
			    intstat = linint(ntime-1,nlon-1,
				    nnav,time_d,&navlon,&itime);
			    intstat = linint(ntime-1,nlat-1,
				    nnav,time_d,&navlat,&itime);
			    }
			}

		/* make up heading and speed if required */
		if (error == MB_ERROR_NO_ERROR
			&& mbp_nav_mode == MBP_NAV_ON
			&& (kind == MB_DATA_DATA
			    || kind == MB_DATA_NAV)
			&& mbp_heading_mode == MBP_HEADING_CALC)
			{
			mb_coor_scale(verbose,nlat[itime-1],&mtodeglon,&mtodeglat);
			del_time = ntime[itime] - ntime[itime-1];
			dx = (nlon[itime] - nlon[itime-1])/mtodeglon;
			dy = (nlat[itime] - nlat[itime-1])/mtodeglat;
			dist = sqrt(dx*dx + dy*dy);
			if (del_time > 0.0)
				speed = 3.6*dist/del_time;
			else
				speed = 0.0;
			if (dist > 0.0)
				{
				heading = RTD*atan2(dx/dist,dy/dist);
				heading_old = heading;
				}
			else
				heading = heading_old;
			}

		/* else adjust heading if required */
		else if (error == MB_ERROR_NO_ERROR
			&& (kind == MB_DATA_DATA
			    || kind == MB_DATA_NAV)
			&& mbp_heading_mode == MBP_HEADING_OFFSET)
			{
			heading += mbp_headingbias;
			}

		/* if survey data encountered, 
			get the bathymetry */
		if (error == MB_ERROR_NO_ERROR
			&& (kind == MB_DATA_DATA))
			{
			/* extract travel times */
			status = mb_ttimes(verbose,imbio_ptr,
				store_ptr,&kind,&nbeams,
				ttimes,angles,
				angles_forward,angles_null,
				heave,alongtrack_offset,
				&draft,&ssv,&error);

			/* set surface sound speed to default if needed */
			if (ssv <= 0.0)
				ssv = ssv_start;
			else
				ssv_start = ssv;

			/* if svp specified recalculate bathymetry
			    by raytracing  */
			if (mbp_bathrecalc_mode == MBP_BATHRECALC_RAYTRACE)
			    {
			    /* loop over the beams */
			    for (i=0;i<beams_bath;i++)
			      {
			      if (ttimes[i] > 0.0)
				{
				/* if needed, translate angles from takeoff
					angle coordinates to roll-pitch 
					coordinates, apply roll and pitch
					corrections, and translate back */
				if (mbp_rollbias != 0.0 
					|| mbp_pitchbias != 0.0)
					{
					mb_takeoff_to_rollpitch(
						verbose,
						angles[i], angles_forward[i], 
						&alpha, &beta, 
						&error);
					alpha += mbp_pitchbias;
					beta += mbp_rollbias;
					mb_rollpitch_to_takeoff(
						verbose, 
						alpha, beta, 
						&angles[i], &angles_forward[i], 
						&error); 
					}
    
				/* add user specified draft */
				if (mbp_draft_mode == MBP_DRAFT_OFF)
					depth_offset_use = heave[i] + draft;
				else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
					depth_offset_use = heave[i] + draft + mbp_draft;
				else if (mbp_draft_mode == MBP_DRAFT_SET)
					depth_offset_use = heave[i] + mbp_draft;
				static_shift = 0.0;
	
				/* check depth_offset - use static shift if depth_offset negative */
				if (depth_offset_use < 0.0)
				    {
				    fprintf(stderr, "\nWarning: Depth offset negative - transducers above water?!\n");
				    fprintf(stderr, "Raytracing performed from zero depth followed by static shift.\n");
				    fprintf(stderr, "Depth offset is sum of heave + transducer depth.\n");
				    fprintf(stderr, "Draft from data:       %f\n", draft);
				    fprintf(stderr, "Heave from data:       %f\n", heave[i]);
				    fprintf(stderr, "User specified draft:  %f\n", mbp_draft);
				    fprintf(stderr, "Depth offset used:     %f\n", depth_offset_use);
				    fprintf(stderr, "Data Record: %d\n",odata);
				    fprintf(stderr, "Ping time:  %4d %2d %2d %2d:%2d:%2d.%6d\n", 
					    time_i[0], time_i[1], time_i[2], 
					    time_i[3], time_i[4], time_i[5], time_i[6]);
	    
				    static_shift = depth_offset_use;
				    depth_offset_use = 0.0;
				    }

				/* raytrace */
				status = mb_rt(verbose, rt_svp, depth_offset_use, 
					angles[i], 0.5*ttimes[i],
					ssv_mode, ssv, angles_null[i], 
					0, NULL, NULL, NULL, 
					&xx, &zz, 
					&ttime, &ray_stat, &error);
					
				/* apply static shift if needed */
				if (static_shift < 0.0)
				    zz += static_shift;
    
				/* uncorrect depth if desired */
				if (mbp_uncorrected == MB_YES)
				    {
				    k = -1;
				    for (j=0;j<nsvp-1;j++)
					{
					if (depth[j] < zz & depth[j+1] >= zz)
					    k = j;
					}
				    if (k > 0)
					vsum = velocity_sum[k-1];
				    else
					vsum = 0.0;
				    if (k >= 0)
					{
					vsum += 0.5*(2*velocity[k] 
					    + (zz - depth[k])*(velocity[k+1] - velocity[k])
					    /(depth[k+1] - depth[k]))*(zz - depth[k]);
					vavg = vsum / zz;
					zz = zz*1500./vavg;
					}
				    }
    
				/* get alongtrack and acrosstrack distances
					and depth */
				bathacrosstrack[i] = xx*cos(DTR*angles_forward[i]);
				bathalongtrack[i] = xx*sin(DTR*angles_forward[i]);
				bath[i] = zz;
				
				/* output some debug values */
				if (verbose >= 5)
				    fprintf(stderr,"dbg5       %3d %3d %6.3f %6.3f %6.3f %8.2f %8.2f %8.2f\n",
					idata, i, 0.5*ttimes[i], angles[i], angles_forward[i],  
					bathacrosstrack[i], bathalongtrack[i], bath[i]);
    
				/* output some debug messages */
				if (verbose >= 5)
				    {
				    fprintf(stderr,"\ndbg5  Depth value calculated in program <%s>:\n",program_name);
				    fprintf(stderr,"dbg5       kind:  %d\n",kind);
				    fprintf(stderr,"dbg5       beam:  %d\n",i);
				    fprintf(stderr,"dbg5       tt:     %f\n",ttimes[i]);
				    fprintf(stderr,"dbg5       xx:     %f\n",xx);
				    fprintf(stderr,"dbg5       zz:     %f\n",zz);
				    fprintf(stderr,"dbg5       xtrack: %f\n",bathacrosstrack[i]);
				    fprintf(stderr,"dbg5       ltrack: %f\n",bathalongtrack[i]);
				    fprintf(stderr,"dbg5       depth:  %f\n",bath[i]);
				    }
				}
				
			      /* else if no travel time no data */
			      else
				beamflag[i] = MB_FLAG_NULL;
			      }
			    }

			/* if svp not specified recalculate bathymetry
			    by rigid rotations  */
			else if (mbp_bathrecalc_mode == MBP_BATHRECALC_ROTATE)
			    {
			    /* loop over the beams */
			    for (i=0;i<beams_bath;i++)
			      {
			      if (mb_beam_ok(beamflag[i]))
				{
				/* add user specified draft */
				if (mbp_draft_mode == MBP_DRAFT_OFF)
					depth_offset_use = heave[i] + draft;
				else if (mbp_draft_mode == MBP_DRAFT_OFFSET)
					depth_offset_use = heave[i] + draft + mbp_draft;
				else if (mbp_draft_mode == MBP_DRAFT_SET)
					depth_offset_use = heave[i] + mbp_draft;

				/* strip off heave + draft */
				bath[i] -= depth_offset_use;
				
				/* get range and angles in 
				    roll-pitch frame */
				range = sqrt(bath[i] * bath[i] 
					    + bathacrosstrack[i] 
						* bathacrosstrack[i]
					    + bathalongtrack[i] 
						* bathalongtrack[i]);
				alpha = asin(bathalongtrack[i] 
					/ range);
				beta = acos(bathacrosstrack[i] 
					/ range / cos(alpha));

				/* apply roll pitch corrections */
				alpha += DTR * mbp_pitchbias;
				beta +=  DTR * mbp_rollbias;
				
				/* recalculate bathymetry */
				bath[i] 
				    = range * cos(alpha) * sin(beta);
				bathalongtrack[i] 
				    = range * sin(alpha);
				bathacrosstrack[i] 
				    = range * cos(alpha) * cos(beta);	
					
				/* add heave and draft back in */	    
				bath[i] += depth_offset_use;
    
				/* output some debug values */
				if (verbose >= 5)
				    fprintf(stderr,"dbg5       %3d %3d %8.2f %8.2f %8.2f\n",
					idata, i, 
					bathacrosstrack[i], 
					bathalongtrack[i], 
					bath[i]);
    
				/* output some debug messages */
				if (verbose >= 5)
				    {
				    fprintf(stderr,"\ndbg5  Depth value calculated in program <%s>:\n",program_name);
				    fprintf(stderr,"dbg5       kind:  %d\n",kind);
				    fprintf(stderr,"dbg5       beam:  %d\n",i);
				    fprintf(stderr,"dbg5       xtrack: %f\n",bathacrosstrack[i]);
				    fprintf(stderr,"dbg5       ltrack: %f\n",bathalongtrack[i]);
				    fprintf(stderr,"dbg5       depth:  %f\n",bath[i]);
				    }
				}
			      }
			    }

			/* output some debug messages */
			if (verbose >= 5)
			    {
			    fprintf(stderr,"\ndbg5  Depth values calculated in program <%s>:\n",program_name);
			    fprintf(stderr,"dbg5       kind:  %d\n",kind);
			    fprintf(stderr,"dbg5      beam    time      depth        dist\n");	
			    for (i=0;i<nbath;i++)
				fprintf(stderr,"dbg5       %2d   %f   %f   %f   %f\n",
				    i,ttimes[i],
				    bath[i],bathacrosstrack[i],
				    bathalongtrack[i]);
			    }
			}
			
		/* apply the saved edits */
		if (mbp_edit_mode == MBP_EDIT_ON
		    && nedit > 0
		    && error == MB_ERROR_NO_ERROR
		    && kind == MB_DATA_DATA)
		    {
		    for (j=0;j<nedit;j++)
			{
			if (time_d == edit_time_d[j] 
			    && edit_beam[j] >= 0 
			    && edit_beam[j] < nbath)
			    {
			    /* apply edit */
			    if (edit_action[j] == MBP_EDIT_FLAG
				&& mb_beam_ok(beamflag[edit_beam[j]]))
				beamflag[edit_beam[j]] 
				    = MB_FLAG_FLAG + MB_FLAG_MANUAL;
			    else if (edit_action[j] == MBP_EDIT_UNFLAG
				&& !mb_beam_ok(beamflag[edit_beam[j]]))
				beamflag[edit_beam[j]] = MB_FLAG_NONE;
			    else if (edit_action[j] == MBP_EDIT_ZERO)
				beamflag[edit_beam[j]] = MB_FLAG_NULL;
			    }			
			}
		    }

		/* write some data */
		if (error == MB_ERROR_NO_ERROR
			|| kind == MB_DATA_COMMENT)
			{
			status = mb_put_all(verbose,ombio_ptr,
					store_ptr,MB_YES,kind,
					time_i,time_d,
					navlon,navlat,speed,heading,
					nbath,namp,nss,
					beamflag,bath,amp,bathacrosstrack,bathalongtrack,
					ss,ssacrosstrack,ssalongtrack,
					comment,&error);
			if (status == MB_SUCCESS)
				{
				if (kind == MB_DATA_DATA)
					odata++;
				else if (kind == MB_DATA_COMMENT)
					ocomment++;
				}
			else
				{
				mb_error(verbose,error,&message);
				fprintf(stderr,"\nMBIO Error returned from function <mb_put>:\n%s\n",message);
				fprintf(stderr,"\nMultibeam Data Not Written To File <%s>\n",mbp_ofile);
				fprintf(stderr,"Output Record: %d\n",odata+1);
				fprintf(stderr,"Time: %4d %2d %2d %2d:%2d:%2d.%6d\n",
					time_i[0],time_i[1],time_i[2],
					time_i[3],time_i[4],time_i[5],
					time_i[6]);
				fprintf(stderr,"\nProgram <%s> Terminated\n",
					program_name);
				exit(error);
				}
			}
		}

	/* close the files */
	status = mb_close(verbose,&imbio_ptr,&error);
	status = mb_close(verbose,&ombio_ptr,&error);

	/* deallocate memory for data arrays */
	mb_free(verbose,&depth,&error);
	mb_free(verbose,&velocity,&error);
	mb_free(verbose,&velocity_sum,&error);
	mb_free(verbose,&ttimes,&error);
	mb_free(verbose,&angles,&error);
	mb_free(verbose,&angles_forward,&error);
	mb_free(verbose,&angles_null,&error);
	mb_free(verbose,&heave,&error);
	mb_free(verbose,&alongtrack_offset,&error);
	mb_free(verbose,&beamflag,&error); 
	mb_free(verbose,&bath,&error); 
	mb_free(verbose,&bathacrosstrack,&error); 
	mb_free(verbose,&bathalongtrack,&error); 
	mb_free(verbose,&amp,&error); 
	mb_free(verbose,&ss,&error); 
	mb_free(verbose,&ssacrosstrack,&error); 
	mb_free(verbose,&ssalongtrack,&error); 

	/* check memory */
	if (verbose >= 4)
		status = mb_memory_list(verbose,&error);

	/* give the statistics */
	if (verbose >= 1)
		{
		fprintf(stderr,"\n%d input data records\n",idata);
		fprintf(stderr,"%d input comment records\n",icomment);
		fprintf(stderr,"%d output data records\n",odata);
		fprintf(stderr,"%d output comment records\n",ocomment);
		}

	/* end it all */
	exit(error);
}
/*--------------------------------------------------------------------*/
int spline(x,y,n,yp1,ypn,y2)
/* From Numerical Recipies */
double x[],y[],yp1,ypn,y2[];
int n;
{
	int i,k;
	double p,qn,sig,un,*u,*vector();
	void free_vector();

	u=vector(1,n-1);
	if (yp1 > 0.99e30)
		y2[1]=u[1]=0.0;
	else {
		y2[1] = -0.5;
		u[1]=(3.0/(x[2]-x[1]))*((y[2]-y[1])/(x[2]-x[1])-yp1);
	}
	for (i=2;i<=n-1;i++) {
		sig=(x[i]-x[i-1])/(x[i+1]-x[i-1]);
		p=sig*y2[i-1]+2.0;
		y2[i]=(sig-1.0)/p;
		u[i]=(y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
		u[i]=(6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
	}
	if (ypn > 0.99e30)
		qn=un=0.0;
	else {
		qn=0.5;
		un=(3.0/(x[n]-x[n-1]))*(ypn-(y[n]-y[n-1])/(x[n]-x[n-1]));
	}
	y2[n]=(un-qn*u[n-1])/(qn*y2[n-1]+1.0);
	for (k=n-1;k>=1;k--)
		y2[k]=y2[k]*y2[k+1]+u[k];
	free_vector(u,1,n-1);
	return(0);
}
/*--------------------------------------------------------------------*/
int splint(xa,ya,y2a,n,x,y,i)
/* From Numerical Recipies */
double xa[],ya[],y2a[],x,*y;
int n;
int *i;
{
	int klo,khi,k;
	double h,b,a;

	klo=1;
	khi=n;
	while (khi-klo > 1) {
		k=(khi+klo) >> 1;
		if (xa[k] > x) khi=k;
		else klo=k;
	}
	if (khi == 1) khi = 2;
	if (klo == n) klo = n - 1;
	h=xa[khi]-xa[klo];
/*	if (h == 0.0) 
		{
		fprintf(stderr,"ERROR: interpolation time out of nav bounds\n");
		return(-1);
		}
*/
	a=(xa[khi]-x)/h;
	b=(x-xa[klo])/h;
	*y=a*ya[klo]+b*ya[khi]+((a*a*a-a)*y2a[klo]
		+(b*b*b-b)*y2a[khi])*(h*h)/6.0;
	*i=klo;
	return(0);
}
/*--------------------------------------------------------------------*/
int linint(xa,ya,n,x,y,i)
double xa[],ya[],x,*y;
int n;
int *i;
{
	int klo,khi,k;
	double h,b;

	klo=1;
	khi=n;
	while (khi-klo > 1) {
		k=(khi+klo) >> 1;
		if (xa[k] > x) khi=k;
		else klo=k;
	}
	if (khi == 1) khi = 2;
	if (klo == n) klo = n - 1;
	h=xa[khi]-xa[klo];
/*	if (h == 0.0) 
		{
		fprintf(stderr,"ERROR: interpolation time out of nav bounds\n");
		return(-1);
		}
*/
	b = (ya[khi] - ya[klo]) / h;
	*y = ya[klo] + b * (x - xa[klo]);
	*i=klo;
	return(0);
}
/*--------------------------------------------------------------------*/
double *vector(nl,nh)
int nl,nh;
{
	double *v;
	v = (double *) malloc ((unsigned) (nh-nl+1)*sizeof(double));
	if (!v) fprintf(stderr,"allocation failure in vector()");
	return v-nl;
}
/*--------------------------------------------------------------------*/
void free_vector(v,nl,nh)
double *v;
int nl,nh;
{
	free((char*) (v+nl));
}
/*--------------------------------------------------------------------*/