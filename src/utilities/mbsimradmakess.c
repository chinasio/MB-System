/*--------------------------------------------------------------------
 *    The MB-system:	mbsimradmakess.c	11/29/98
 *
 *    $Id: mbsimradmakess.c,v 4.1 1999-02-04 23:55:08 caress Exp $
 *
 *    Copyright (c) 1998 by 
 *    D. W. Caress (caress@lamont.ldgo.columbia.edu)
 *    and D. N. Chayes (dale@lamont.ldgo.columbia.edu)
 *    Lamont-Doherty Earth Observatory
 *    Palisades, NY  10964
 *
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/*
 * 
 * The program mbsimradmakess is an utility for regenerating sidescan
 * imagery from the raw amplitude samples contained in data from 
 * Simrad EM300 and EM3000 multibeam sonars. This program ignores
 * amplitude data associated with flagged (bad) bathymetry data, 
 * thus removing one important source of noise in the
 * sidescan data.
 * 
 * The number of raw samples provided by the sonar for 
 * each ping ranges from a few hundred to
 * as much as ten thousand, depending on the water depth, swath width,
 * and the raw digitization sampling interval. Each bathymetry
 * sounding (or beam) is associated with some number of raw
 * amplitude samples. The acrosstrack locations of the samples are
 * found using the raw sample size (determined by the digitization
 * rate within the sonar) and the beam locations. This program 
 * regenerates sidescan by binning, averaging, and interpolating 
 * the amplitudes into a 1024 pixel array.
 * 
 * The most important control is the output pixel size. The user
 * can set this value to a constant using the -P command. By
 * default, the program adjusts the pixel size for each ping to maintain
 * reasonable swath coverage with 1024 pixels. The desired swath
 * width (in angular terms) can be set by the user using 
 * the -S command; by default
 * the swath width actually achieved by the sonar is used. Regardless
 * of the swath width setting, the pixel size is not allowed to change
 * by more than 5% for any ping.

 * Once the pizel size for a particular ping is set, all of the
 * amplitude samples associated with unflagged (presumably good)
 * bathymetry are binned into the 1024 pixel array, and then averaged.
 * The removal of amplitudes from bad beams may leave gaps in
 * the sidescan; these gaps can be filled by a simple linear
 * interpolation. The -T command sets the maximum size of the
 * gaps (in number of pixels) which can be interpolated. The default
 * is for no interpolation. 
 * 
 * The default input and output streams are stdin and stdout.
 *
 * Author:	D. W. Caress
 * Date:	November 29, 1998
 *
 * $Log: not supported by cvs2svn $
 * Revision 4.0  1998/12/17  22:47:17  caress
 * Initial revision.
 *
 *  *
 */

/* standard include files */
#include <stdio.h>
#include <math.h>
#include <strings.h>
#include <time.h>

/* mbio include files */
#include "../../include/mb_status.h"
#include "../../include/mb_format.h"
#include "../../include/mb_define.h"
#include "../../include/mbsys_simrad2.h"

/*--------------------------------------------------------------------*/

main (argc, argv)
int argc;
char **argv; 
{
	/* id variables */
	static char rcs_id[] = "$Id: mbsimradmakess.c,v 4.1 1999-02-04 23:55:08 caress Exp $";
	static char program_name[] = "MBSIMRADMAKESS";
	static char help_message[] =  "MBSIMRADMAKESS is an utility for regenerating sidescan imagery from the raw amplitude samples contained in data from  Simrad \nEM300 and EM3000 multibeam sonars. This program ignores amplitude \ndata associated with flagged (bad) bathymetry data, thus removing \none important source of noise in the sidescan data. The default \ninput and output streams are stdin and stdout.";
	static char usage_message[] = "mbsimradmakess [-Fformat -V -H  -Iinfile -Ooutfile -Ppixel_size -Sswath_width -Tpixel_int]";

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
	char	*message;

	/* MBIO read and write control parameters */
	int	format = 0;
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
	char	ifile[128];
	char	*imbio_ptr;
	char	ofile[128];
	char	*ombio_ptr;

	/* mbio read and write values */
	char	*store_ptr;
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
	char	*beamflag;
	double	*bath;
	double	*bathacrosstrack;
	double	*bathalongtrack;
	double	*amp;
	double	*ss;
	int	*ss_cnt;
	double	*ssacrosstrack;
	double	*ssalongtrack;
	int	idata = 0;
	int	icomment = 0;
	int	odata = 0;
	int	ocomment = 0;
	char	comment[256];

	/* time, user, host variables */
	time_t	right_now;
	char	date[25], user[128], *user_ptr, host[128];

	/* sidescan handling variables */
	struct mbsys_simrad2_struct *store;
	struct mbsys_simrad2_ping_struct *ping;
	mb_s_char *beam_ss;
	int	nbathsort;
	double	bathsort[MBSYS_SIMRAD2_MAXBEAMS];
	double	median_depth;
	double	depthscale, depthoffset;
	double	dacrscale, daloscale;
	double	reflscale;
	int	swath_width_set = MB_NO;
	int	pixel_size_set = MB_NO;
	double	swath_width;
	double	pixel_size = 0.0;
	double  pixel_size_calc;
	double	ss_spacing;
	int	pixel_int = 0;
	int	pixel_int_use;
	double	xtrack;
	int	first, last, k1, k2;
	int	i, j, k, jj, kk, l, m;

	/* compare function for qsort */
	int mb_double_compare();

	char	*ctime();
	char	*getenv();

	/* get current default values */
	status = mb_defaults(verbose,&format,&pings,&lonflip,bounds,
		btime_i,etime_i,&speedmin,&timegap);

	/* reset all defaults but the lonflip */
	format = MBF_EM300MBA;
	pings = 1;
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
	strcpy (ifile, "stdin");
	strcpy (ofile, "stdout");

	/* process argument list */
	while ((c = getopt(argc, argv, "VvHhF:f:I:i:M:m:O:o:P:p:S:s:T:t:")) != -1)
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
			sscanf (optarg,"%d", &format);
			flag++;
			break;
		case 'I':
		case 'i':
			sscanf (optarg,"%s", ifile);
			flag++;
			break;
		case 'O':
		case 'o':
			sscanf (optarg,"%s", ofile);
			flag++;
			break;
		case 'P':
		case 'p':
			sscanf (optarg,"%lf", &pixel_size);
			pixel_size_set = MB_YES;
			flag++;
			break;
		case 'S':
		case 's':
			sscanf (optarg,"%lf", &swath_width);
			swath_width_set = MB_YES;
			flag++;
			break;
		case 'T':
		case 't':
			sscanf (optarg,"%d", &pixel_int);
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
		exit(MB_FAILURE);
		}

	/* print starting message */
	if (verbose == 1 || help)
		{
		fprintf(stderr,"\nProgram %s\n",program_name);
		fprintf(stderr,"Version %s\n",rcs_id);
		fprintf(stderr,"MB-system Version %s\n",MB_VERSION);
		}

	/* print starting debug statements */
	if (verbose >= 2)
		{
		fprintf(stderr,"\ndbg2  Program <%s>\n",program_name);
		fprintf(stderr,"dbg2  Version %s\n",rcs_id);
		fprintf(stderr,"dbg2  MB-system Version %s\n",MB_VERSION);
		fprintf(stderr,"dbg2  Control Parameters:\n");
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
		fprintf(stderr,"dbg2       input file:      %s\n",ifile);
		fprintf(stderr,"dbg2       output file:     %s\n",ofile);
		if (pixel_size_set == MB_YES)
		    fprintf(stderr,"dbg2       pixel_size:      %f\n",pixel_size);
		if (swath_width_set == MB_YES)
		    fprintf(stderr,"dbg2       swath_width:     %f\n",swath_width);
		fprintf(stderr,"dbg2       pixel_int:       %d\n",pixel_int);
		}

	/* if help desired then print it and exit */
	if (help)
		{
		fprintf(stderr,"\n%s\n",help_message);
		fprintf(stderr,"\nusage: %s\n", usage_message);
		exit(MB_SUCCESS);
		}

	/* if incorrect format specified then print error message and exit */
	if (format != MBF_EM300MBA)
		{
		fprintf(stderr,"\nProgram <%s> only works with format <%d>\n",
			program_name, MBF_EM300MBA);
		fprintf(stderr,"Incorrect format <%d> specified...\n",
			format);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(MB_FAILURE);
		}

	/* initialize reading the input multibeam file */
	if ((status = mb_read_init(
		verbose,ifile,format,pings,lonflip,bounds,
		btime_i,etime_i,speedmin,timegap,
		&imbio_ptr,&btime_d,&etime_d,
		&beams_bath,&beams_amp,&pixels_ss,&error)) != MB_SUCCESS)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error returned from function <mb_read_init>:\n%s\n",message);
		fprintf(stderr,"\nMultibeam File <%s> not initialized for reading\n",ifile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(MB_FAILURE);
		}

	/* initialize writing the output multibeam file */
	if ((status = mb_write_init(
		verbose,ofile,format,&ombio_ptr,
		&beams_bath,&beams_amp,&pixels_ss,&error)) != MB_SUCCESS)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error returned from function <mb_write_init>:\n%s\n",message);
		fprintf(stderr,"\nMultibeam File <%s> not initialized for writing\n",ofile);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(MB_FAILURE);
		}

	/* allocate memory for data arrays */
	status = mb_malloc(verbose,beams_bath*sizeof(char),&beamflag,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),&bath,&error);
	status = mb_malloc(verbose,beams_amp*sizeof(double),&amp,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),
			&bathacrosstrack,&error);
	status = mb_malloc(verbose,beams_bath*sizeof(double),
			&bathalongtrack,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),&ss,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(int),&ss_cnt,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),
			&ssacrosstrack,&error);
	status = mb_malloc(verbose,pixels_ss*sizeof(double),
			&ssalongtrack,&error);

	/* if error initializing memory then quit */
	if (error != MB_ERROR_NO_ERROR)
		{
		mb_error(verbose,error,&message);
		fprintf(stderr,"\nMBIO Error allocating data arrays:\n%s\n",message);
		fprintf(stderr,"\nProgram <%s> Terminated\n",
			program_name);
		exit(MB_FAILURE);
		}

	/* write comments to beginning of output file */
	kind = MB_DATA_COMMENT;
	sprintf(comment,"Sidescan imagery regenerated by program %s version %s",
		program_name,rcs_id);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
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
	gethostname(host,128);
	sprintf(comment,"Run by user <%s> on cpu <%s> at <%s>",
		user,host,date);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"Control Parameters:");
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"  MBIO data format:   %d",format);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"  Input file:         %s",ifile);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"  Output file:        %s",ofile);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"  Pixel size:        %f m",pixel_size);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment,"  Swath width:       %f deg",swath_width);
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;
	sprintf(comment," ");
	status = mb_put_comment(verbose,ombio_ptr,comment,&error);
	if (error == MB_ERROR_NO_ERROR) ocomment++;

	/* read and write */
	while (error <= MB_ERROR_NO_ERROR)
		{
		/* read some data */
		error = MB_ERROR_NO_ERROR;
		status = MB_SUCCESS;
		status = mb_get_all(verbose,imbio_ptr,&store_ptr,&kind,
				time_i,&time_d,&navlon,&navlat,&speed,
				&heading,&distance,
				&beams_bath,&beams_amp,&pixels_ss,
				beamflag,bath,amp,bathacrosstrack,bathalongtrack,
				ss,ssacrosstrack,ssalongtrack,
				comment,&error);

		/* increment counter */
		if (error <= MB_ERROR_NO_ERROR 
			&& kind == MB_DATA_DATA)
			idata = idata + pings;
		else if (error <= MB_ERROR_NO_ERROR 
			&& kind == MB_DATA_COMMENT)
			icomment++;

		/* time gaps do not matter to mbsimradmakess */
		if (error == MB_ERROR_TIME_GAP)
			{
			status = MB_SUCCESS;
			error = MB_ERROR_NO_ERROR;
			}

		/* non-survey data do not matter to mbsimradmakess */
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
			fprintf(stderr,"Time: %d %d %d %d %d %d %d\n",
				time_i[0],time_i[1],time_i[2],
				time_i[3],time_i[4],time_i[5],
				time_i[6]);
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
			fprintf(stderr,"Last Good Time: %d %d %d %d %d %d %d\n",
				time_i[0],time_i[1],time_i[2],
				time_i[3],time_i[4],time_i[5],
				time_i[6]);
			}

		/* process the sidescan */
		if (error == MB_ERROR_NO_ERROR
			&& kind == MB_DATA_DATA)
			{
			/* get pointers to raw data structure */
			store = (struct mbsys_simrad2_struct *) store_ptr;
			ping = (struct mbsys_simrad2_ping_struct *) store->ping;

			/* zero the sidescan */
			for (i=0;i<MBSYS_SIMRAD2_MAXPIXELS;i++)
				{
				ss[i] = 0.0;
				ssacrosstrack[i] = 0.0;
				ssalongtrack[i] = 0.0;
				ss_cnt[i] = 0;
				}

			/* set scaling parameters */
			depthscale = 0.01 * ping->png_depth_res;
			depthoffset = 0.01 * ping->png_xducer_depth
					+ 655.36 * ping->png_offset_multiplier;
			dacrscale  = 0.01 * ping->png_distance_res;
			daloscale  = 0.01 * ping->png_distance_res;
			reflscale  = 0.5;

			/* get median depth */
			nbathsort = 0;
			for (i=0;i<ping->png_nbeams;i++)
			    {
			    if (mb_beam_ok(ping->png_beamflag[i]))
				{
				bathsort[nbathsort] = depthscale 
					* ping->png_depth[i]
					    + depthoffset;
 				nbathsort++;
				}
			    }
			if (nbathsort > 0)
			    {
			    qsort((char *)bathsort, nbathsort, sizeof(double),mb_double_compare);
			    median_depth = bathsort[nbathsort/2];
			    }
		
			/* get sidescan pixel size */
			if (swath_width_set == MB_NO)
			    {
			    swath_width = 2.5 + MAX(90.0 - 0.01 * ping->png_depression[0], 
					    90.0 - 0.01 * ping->png_depression[ping->png_nbeams-1]);
			    }
			if (pixel_size_set == MB_NO)
			    {
			    pixel_size_calc = 2 * tan(DTR * swath_width) * bathsort[nbathsort/2] 
						/ MBSYS_SIMRAD2_MAXPIXELS;
			    pixel_size_calc = MAX(pixel_size_calc, bathsort[nbathsort/2] * sin(DTR * 0.1));
			    if (pixel_size <= 0.0)
				pixel_size = pixel_size_calc;
			    else if (0.95 * pixel_size > pixel_size_calc)
				pixel_size = 0.95 * pixel_size;
			    else if (1.05 * pixel_size < pixel_size_calc)
				pixel_size = 1.05 * pixel_size;
			    else
				pixel_size = pixel_size_calc;
			    }
	
			/* get raw pixel size */
			if (store->sonar == 300 || store->sonar == 3000)
			    ss_spacing = 750.0 / ping->png_sample_rate;
			else
			    ss_spacing = 750.0 / 14000;
			    
			/* get pixel interpolation */
			pixel_int_use = pixel_int + 1;
	
			/* loop over raw sidescan, putting each raw pixel into
				the binning arrays */
			pixels_ss = MBSYS_SIMRAD2_MAXPIXELS;
			for (i=0;i<ping->png_nbeams_ss;i++)
				{
				beam_ss = &ping->png_ssraw[ping->png_start_sample[i]];
				j = ping->png_beam_num[i] - 1;
				if (mb_beam_ok(beamflag[j]))
				    for (k=0;k<ping->png_beam_samples[i];k++)
					{
					if (beam_ss[k] != EM2_INVALID_AMP)
						{
						/* interpolate based on range */
						if (k == ping->png_center_sample[i])
						    {
						    xtrack = bathacrosstrack[j];
						    }
						else if (i == ping->png_nbeams_ss - 1 
						    || (k <= ping->png_center_sample[i]
							&& i != 0))
						    {
						    if (ping->png_range[i] != ping->png_range[i-1])
							{
							jj = ping->png_beam_num[i-1] - 1;
							xtrack = bathacrosstrack[j]
							    + (bathacrosstrack[j]
								- bathacrosstrack[jj])
							    * 2 *((double)(k - ping->png_center_sample[i]))
							    / fabs((double)(ping->png_range[i] - ping->png_range[i-1]));
							}
						    else
							{
							xtrack = bathacrosstrack[j]
							    + ss_spacing * (k - ping->png_center_sample[i]);
							}
						    }
						else
						    {
						    if (ping->png_range[i] != ping->png_range[i+1])
							{
							jj = ping->png_beam_num[i+1] - 1;
							    
							xtrack = bathacrosstrack[j]
							    + (bathacrosstrack[jj]
								- bathacrosstrack[j])
							    * 2 *((double)(k - ping->png_center_sample[i]))
							    / fabs((double)(ping->png_range[i+1] - ping->png_range[i]));
							}
						    else
							{
							xtrack = bathacrosstrack[j]
							    + ss_spacing * (k - ping->png_center_sample[i]);
							}
						    }
						kk = MBSYS_SIMRAD2_MAXPIXELS / 2 
						    + (int)(xtrack / pixel_size);
						if (kk > 0 && kk < MBSYS_SIMRAD2_MAXPIXELS)
						    {
						    ss[kk] 
							    += reflscale*((double)beam_ss[k]) + 64.0;
						    ssalongtrack[kk] 
							    += bathalongtrack[j];
						    ss_cnt[kk]++;
						    }
						}
					}
				}
				
			/* average the sidescan */
			first = MBSYS_SIMRAD2_MAXPIXELS;
			last = -1;
			for (k=0;k<MBSYS_SIMRAD2_MAXPIXELS;k++)
				{
				if (ss_cnt[k] > 0)
					{
					ss[k] /= ss_cnt[k];
					ssalongtrack[k] /= ss_cnt[k];
					ssacrosstrack[k] 
						= (k - MBSYS_SIMRAD2_MAXPIXELS / 2)
							* pixel_size;
					first = MIN(first, k);
					last = k;
					}
				}
				
			/* interpolate the sidescan */
			k1 = first;
			k2 = first;
			for (k=first+1;k<last;k++)
			    {
			    if (ss_cnt[k] <= 0)
				{
				if (k2 <= k)
				    {
				    k2 = k+1;
				    while (ss_cnt[k2] <= 0 && k2 < last)
					k2++;
				    }
				if (k2 - k1 <= pixel_int_use)
				    {
				    ss[k] = ss[k1]
					+ (ss[k2] - ss[k1])
					    * ((double)(k - k1)) / ((double)(k2 - k1));
				    ssacrosstrack[k] 
					    = (k - MBSYS_SIMRAD2_MAXPIXELS / 2)
						    * pixel_size;
				    ssalongtrack[k] = ssalongtrack[k1]
					+ (ssalongtrack[k2] - ssalongtrack[k1])
					    * ((double)(k - k1)) / ((double)(k2 - k1));
				    }
				}
			    else
				{
				k1 = k;
				}
			    }
	
			/* print debug statements */
			if (verbose >= 2)
				{
				fprintf(stderr,"\ndbg2  Sidescan regenerated in <%s>\n",
					program_name);
				fprintf(stderr,"dbg2       longitude:  %f\n",
					navlon);
				fprintf(stderr,"dbg2       latitude:   %f\n",
					navlat);
				fprintf(stderr,"dbg2       speed:      %f\n",
					speed);
				fprintf(stderr,"dbg2       heading:    %f\n",
					heading);
				fprintf(stderr,"dbg2       beams_bath: %d\n",
					beams_bath);
				fprintf(stderr,"dbg2       beams_amp:  %d\n",
					beams_amp);
				for (i=0;i<beams_bath;i++)
				  fprintf(stderr,"dbg2       beam:%d  flag:%3d  bath:%f  amp:%f  acrosstrack:%f  alongtrack:%f\n",
					i,beamflag[i],
					bath[i],
					amp[i],
					bathacrosstrack[i],
					bathalongtrack[i]);
				fprintf(stderr,"dbg2       pixels_ss:  %d\n",
					pixels_ss);
				for (i=0;i<pixels_ss;i++)
				  fprintf(stderr,"dbg2       pixel:%4d  cnt:%3d  ss:%10f  xtrack:%10f  ltrack:%10f\n",
					i,ss_cnt[i],ss[i],
					ssacrosstrack[i],
					ssalongtrack[i]);
				}
				
			/* insert the new sidescan into store */
			ping->png_pixel_size = (int) (100 * pixel_size);
			if (last > first)
			    ping->png_pixels_ss = 2 * MAX(MBSYS_SIMRAD2_MAXPIXELS/2 - first, 
						last - MBSYS_SIMRAD2_MAXPIXELS/2);
			else 
			    ping->png_pixels_ss = 0;
			for (i=0;i<MBSYS_SIMRAD2_MAXPIXELS;i++)
			    {
			    ping->png_ss[i] = (short)(100 * ss[i]);
			    ping->png_ssalongtrack[i] 
				    = (short)(ssalongtrack[i] / daloscale);
			    }
			}

		/* write some data */
		if (error == MB_ERROR_NO_ERROR)
			{
			status = mb_put_all(verbose,ombio_ptr,
					store_ptr,MB_NO,kind,
					time_i,time_d,
					navlon,navlat,speed,heading,
					beams_bath,beams_amp,pixels_ss,
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
				fprintf(stderr,"\nMultibeam Data Not Written To File <%s>\n",ofile);
				fprintf(stderr,"Output Record: %d\n",odata+1);
				fprintf(stderr,"Time: %d %d %d %d %d %d %d\n",
					time_i[0],time_i[1],time_i[2],
					time_i[3],time_i[4],time_i[5],
					time_i[6]);
				fprintf(stderr,"\nProgram <%s> Terminated\n",
					program_name);
				exit(MB_FAILURE);
				}
			}
		}

	/* close the files */
	status = mb_close(verbose,&imbio_ptr,&error);
	status = mb_close(verbose,&ombio_ptr,&error);

	/* deallocate memory for data arrays */
	mb_free(verbose,&beamflag,&error); 
	mb_free(verbose,&bath,&error); 
	mb_free(verbose,&amp,&error); 
	mb_free(verbose,&bathacrosstrack,&error); 
	mb_free(verbose,&bathalongtrack,&error); 
	mb_free(verbose,&ss,&error); 
	mb_free(verbose,&ss_cnt,&error); 
	mb_free(verbose,&ssacrosstrack,&error); 
	mb_free(verbose,&ssalongtrack,&error); 

	/* check memory */
	if (verbose >= 4)
		status = mb_memory_list(verbose,&error);

	/* give the statistics */
	if (verbose >= 1)
		{
		fprintf(stderr,"%d input data records\n",idata);
		fprintf(stderr,"%d input comment records\n",icomment);
		fprintf(stderr,"%d output data records\n",odata);
		fprintf(stderr,"%d output comment records\n",ocomment);
		}

	/* end it all */
	exit(status);
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
int splint(xa,ya,y2a,n,x,y)
/* From Numerical Recipies */
double xa[],ya[],y2a[],x,*y;
int n;
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
	h=xa[khi]-xa[klo];
	if (h == 0.0) 
		{
		fprintf(stderr,"ERROR: interpolation time out of tide bounds\n");
		return(-1);
		}
	a=(xa[khi]-x)/h;
	b=(x-xa[klo])/h;
	*y=a*ya[klo]+b*ya[khi]+((a*a*a-a)*y2a[klo]
		+(b*b*b-b)*y2a[khi])*(h*h)/6.0;
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