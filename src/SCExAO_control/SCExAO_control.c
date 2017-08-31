/**
 * @file    SCExAO_control.c
 * @brief   misc SCExAO-specific control routines
 * 
 * Alignment, some processing etc...
 *  
 * @author  O. Guyon
 * @date    25 Aug 2017
 *
 * 
 * @bug No known bugs.
 * 
 */

#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <math.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <ncurses.h>
#include <semaphore.h>


#include <fitsio.h>

#include "CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "linopt_imtools/linopt_imtools.h"
#include "image_filter/image_filter.h"
#include "image_basic/image_basic.h"
#include "info/info.h"
#include "fft/fft.h"
#include "ZernikePolyn/ZernikePolyn.h"

#include "SCExAO_control/SCExAO_control.h"

# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000
# endif


/** SCExAO instrument control
 * 
 * These routines only work on SCExAO computer, and are specific to SCExAO system
 * 
 */



extern DATA data;

static float PcamPixScaleAct = 0.7*10000.0; // pyramid re-image actuators: step per pixel

/// CONFIGURATION
/// CONFIGURATION
static char WFScam_name[200];
static long long WFScnt = 0;

static long pXsize = 120;
static long pYsize = 120;

static long SCExAO_DM_STAGE_Xpos = 0;
static long SCExAO_DM_STAGE_Ypos = 0;

static long SCExAO_Pcam_Xpos0 = 60000;
static long SCExAO_Pcam_Ypos0 = 62000;
static long SCExAO_Pcam_Xpos = 60000;
static long SCExAO_Pcam_Ypos = 62000;
static long SCExAO_Pcam_Range = 50000;

static float SCExAO_PZT_STAGE_Xpos = -5.0;
static float SCExAO_PZT_STAGE_Xpos_ref = -5.0;
static float SCExAO_PZT_STAGE_Xpos_min = -7.0;
static float SCExAO_PZT_STAGE_Xpos_max = -3.0;

static float SCExAO_PZT_STAGE_Ypos = -5.0;
static float SCExAO_PZT_STAGE_Ypos_ref = -5.0;
static float SCExAO_PZT_STAGE_Ypos_min = -7.0;
static float SCExAO_PZT_STAGE_Ypos_max = -3.0;




// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string, not existing image
// 4: existing image
// 5: string


int_fast8_t SCExAOcontrol_mkSegmentModes_cli()
{
	if(CLI_checkarg(1,4)+CLI_checkarg(2,3)==0)
    {

        SCExAOcontrol_mkSegmentModes(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string);
        return 0;
    }
    else
        return 1;
}



int_fast8_t SCExAOcontrol_mv_DMstage_cli()
{
    if(CLI_checkarg(1,2)+CLI_checkarg(2,2)==0)
    {
        SCExAOcontrol_mv_DMstage(data.cmdargtoken[1].val.numl, data.cmdargtoken[2].val.numl);
        return 0;
    }
    else
        return 1;
}



int_fast8_t SCExAOcontrol_PyramidWFS_AutoAlign_TT_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,1)+CLI_checkarg(3,1)==0)
    {
        SCExAOcontrol_PyramidWFS_AutoAlign_TT(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.numf, data.cmdargtoken[3].val.numf);
        return 0;
    }
    else
        return 1;
}


int_fast8_t SCExAOcontrol_PyramidWFS_AutoAlign_cam_cli()
{
    if(CLI_checkarg(1,4)==0)
    {
        SCExAOcontrol_PyramidWFS_AutoAlign_cam(data.cmdargtoken[1].val.string);
        return 0;
    }
    else
        return 1;
}


int_fast8_t SCExAOcontrol_PyramidWFS_Pcenter_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,1)+CLI_checkarg(3,1)==0)
    {
        SCExAOcontrol_PyramidWFS_Pcenter(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.numf, data.cmdargtoken[3].val.numf);
        return 0;
    }
    else
        return 1;
}


int_fast8_t SCExAOcontrol_Pyramid_flattenRefWF_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,2)+CLI_checkarg(3,1)==0)
    {
        SCExAOcontrol_Pyramid_flattenRefWF(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.numl, data.cmdargtoken[3].val.numf);
        return 0;
    }
    else
        return 1;
}


int_fast8_t SCExAOcontrol_optPSF_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,2)+CLI_checkarg(3,1)==0)
    {
        SCExAOcontrol_optPSF(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.numl, data.cmdargtoken[3].val.numf);
        return 0;
    }
    else
        return 1;
}





int_fast8_t SCExAOcontrol_SAPHIRA_cam_process_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,3)==0)
    {
        SCExAOcontrol_SAPHIRA_cam_process(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string);
        return 0;
    }
    else
        return 1;
}


int_fast8_t SCExAOcontrol_vib_ComputeCentroid_cli()
{
	 if(CLI_checkarg(1,4)+CLI_checkarg(2,4)+CLI_checkarg(3,3)==0)
    {
        SCExAOcontrol_vib_ComputeCentroid(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string, data.cmdargtoken[3].val.string);
        return 0;
    }
    else
        return 1;
}



int_fast8_t SCExAOcontrol_vib_mergeData_cli()
{
	 if(CLI_checkarg(1,4)+CLI_checkarg(2,4)+CLI_checkarg(3,3)+CLI_checkarg(4,2)==0)
    {
        SCExAOcontrol_vib_mergeData(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string, data.cmdargtoken[3].val.string, data.cmdargtoken[4].val.numl);
        return 0;
    }
    else
        return 1;
}










int_fast8_t init_SCExAO_control()
{

    strcpy(data.module[data.NBmodule].name, __FILE__);
    strcpy(data.module[data.NBmodule].info, "SCExAO control");
    data.NBmodule++;


    strcpy(data.cmd[data.NBcmd].key,"scexaomksegmodes");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_mkSegmentModes_cli;
    strcpy(data.cmd[data.NBcmd].info,"make segments modes from dm map");
    strcpy(data.cmd[data.NBcmd].syntax,"<dmmap> <segmap>");
    strcpy(data.cmd[data.NBcmd].example,"scexaomksegmodes dmmap segmodes");
    strcpy(data.cmd[data.NBcmd].Ccall,"long SCExAOcontrol_mkSegmentModes(const char *IDdmmap_name, const char *IDout_name)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"scexaottdmpos");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_mv_DMstage_cli;
    strcpy(data.cmd[data.NBcmd].info,"move DM TT stage to position");
    strcpy(data.cmd[data.NBcmd].syntax,"<x pos> <y pos>");
    strcpy(data.cmd[data.NBcmd].example,"scexaottdmpos");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_mv_DMstage(long stepXpos, long stepYpos)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"scexaopywfsttalign");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_PyramidWFS_AutoAlign_TT_cli;
    strcpy(data.cmd[data.NBcmd].info,"move TT to center pyrWFS");
    strcpy(data.cmd[data.NBcmd].syntax,"<wfscamname> <XposStart> <YposStart>");
    strcpy(data.cmd[data.NBcmd].example,"scexaopywfsttalign wfscam -5.5 -4.5");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_PyramidWFS_AutoAlign_TT(const char *WFScam_name, float XposStart, float YposStart);");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"scexaopywfscamalign");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_PyramidWFS_AutoAlign_cam_cli;
    strcpy(data.cmd[data.NBcmd].info,"move Camera to center pyrWFS");
    strcpy(data.cmd[data.NBcmd].syntax,"<wfscamname>");
    strcpy(data.cmd[data.NBcmd].example,"scexaopywfscamalign");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_PyramidWFS_AutoAlign_cam();");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"scexaopypcent");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_PyramidWFS_Pcenter_cli;
    strcpy(data.cmd[data.NBcmd].info,"center pyrWFS pupil");
    strcpy(data.cmd[data.NBcmd].syntax,"<wfsimname> <pup radius [float]>");
    strcpy(data.cmd[data.NBcmd].example,"scexaopypcent wfsim 25.0");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_PyramidWFS_Pcenter(const char *IDwfsname, float prad)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"scexaopyflatten");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_Pyramid_flattenRefWF_cli;
    strcpy(data.cmd[data.NBcmd].info,"flatten  pyrWFS");
    strcpy(data.cmd[data.NBcmd].syntax,"<wfscamname> <NB zern> <ampl>");
    strcpy(data.cmd[data.NBcmd].example,"scexaopyflatten wfsim 20 0.05");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_Pyramid_flattenRefWF(const char *WFScam_name, long zimaxmax, float ampl0);");
    data.NBcmd++;


	strcpy(data.cmd[data.NBcmd].key,"scexaoPSFopt");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_optPSF_cli;
    strcpy(data.cmd[data.NBcmd].info,"optimize PSF shape");
    strcpy(data.cmd[data.NBcmd].syntax,"<wfscamname> <NB modes> <ampl>");
    strcpy(data.cmd[data.NBcmd].example,"scexaoPSFopt wfsim 20 0.05");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_optPSF(const char *WFScam_name, long zimaxmax, float alpha)");
    data.NBcmd++;
     

    strcpy(data.cmd[data.NBcmd].key,"scexaosaphiraproc");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_SAPHIRA_cam_process_cli;
    strcpy(data.cmd[data.NBcmd].info,"process saphira camera images");
    strcpy(data.cmd[data.NBcmd].syntax,"<input> <output>");
    strcpy(data.cmd[data.NBcmd].example,"scexaosaphiraproc");
    strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_SAPHIRA_cam_process(const char *IDinname, const char *IDoutname)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key, "scexaostreamcentr");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_vib_ComputeCentroid_cli;
    strcpy(data.cmd[data.NBcmd].info, "compute centroid of image stream");
    strcpy(data.cmd[data.NBcmd].syntax, "<input stream> <dark image> <output stream>");
    strcpy(data.cmd[data.NBcmd].example, "scexaostreamcentr");
    strcpy(data.cmd[data.NBcmd].Ccall, "long SCExAOcontrol_vib_ComputeCentroid(const char *IDin_name, const char *IDdark_name, const char *IDout_name)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key, "scexaovibmerge");
    strcpy(data.cmd[data.NBcmd].module, __FILE__);
    data.cmd[data.NBcmd].fp = SCExAOcontrol_vib_mergeData_cli;
    strcpy(data.cmd[data.NBcmd].info, "merge accelerometer and position data");
    strcpy(data.cmd[data.NBcmd].syntax, "<acc stream> <pos stream> <output stream> <mode>");
    strcpy(data.cmd[data.NBcmd].example, "scexaovibmerge acc pos out 0");
    strcpy(data.cmd[data.NBcmd].Ccall, "long SCExAOcontrol_vib_mergeData(const char *IDacc_name, const char *IDttpos_name, const char *IDout_name, int mode)");
    data.NBcmd++;



    // add atexit functions here


    return 0;
}



//
// optional : use dmmask
//
long SCExAOcontrol_mkSegmentModes(const char *IDdmmap_name, const char *IDout_name)
{
	long IDdmmap, IDout;
	long ii, jj;
	long size, size2;
	double xc, yc, r0, r1;
	double x, y, r;
	double lim0, lim;
	double limstep = 0.99;
	double limstop;
	double val, val1, rms;
	
	int *segarray;
	int nbseg = 4;
	
	int *segarrayn;
	float *segarrayv;
	long ii1, jj1;
	long NBpixelAdded = 0;
	long NBoffpix = 0;
	long cnt1 = 0;
	long kk, seg;
	long cnt;
	
	
	long IDmask;
	
	
	IDdmmap = image_ID(IDdmmap_name);
	size = data.image[IDdmmap].md[0].size[0];
	size2 = size*size;
	

	lim0 = img_percentile(IDdmmap_name, 0.90);
	limstop = lim0*0.001;
	
	IDmask = image_ID("dmmask");
	if(IDmask!=-1)
		for(ii=0;ii<size2;ii++)
			data.image[IDdmmap].array.F[ii] += (2.0*limstop)*data.image[IDmask].array.F[ii];
	
	
	segarray = (int*) malloc(sizeof(int)*size*size);
	segarrayn = (int*) malloc(sizeof(int)*size*size); // proposed new allocation
	segarrayv = (float*) malloc(sizeof(float)*size*size); // proposed new allocation pixel strength
	
	
	IDout = create_3Dimage_ID(IDout_name, size, size, nbseg*3);
	
	for(ii=0; ii<size; ii++)
		for(jj=0; jj<size; jj++)
			data.image[IDout].array.F[jj*size+ii] = 0.0;
				
	// initial segment allocation
	r0 = 0.25*size;
	r1 = 3.0;
	
	for(ii=0;ii<size;ii++)
		for(jj=0; jj<size; jj++)
			{
				segarray[jj*size+ii] = 0;				
			}
	
	xc = 0.5*size + r0;
	yc = 0.5*size;
	for(ii=0;ii<size;ii++)
		for(jj=0; jj<size; jj++)
			{
				x = 1.0*ii - xc;
				y = 1.0*jj - yc;
				r = sqrt(x*x+y*y);
				if(r<r1)
					segarray[jj*size+ii] = 1;
			}
	
	xc = 0.5*size;
	yc = 0.5*size + r0;
	for(ii=0;ii<size;ii++)
		for(jj=0; jj<size; jj++)
			{
				x = 1.0*ii - xc;
				y = 1.0*jj - yc;
				r = sqrt(x*x+y*y);
				if(r<r1)
					segarray[jj*size+ii] = 2;
			}
	
	xc = 0.5*size - r0;
	yc = 0.5*size;
	for(ii=0;ii<size;ii++)
		for(jj=0; jj<size; jj++)
			{
				x = 1.0*ii - xc;
				y = 1.0*jj - yc;
				r = sqrt(x*x+y*y);
				if(r<r1)
					segarray[jj*size+ii] = 3;
			}
	
	xc = 0.5*size;
	yc = 0.5*size - r0;
	for(ii=0;ii<size;ii++)
		for(jj=0; jj<size; jj++)
			{
				x = 1.0*ii - xc;
				y = 1.0*jj - yc;
				r = sqrt(x*x+y*y);
				if(r<r1)
					segarray[jj*size+ii] = 4;
			}
	
	
	
	lim = lim0;
	while (lim > limstop)
	{
		for(ii=0;ii<size;ii++)
			for(jj=0; jj<size; jj++)
				{
					segarrayn[jj*size+ii] = 0;
					segarrayv[jj*size+ii] = 0.0;				
				}
				
		NBpixelAdded = 0;
		NBoffpix = 0;
		for(ii=0;ii<size;ii++)
			for(jj=0; jj<size; jj++)
			{
				if(segarray[jj*size+ii] == 0) // pixel not yet allocated
				{
					NBoffpix ++;
					// testing pixel on left
					ii1 = ii-1;
					jj1 = jj;
					if(ii1>-1)
						if(segarray[jj1*size+ii1] != 0)
						{
							cnt1++;
							val = data.image[IDdmmap].array.F[jj1*size+ii1];
							if((val > segarrayv[jj*size+ii])&&(val>lim))
								{
									segarrayv[jj*size+ii] = val;
									segarrayn[jj*size+ii] = segarray[jj1*size+ii1];
								}								
						}

					// testing pixel on top
					ii1 = ii;
					jj1 = jj+1;
					if(jj1<size)
						if(segarray[jj1*size+ii1] != 0)
						{
							cnt1++;
							val = data.image[IDdmmap].array.F[jj1*size+ii1];
							if((val > segarrayv[jj*size+ii])&&(val>lim))
								{
									segarrayv[jj*size+ii] = val;
									segarrayn[jj*size+ii] = segarray[jj1*size+ii1];
								}								
						}

					// testing pixel on right
					ii1 = ii+1;
					jj1 = jj;
					if(ii1<size)
						if(segarray[jj1*size+ii1] != 0)
						{
							cnt1++;
							val = data.image[IDdmmap].array.F[jj1*size+ii1];
							if((val > segarrayv[jj*size+ii])&&(val>lim))
								{
									segarrayv[jj*size+ii] = val;
									segarrayn[jj*size+ii] = segarray[jj1*size+ii1];
								}								
						}

					// testing pixel on bottom
					ii1 = ii;
					jj1 = jj-1;
					if(jj1>-1)
						if(segarray[jj1*size+ii1] != 0)
						{
							cnt1++;
							val = data.image[IDdmmap].array.F[jj1*size+ii1];
							if((val > segarrayv[jj*size+ii])&&(val>lim))
								{
									segarrayv[jj*size+ii] = val;
									segarrayn[jj*size+ii] = segarray[jj1*size+ii1];
								}								
						}		
				}
			}
		
		for(ii=0;ii<size;ii++)
			for(jj=0; jj<size; jj++)
				if(segarray[jj*size+ii] == 0) // pixel not yet allocated
					if(segarrayn[jj*size+ii] != 0)
					{
						segarray[jj*size+ii] = segarrayn[jj*size+ii];
						NBpixelAdded++;
					} 
	
		printf("limit = %20g      %6ld pix off   %6ld pix tested     ->  %6ld pixels added\n", lim, NBoffpix, cnt1, NBpixelAdded);
				
		lim *= limstep;
	}
	
	
	
	kk = 0;
	for(seg=1;seg<nbseg+1;seg++)
	{
		xc = 0.0;
		yc = 0.0;
		cnt = 0;
		
		// piston
		for(ii=0; ii<size; ii++)
			for(jj=0; jj<size; jj++)
			{	
				if(segarray[jj*size+ii] == seg)
					{
						xc += 1.0*ii;
						yc += 1.0*jj;
						cnt++;
						data.image[IDout].array.F[kk*size2+jj*size+ii] = 1.0;
					}
			}
		
		xc /= cnt;
		yc /= cnt;
		kk++;
		
		// tip
		for(ii=0; ii<size; ii++)
			for(jj=0; jj<size; jj++)
			{
				if(segarray[jj*size+ii] == seg)
					{
						data.image[IDout].array.F[kk*size2+jj*size+ii] = 1.0*ii-xc;
					}
			}
		kk++;
		
		// tilt
		for(ii=0; ii<size; ii++)
			for(jj=0; jj<size; jj++)
			{
				if(segarray[jj*size+ii] == seg)
					{
						data.image[IDout].array.F[kk*size2+jj*size+ii] = 1.0*jj-yc;
					}
			}
		kk++;
	}
		
		
	if(IDmask!=-1)
	{
		for(kk=0;kk<nbseg*3;kk++)
		{
			val = 0.0;
			val1 = 0.0;
			for(ii=0; ii<size2; ii++)
			{
				data.image[IDout].array.F[kk*size2+ii] *= data.image[IDmask].array.F[ii];
				val += data.image[IDout].array.F[kk*size2+ii];
				val1 += data.image[IDmask].array.F[ii];
			}
			rms = 0.0;
			for(ii=0; ii<size2; ii++)
				{
					data.image[IDout].array.F[kk*size2+ii] -= data.image[IDmask].array.F[ii] * val/val1;
					rms += data.image[IDout].array.F[kk*size2+ii]*data.image[IDout].array.F[kk*size2+ii];
				}
			
			for(ii=0; ii<size2; ii++)
			{
				data.image[IDout].array.F[kk*size2+ii] /= sqrt(rms/val1);
			}

		}
	}		
	
	
	free(segarray);
	free(segarrayn);
	free(segarrayv);
	
	list_image_ID();
	
	return(IDout);
}








/** \brief Move DM stage 
 * 
 *  Absolute position
 * 
 */

int SCExAOcontrol_mv_DMstage(long stepXpos, long stepYpos)
{
    char command[200];
    long ABoffset = 200; /// anti-backlash offset - rule: go negative first, and then positive
    int r;
    long delayus = 2000000;
    long stepX, stepY;

    stepX = stepXpos - SCExAO_DM_STAGE_Xpos;
    stepY = stepYpos - SCExAO_DM_STAGE_Ypos;

   printf("X: %ld -> %ld      Y: %ld -> %ld\n", SCExAO_DM_STAGE_Xpos, stepXpos, SCExAO_DM_STAGE_Ypos, stepYpos);
   printf("Moving by  %ld x %ld\n", stepX, stepY);

    if((fabs(stepX)>500.0)||(fabs(stepY)>500)||(fabs(stepXpos)>1000.0)||(fabs(stepYpos)>1000))
    {
        printf("ERROR: motion is too large or out of range: ignoring command\n");
    }
    else
    {

        if(stepX!=0)
        {
            if(stepX>ABoffset)
            {
                sprintf(command, "dm_stage x push %ld\n", stepX);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);
            }
            else
            {
                sprintf(command, "dm_stage x push %ld\n", stepX-ABoffset);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);

                sprintf(command, "dm_stage x push %ld\n", ABoffset);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);
            }

            SCExAO_DM_STAGE_Xpos += stepX;
        }

        if(stepY!=0)
        {
            if(stepY>ABoffset)
            {
                sprintf(command, "dm_stage y push %ld\n", stepY);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);
            }
            else
            {
                sprintf(command, "dm_stage y push %ld\n", stepY-ABoffset);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);

                sprintf(command, "dm_stage y push %ld\n", ABoffset);
                printf("command : %s\n", command);
                r = system(command);
                usleep(delayus);
            }

            SCExAO_DM_STAGE_Ypos += stepY;
        }
    }

    return(0);
}









/** auto aligns tip-tilt by equalizing fluxes between quadrants */

int SCExAOcontrol_PyramidWFS_AutoAlign_TT_DM(const char *WFScam_name)
{
    long ID;
    long xsize, ysize;
    long ii, jj;
    double tot00, tot01, tot10, tot11, tot;
    double xsig, ysig;
    long ttxpos, ttypos;
    double gain = 1.0;


  //  ID = SCExAOcontrol_Average_image(WFScam_name, 5000, "imwfs", 7);
    
    ID = IMAGE_BASIC_streamaverage(WFScam_name, 5000, "imwfs", 0, 7);
    xsize = data.image[ID].md[0].size[0];
    ysize = data.image[ID].md[0].size[1];

    printf("%ld x %ld image\n", xsize, ysize);

    tot00 = 0.0;
    tot01 = 0.0;
    tot10 = 0.0;
    tot11 = 0.0;

    for(ii=0; ii<xsize/2; ii++)
        for(jj=0; jj<ysize/2; jj++)
            tot00 += data.image[ID].array.F[jj*xsize+ii];

    for(ii=xsize/2; ii<xsize; ii++)
        for(jj=0; jj<ysize/2; jj++)
            tot10 += data.image[ID].array.F[jj*xsize+ii];

    for(ii=0; ii<xsize/2; ii++)
        for(jj=ysize/2; jj<ysize; jj++)
            tot01 += data.image[ID].array.F[jj*xsize+ii];

    for(ii=xsize/2; ii<xsize; ii++)
        for(jj=ysize/2; jj<ysize; jj++)
            tot11 += data.image[ID].array.F[jj*xsize+ii];

    tot = tot00+tot10+tot01+tot11;
    tot00 /= tot;
    tot10 /= tot;
    tot01 /= tot;
    tot11 /= tot;

    printf("  %6.4f   %6.4f\n", tot01, tot11);
    printf("  %6.4f   %6.4f\n", tot00, tot10);
    printf("tot = %f\n", tot);
    xsig = tot01-tot10;
    ysig = tot11-tot00;
    printf(" sig = %6.4f  x %6.4f\n", xsig, ysig);

    /// 100 steps -> sig = 0.055 for modulation = 1.0
    ttxpos = (long) (SCExAO_DM_STAGE_Xpos - gain*100.0*(xsig/0.055));
    ttypos = (long) (SCExAO_DM_STAGE_Ypos - gain*100.0*(ysig/0.055));

    SCExAOcontrol_mv_DMstage(ttxpos, ttypos);

    save_fits("imwfs", "!imwfs.fits");

    return(0);
}





int SCExAOcontrol_PyramidWFS_AutoAlign_TT(const char *WFScam_name, float XposStart, float YposStart)
{
    FILE *fp;
    long ID;
    long xsize, ysize;
    long ii, jj;
    double tot00, tot01, tot10, tot11, tot;
    double tot00x, tot01x, tot10x, tot11x;
    double tot00y, tot01y, tot10y, tot11y;
    double xsig, ysig;
    long ttxpos, ttypos;
    double gain = 1.0;
    char command[200];
    int r;
    double x, y;
    double totx, toty;
    char pausefilename[200];
    float v0;
    long IDshm;
    uint32_t *sizearray;

    long NBframesAve;
    long NBframesAveMin = 500;
    long NBframesAveMax = 30000;
    long twaitus = 500000; // 0.5 sec

    float gainfactor;
	
	char LoopName[200];
	int ret;


	long IDdark;



	fp = fopen("LOOPNAME", "r");
	ret = fscanf(fp, "%s", LoopName);
	fclose(fp);

    //        SCExAOcontrol_PyramidWFS_AutoAlign_TT_DM();
    // exit(0);

    SCExAO_PZT_STAGE_Xpos = XposStart;
    SCExAO_PZT_STAGE_Ypos = YposStart;

    IDshm = image_ID("pyrTT");
    if(IDshm == -1)
    {
        sizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);
        sizearray[0] = 2;
        sizearray[1] = 1;
        IDshm = create_image_ID("pyrTT", 2, sizearray, _DATATYPE_FLOAT, 1, 0);
    }


    NBframesAve = NBframesAveMin;
    gainfactor = 1.0;
    
    
    IDdark = image_ID("wfsdark");
    
    
    while(file_exists("stop_PyAlignTT.txt")==0)
    {
        while (file_exists("pause_PyAlignTT.txt"))
            usleep(100000);

        if(file_exists("./status/gain_PyAlignTT.txt"))
        {
            fp = fopen("./status/gain_PyAlignTT.txt", "r");
            r = fscanf(fp, "%f", &v0);
            fclose(fp);
            if((v0>0.0)&&(v0<1.0))
                gain = gainfactor*v0;
        }
        
        gainfactor = 0.99*gainfactor;
               
        gain *= gainfactor;
        if(gain < 0.1)
            gain = 0.1;
        printf("\n");
        printf("================== AVERAGING %6ld FRAMES    gain = %f ================ \n", NBframesAve, gain);
//        ID = SCExAOcontrol_Average_image(WFScam_name, NBframesAve, "imwfs", 4);
        ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframesAve, "imwfs", 0, 4);
		
        xsize = data.image[ID].md[0].size[0];
        ysize = data.image[ID].md[0].size[1];
        
        NBframesAve = (long) (1.1*NBframesAve);
        if (NBframesAve>NBframesAveMax)
            NBframesAve = NBframesAveMax;
        
        
        for(ii=0; ii<xsize*ysize; ii++)
			data.image[ID].array.F[ii] -= data.image[IDdark].array.F[ii];
        
        

       // printf("%ld x %ld image\n", xsize, ysize);

        tot00 = 0.0;
        tot01 = 0.0;
        tot10 = 0.0;
        tot11 = 0.0;

        tot00x = 0.0;
        tot01x = 0.0;
        tot10x = 0.0;
        tot11x = 0.0;

        tot00y = 0.0;
        tot01y = 0.0;
        tot10y = 0.0;
        tot11y = 0.0;

   


        for(ii=0; ii<xsize/2; ii++)
            for(jj=0; jj<ysize/2; jj++)
            {
                x = 1.0*(0.5+ii)/(xsize/2)-0.5;
                y = 1.0*(0.5+jj)/(ysize/2)-0.5;
                tot00x += x*data.image[ID].array.F[jj*xsize+ii];
                tot00y += y*data.image[ID].array.F[jj*xsize+ii];
                tot00 += data.image[ID].array.F[jj*xsize+ii];
            }

        for(ii=xsize/2; ii<xsize; ii++)
            for(jj=0; jj<ysize/2; jj++)
            {
                x = 1.0*(0.5+ii-xsize/2)/(xsize/2)-0.5;
                y = 1.0*(0.5+jj)/(ysize/2)-0.5;
                tot10x += x*data.image[ID].array.F[jj*xsize+ii];
                tot10y += y*data.image[ID].array.F[jj*xsize+ii];
                tot10 += data.image[ID].array.F[jj*xsize+ii];
            }

        for(ii=0; ii<xsize/2; ii++)
            for(jj=ysize/2; jj<ysize; jj++)
            {
                x = 1.0*(0.5+ii)/(xsize/2)-0.5;
                y = 1.0*(0.5+jj-ysize/2)/(ysize/2)-0.5;
                tot01x += x*data.image[ID].array.F[jj*xsize+ii];
                tot01y += y*data.image[ID].array.F[jj*xsize+ii];
                tot01 += data.image[ID].array.F[jj*xsize+ii];
            }

        for(ii=xsize/2; ii<xsize; ii++)
            for(jj=ysize/2; jj<ysize; jj++)
            {
                x = 1.0*(0.5+ii-xsize/2)/(xsize/2)-0.5;
                y = 1.0*(0.5+jj-ysize/2)/(ysize/2)-0.5;
                tot11x += x*data.image[ID].array.F[jj*xsize+ii];
                tot11y += y*data.image[ID].array.F[jj*xsize+ii];
                tot11 += data.image[ID].array.F[jj*xsize+ii];
            }

        tot = tot00+tot10+tot01+tot11;

        tot00x /= tot00;
        tot10x /= tot10;
        tot01x /= tot01;
        tot11x /= tot11;

        tot00y /= tot00;
        tot10y /= tot10;
        tot01y /= tot01;
        tot11y /= tot11;

        tot00 /= tot;
        tot10 /= tot;
        tot01 /= tot;
        tot11 /= tot;



        printf("  %6.4f   %6.4f\n", tot01, tot11);
        printf("  %6.4f   %6.4f\n", tot00, tot10);
        printf("total = %f   average = %f \n", tot, tot/xsize/ysize);

        totx = 0.25*(tot00x+tot10x+tot10x+tot11x);
        toty = 0.25*(tot00y+tot10y+tot10y+tot11y);

        printf(" PUP X   %+6.4f %+6.4f %+6.4f %+6.4f  -> %+6.4f\n", tot00x, tot01x, tot10x, tot11x, totx);
        printf(" PUP Y   %+6.4f %+6.4f %+6.4f %+6.4f  -> %+6.4f\n", tot00y, tot01y, tot10y, tot11y, toty);

        xsig = (tot10+tot11)-(tot00+tot01); // camera coordinates
        ysig = (tot01+tot11)-(tot00+tot10);
        printf(" SIGNAL = %6.4f  x %6.4f\n", xsig, ysig);

        //exit(0);

        /// 1 V step -> sig = 0.2 for modulation = 0.3
        //       SCExAO_PZT_STAGE_Xpos += gain*(xsig/0.2);
        //     SCExAO_PZT_STAGE_Ypos -= gain*(ysig/0.2);

	


        if(tot > 1.0*xsize*ysize)
        {
            SCExAO_PZT_STAGE_Xpos -= gain*((xsig-ysig)/1.0);  // D actuator
            SCExAO_PZT_STAGE_Ypos -= gain*((xsig+ysig)/1.0);  // C actuator



            printf("  --- %f  %f ----\n", SCExAO_PZT_STAGE_Xpos, SCExAO_PZT_STAGE_Ypos);


            if(SCExAO_PZT_STAGE_Xpos<SCExAO_PZT_STAGE_Xpos_min)
                SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_min;
            if(SCExAO_PZT_STAGE_Xpos>SCExAO_PZT_STAGE_Xpos_max)
                SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_max;

            if(SCExAO_PZT_STAGE_Ypos<SCExAO_PZT_STAGE_Ypos_min)
                SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_min;
            if(SCExAO_PZT_STAGE_Ypos>SCExAO_PZT_STAGE_Ypos_max)
                SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_max;

            // sig X
            sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);
            printf("COMMAND: \"%s\"\n", command);
            r = system(command);

            // sig Y
            sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
            printf("COMMAND: \"%s\"\n", command);
            r = system(command);

			sprintf(command, "./aolconfscripts/aollog \"%s\" \"auto pyTT ave %6ld g %6.4f pupf %6.4f %6.4f %6.4f %6.4f  sig %+6.4f %+6.4f  XY %+5.3f %+5.3f \"", LoopName, NBframesAve, gain, tot01, tot11, tot00, tot10, xsig, ysig, SCExAO_PZT_STAGE_Xpos, SCExAO_PZT_STAGE_Ypos);
            printf("COMMAND: \"%s\"\n", command);
            r = system(command);


            data.image[IDshm].md[0].write = 1;
            data.image[IDshm].array.F[0] = SCExAO_PZT_STAGE_Xpos;
            data.image[IDshm].array.F[1] = SCExAO_PZT_STAGE_Ypos;
            data.image[IDshm].md[0].cnt0 ++;
            data.image[IDshm].md[0].write = 0;
        }
        else
        {
			printf("NOT ENOUGH FLUX - NO CORRECTION\n");
			fflush(stdout);			
		}

        save_fits("imwfs", "!./tmp/imwfs_alignTT.fits");
        usleep(twaitus);
    }

    r = system("rm stop_PyAlignTT.txt");

    return(0);
}











/** assumes imref has been loaded */
int SCExAOcontrol_PyramidWFS_AutoAlign_cam(const char *WFScam_name)
{
    FILE *fp;
    long ID, IDc;
    long ii, jj;
    long brad = 30; // box radius
    double totx, toty, tot;
    double alpha = 20.0;
    double peak, v;
    double gain = 0.2;
    long stepx, stepy;
    int r;
    char command[200];
    long delayus = 500000;
    
    long NBframes = 20000;
    
    float v0;
    long maxstep = 3000;
    float ave;
    char pausefilename[200];


    long NBframesAve;
    long NBframesAveMin = 500;
    long NBframesAveMax = 30000;
    float gainfactor;


	char LoopName[200];
	int ret;


	fp = fopen("LOOPNAME", "r");
	ret = fscanf(fp, "%s", LoopName);
	fclose(fp);



    /// read position of stages
    if((fp = fopen("./status/pcampos.txt", "r"))!=NULL)
    {
        r = fscanf(fp, "%ld %ld\n", &SCExAO_Pcam_Xpos, &SCExAO_Pcam_Ypos);
        fclose(fp);
    }

    SCExAO_Pcam_Xpos0 = SCExAO_Pcam_Xpos;
    SCExAO_Pcam_Ypos0 = SCExAO_Pcam_Ypos;

    NBframesAve = NBframesAveMin;
    gainfactor = 1.0;
    while(file_exists ("stop_PyAlignCam.txt")==0)
    {
        while (file_exists ("pause_PyAlignCam.txt"))
            usleep(100000);



        if(file_exists("./status/gain_PyAlignCam.txt"))
        {
            fp = fopen("./status/gain_PyAlignCam.txt", "r");
            r = fscanf(fp, "%f", &v0);
            fclose(fp);
            if((v0>0.0)&&(v0<1.0))
                gain = gainfactor*v0;
        }
        gainfactor = 0.98*gainfactor;
        if(gainfactor < 0.1)
            gainfactor = 0.1;


        printf("================== AVERAGING %6ld FRAMES    gain = %f ================ \n", NBframesAve, gain);
//        ID = SCExAOcontrol_Average_image(WFScam_name, NBframesAve, "imwfs", 5);
        ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframesAve, "imwfs", 0, 5);
        save_fits("imwfs", "!./tmp/imwfs_aligncam.fits");
  
        NBframesAve = (long) (1.1*NBframesAve);
        if (NBframesAve>NBframesAveMax)
            NBframesAve = NBframesAveMax;
        

        tot = 0.0;
        for(ii=0; ii<pXsize*pYsize; ii++)
            tot += data.image[ID].array.F[ii];
        for(ii=0; ii<pXsize*pYsize; ii++)
            data.image[ID].array.F[ii] /= tot;
        ave =  tot/pXsize/pYsize;
        printf("tot = %f   ave = %f \n", tot, ave);

      

        if(ave > 10.0)
        {
            /** compute offset */
            fft_correlation("imwfs", "imref", "outcorr");
            IDc = image_ID("outcorr");
            
            save_fits("imwfs", "!./tmp/imwfs0.fits");
            save_fits("imref", "!./tmp/imref0.fits");
            save_fits("outcorr", "!./tmp/outcorr0.fits");
            list_image_ID();
            
            
            
            peak = 0.0;
            for(ii=0; ii<pXsize*pYsize; ii++)
                if(data.image[IDc].array.F[ii]>peak)
                    peak = data.image[IDc].array.F[ii];

            for(ii=0; ii<pXsize*pYsize; ii++)
                if(data.image[IDc].array.F[ii]>0.0)
                    data.image[IDc].array.F[ii] = pow(data.image[IDc].array.F[ii]/peak, alpha);
                else
                    data.image[IDc].array.F[ii] = 0.0;

			printf("---------- %ld %ld   %g %f ------------\n", (long) pXsize, (long) pYsize, peak, alpha);
			
            
            totx = 0.0;
            toty = 0.0;
            tot = 0.0;
            for(ii=pXsize/2-brad; ii<pXsize/2+brad; ii++)
                for(jj=pXsize/2-brad; jj<pXsize/2+brad; jj++)
                {
                    v = data.image[IDc].array.F[jj*pXsize+ii];
                    totx += 1.0*(ii-pXsize/2)*v;
                    toty += 1.0*(jj-pXsize/2)*v;
                    tot += v;
                }
            totx /= tot;
            toty /= tot;

            save_fits("outcorr", "!./tmp/outcorr.fits");
            save_fits("imwfs", "!./tmp/imwfs.fits");
            save_fits("imref", "!./tmp/imref.fits");
            delete_image_ID("outcorr");

            printf("  %6.4f  x  %6.4f\n", totx, toty);

            stepx = (long) (-gain*totx*PcamPixScaleAct); // 0.7*10000.0);
            stepy = (long) (gain*toty*PcamPixScaleAct); //  0.7*10000.0);

            if(stepx>maxstep)
                stepx = maxstep;
            if(stepx<-maxstep)
                stepx = -maxstep;
            if(stepy>maxstep)
                stepy = maxstep;
            if(stepy<-maxstep)
                stepy = -maxstep;


            printf("STEP     : %ld %ld\n", stepx, stepy);

            SCExAO_Pcam_Xpos += stepx;
            SCExAO_Pcam_Ypos += stepy;

            if (SCExAO_Pcam_Xpos>SCExAO_Pcam_Xpos0+SCExAO_Pcam_Range)
                SCExAO_Pcam_Xpos = SCExAO_Pcam_Xpos0+SCExAO_Pcam_Range;
            if (SCExAO_Pcam_Ypos>SCExAO_Pcam_Ypos0+SCExAO_Pcam_Range)
                SCExAO_Pcam_Ypos = SCExAO_Pcam_Ypos0+SCExAO_Pcam_Range;

            if (SCExAO_Pcam_Xpos<SCExAO_Pcam_Xpos0-SCExAO_Pcam_Range)
                SCExAO_Pcam_Xpos = SCExAO_Pcam_Xpos0-SCExAO_Pcam_Range;
            if (SCExAO_Pcam_Ypos<SCExAO_Pcam_Ypos0-SCExAO_Pcam_Range)
                SCExAO_Pcam_Ypos = SCExAO_Pcam_Ypos0-SCExAO_Pcam_Range;

            /// write stages position
            fp = fopen("./status/pcampos.txt", "w");
            fprintf(fp, "%ld %ld\n", SCExAO_Pcam_Xpos, SCExAO_Pcam_Ypos);
            fclose(fp);

            sprintf(command, "pywfs_pup x goto %ld\n", SCExAO_Pcam_Xpos);
            printf("%s", command);
            r = system(command);
            usleep(delayus);

            sprintf(command, "pywfs_pup y goto %ld\n", SCExAO_Pcam_Ypos);
            printf("%s", command);
            r = system(command);
            usleep(delayus);


			sprintf(command, "./aolconfscripts/aollog \"%s\" \"auto pcam ave %6ld g %6.4f totxy %+6.4f %+6.4f step %6ld %6ld  XY %7ld %7ld \"", LoopName, NBframesAve, gain, totx, toty, stepx, stepy, SCExAO_Pcam_Xpos, SCExAO_Pcam_Ypos);
            printf("COMMAND: \"%s\"\n", command);
            r = system(command);


        }
        else
        {
            printf("Not enough light on detector... waiting... \n");
        }
    }
    r = system("rm stop_PyAlignCam.txt");

    return(0);
}



/// pupil centering tool
/// watch pcenter stream
int SCExAOcontrol_PyramidWFS_Pcenter(const char *IDwfsname, float prad, float poffset)
{
    long IDmask;
    long IDwfs;
    long size;
    float centobs = 0.3;
    long ii, jj;
    float x, y, r;
    uint32_t *sizearray;
    long ID;
    long size2;
    long cnt;
    float voltAmpOffset = 2.0;
    char command[200];
    long NBframes = 10000;
    long IDpp, IDpm, IDmp, IDmm;
    long IDpyrTTref;
    float xcmm, ycmm, xcpm, ycpm, xcmp, ycmp, xcpp, ycpp;
    float flim;
    float totmm, totpm, totmp, totpp;
    float p10, p90;
    long ii1, jj1;
    float xave, yave;
    FILE *fp;
    long delayus = 1000000;
  

    IDwfs = image_ID(IDwfsname);
    size = data.image[IDwfs].md[0].size[0];
    size2 = size*size;

    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);
    sizearray[0] = size;
    sizearray[1] = size;


    // Read reference pupil illumination
    IDpyrTTref = read_sharedmem_image("pyrTT");
    SCExAO_PZT_STAGE_Xpos_ref = data.image[IDpyrTTref].array.F[0];
    SCExAO_PZT_STAGE_Ypos_ref = data.image[IDpyrTTref].array.F[1];
    printf("X = %f   Y = %f\n", SCExAO_PZT_STAGE_Xpos_ref, SCExAO_PZT_STAGE_Ypos_ref);

    // + +
    SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_ref + voltAmpOffset;
    SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_ref;
    sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);
    printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
    printf("COMMAND: \"%s\"\n", command);
    r = system(command);
//    IDpp = SCExAOcontrol_Average_image(IDwfsname, NBframes, "imwfspp", 4);
    IDpp = IMAGE_BASIC_streamaverage(IDwfsname, NBframes, "imwfspp", 0, 4);
    save_fits("imwfspp", "!imwfspp.fits");


    // + -
    SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_ref + voltAmpOffset;
    SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_ref + 2.0*voltAmpOffset;
    sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);     
    printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
    printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    //IDpm = SCExAOcontrol_Average_image(IDwfsname, NBframes, "imwfspm", 4);
    IDpm = IMAGE_BASIC_streamaverage(IDwfsname, NBframes, "imwfspm", 0, 4);
    save_fits("imwfspm", "!imwfspm.fits");

    // - +
    SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_ref - voltAmpOffset;
    SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_ref - 2.0*voltAmpOffset;
    sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);
	printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
    printf("COMMAND: \"%s\"\n", command);
    r = system(command);
//    IDmp = SCExAOcontrol_Average_image(IDwfsname, NBframes, "imwfsmp", 4);
    IDmp = IMAGE_BASIC_streamaverage(IDwfsname, NBframes, "imwfsmp", 0, 4);
    save_fits("imwfsmp", "!imwfsmp.fits");


    // - -
    SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_ref - voltAmpOffset;
    SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_ref;
    sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);
	printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
	printf("COMMAND: \"%s\"\n", command); 
    r = system(command);
//    IDmm = SCExAOcontrol_Average_image(IDwfsname, NBframes, "imwfsmm", 4);
    IDmm = IMAGE_BASIC_streamaverage(IDwfsname, NBframes, "imwfsmm", 0, 4);
    save_fits("imwfsmm", "!imwfsmm.fits");


    // going back to reference

    SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_ref;
    SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_ref;
    sprintf(command, "./aocscripts/SCExAO_analogoutput D %5.3f", SCExAO_PZT_STAGE_Xpos);
	printf("COMMAND: \"%s\"\n", command);
    r = system(command);
    sprintf(command, "./aocscripts/SCExAO_analogoutput C %5.3f", SCExAO_PZT_STAGE_Ypos);
	printf("COMMAND: \"%s\"\n", command);
    r = system(command);


    // sum the 4 images
    arith_image_add("imwfspp", "imwfspm", "prefsum");
    arith_image_add_inplace("prefsum", "imwfsmp");
    arith_image_add_inplace("prefsum", "imwfsmm");

    delete_image_ID("imwfspp");
    delete_image_ID("imwfspm");
    delete_image_ID("imwfsmp");
    delete_image_ID("imwfsmm");

    save_fits("prefsum", "!prefsum.fits");
    p10 = img_percentile("prefsum", 0.10);
    p90 = img_percentile("prefsum", 0.90);

    xcpp = 0.0;
    ycpp = 0.0;
    totpp = 0.0;

    xcmp = 0.0;
    ycmp = 0.0;
    totmp = 0.0;

    xcpm = 0.0;
    ycpm = 0.0;
    totpm = 0.0;

    xcmm = 0.0;
    ycmm = 0.0;
    totmm = 0.0;

    flim = p10 + 0.3*(p90-p10);
    ID = image_ID("prefsum");
    for(ii=0; ii<size/2; ii++)
        for(jj=0; jj<size/2; jj++)
        {
            if(data.image[ID].array.F[jj*size+ii]>flim)
            {
                totmm += 1.0;
                xcmm += ii;
                ycmm += jj;
            }

            ii1 = ii+size/2;
            jj1 = jj;
            if(data.image[ID].array.F[jj1*size+ii1]>flim)
            {
                totpm += 1.0;
                xcpm += ii;
                ycpm += jj;
            }

            ii1 = ii;
            jj1 = jj+size/2;
            if(data.image[ID].array.F[jj1*size+ii1]>flim)
            {
                totmp += 1.0;
                xcmp += ii;
                ycmp += jj;
            }

            ii1 = ii+size/2;
            jj1 = jj+size/2;
            if(data.image[ID].array.F[jj1*size+ii1]>flim)
            {
                totpp += 1.0;
                xcpp += ii;
                ycpp += jj;
            }

        }

    xcpp /= totpp;
    ycpp /= totpp;

    xcpm /= totpm;
    ycpm /= totpm;

    xcmp /= totmp;
    ycmp /= totmp;

    xcmm /= totmm;
    ycmm /= totmm;



    printf("++ : %f %f\n", xcpp, ycpp);
    printf("+- : %f %f\n", xcpm, ycpm);
    printf("-+ : %f %f\n", xcmp, ycmp);
    printf("-- : %f %f\n", xcmm, ycmm);

    xave = 0.25*(xcpp+xcpm+xcmp+xcmm);
    yave = 0.25*(ycpp+ycpm+ycmp+ycmm);

    xave = xave - 0.25*size + 0.5;
    yave = yave - 0.25*size + 0.5;
    printf("AVERAGE PIXEL OFFSET = %f %f\n", xave, yave);

    delete_image_ID("prefsum");


    /// read position of stages
    if((fp = fopen("./status/pcampos.txt", "r"))!=NULL)
    {
        r = fscanf(fp, "%ld %ld\n", &SCExAO_Pcam_Xpos, &SCExAO_Pcam_Ypos);
        printf("CURRENT POSITION : %ld %ld\n", SCExAO_Pcam_Xpos, SCExAO_Pcam_Ypos);
        fclose(fp);
    }

    SCExAO_Pcam_Xpos += (long) (xave*PcamPixScaleAct);
    SCExAO_Pcam_Ypos -= (long) (yave*PcamPixScaleAct);
    printf("NEW POSITION : %ld %ld\n", SCExAO_Pcam_Xpos, SCExAO_Pcam_Ypos);

    xcpp -= xave;
    xcpm -= xave;
    xcmp -= xave;
    xcmm -= xave;

    ycpp -= yave;
    ycpm -= yave;
    ycmp -= yave;
    ycmm -= yave;

    /// write stages position
    fp = fopen("./status/pcampos.txt", "w");
    fprintf(fp, "%ld %ld\n", SCExAO_Pcam_Xpos, SCExAO_Pcam_Ypos);
    fclose(fp);

    sprintf(command, "pywfs reimage x goto %ld\n", SCExAO_Pcam_Xpos);
    printf("%s", command);
    r = system(command);
    usleep(delayus);

    sprintf(command, "pywfs reimage y goto %ld\n", SCExAO_Pcam_Ypos);
    printf("%s", command);
    r = system(command);
    usleep(delayus);


    ID = create_image_ID("pcenter", 2, sizearray, _DATATYPE_FLOAT, 1, 0);


    IDmask = create_2Dimage_ID("pmask", size, size);

    for(ii=0; ii<size/2; ii++)
        for(jj=0; jj<size/2; jj++)
        {
            // --
            ii1 = ii;
            jj1 = jj;
            x = xcmm-ii;
            y = ycmm-jj;
            r = sqrt(x*x+y*y);
            r /= prad;
            if((r>centobs)&&(r<1.0))
                data.image[IDmask].array.F[jj1*size+ii1] = 1.0;

            // +-
            ii1 = ii+size/2;
            jj1 = jj;
            x = xcpm-ii;
            y = ycpm-jj;
            r = sqrt(x*x+y*y);
            r /= prad;
            if((r>centobs)&&(r<1.0))
                data.image[IDmask].array.F[jj1*size+ii1] = 1.0;

            // -+
            ii1 = ii;
            jj1 = jj+size/2;
            x = xcmp-ii;
            y = ycmp-jj;
            r = sqrt(x*x+y*y);
            r /= prad;
            if((r>centobs)&&(r<1.0))
                data.image[IDmask].array.F[jj1*size+ii1] = 1.0;

            // ++
            ii1 = ii+size/2;
            jj1 = jj+size/2;
            x = xcpp-ii;
            y = ycpp-jj;
            r = sqrt(x*x+y*y);
            r /= prad;
            if((r>centobs)&&(r<1.0))
                data.image[IDmask].array.F[jj1*size+ii1] = 1.0;
        }



    printf("Applying mask to image ...\n");
    fflush(stdout);
    cnt = data.image[IDwfs].md[0].cnt0;
    while (1)
    {
        usleep(10);
        if(cnt != data.image[IDwfs].md[0].cnt0)
        {
            cnt = data.image[IDwfs].md[0].cnt0;

            data.image[ID].md[0].write = 1;
            for(ii=0; ii<size2; ii++)
                data.image[ID].array.F[ii] = 1.0*data.image[IDwfs].array.UI16[ii]*data.image[IDmask].array.F[ii];
            data.image[ID].md[0].cnt0++;
            data.image[ID].md[0].write = 1;
        }
    }

    free(sizearray);

    return(0);
}





int SCExAOcontrol_Pyramid_flattenRefWF(const char *WFScam_name, long zimaxmax, float ampl0)
{
    long zimax;
    long zi;
    long ID;
    long NBframes = 100;
    double val, valp, valm, val0;
    double ampl;
    double a;
    char command[200];
    int r;
    long IDdm5, IDdm6;
    long ii;
    long dmsize;
    long dmsize2;
    long IDz;
    long IDdisp;
    long sleeptimeus = 1000; // 1ms


    // 60perc of pixels illuminated
    // perc 70 is median over pupil

    double p0, p1, p2;
    float level0, level1, level2;

    level0 = 0.55;
    level1 = 0.70;
    level2 = 0.95;
    

    IDdm5 = read_sharedmem_image("dmdisp5");
    IDdm6 = read_sharedmem_image("dmdisp6");
    IDdisp = read_sharedmem_image("dmdisp");
    dmsize = data.image[IDdm5].md[0].size[0];
    dmsize2 = dmsize*dmsize;

    // prepare modes
    IDz = mk_zer_seriescube("zcube", dmsize, zimaxmax, 0.5*dmsize);
    list_image_ID();
    printf("IDz = %ld\n", IDz);

    zimax = zimaxmax;


    
//            ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "imwfs", 6);
            ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "imwfs", 0, 6);
            save_fits("imwfs", "!./tmp/imwfs_pyrflat.fits");
            p0 = img_percentile("imwfs", level0);
            p1 = img_percentile("imwfs", level1);
            p2 = img_percentile("imwfs", level2);
            val = (p2-p0)/p1; //+p90);
            printf("%lf %lf %lf -> %f\n", p0, p1, p2, val);
            val0 = val;

    
    ampl = ampl0;

    while(1)
    {
        ampl *= 0.95;
            if(ampl<0.1*ampl0)
                ampl = 0.1*ampl0;

        
        for(zi=4; zi<zimax; zi++)
        {
//            ampl = ampl0; //*pow((1.0 - 0.9*(zimax/zimaxmax)), 2.0);

            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] += ampl*data.image[IDz].array.F[zi*dmsize2+ii];
            sem_post(data.image[IDdm5].semptr[0]);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;
           
            usleep(sleeptimeus);


//            ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "imwfs", 6);
            ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "imwfs", 0, 6);
            save_fits("imwfs", "!./tmp/imwfs_pyrflat.fits");
            p0 = img_percentile("imwfs", level0);
            p1 = img_percentile("imwfs", level1);
            p2 = img_percentile("imwfs", level2);
            val = (p2-p0)/p1; //+p90);
            printf("%lf %lf %lf -> %f\n", p0, p1, p2, val);
            valp = val;


            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] -= 2.0*ampl*data.image[IDz].array.F[zi*dmsize2+ii];
            sem_post(data.image[IDdm5].semptr[0]);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;
           
            usleep(sleeptimeus);


//            ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "imwfs", 6);
            ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "imwfs", 0, 6);
            save_fits("imwfs", "!./tmp/imwfs_pyrflat.fits");
            p0 = img_percentile("imwfs", level0);
            p1 = img_percentile("imwfs", level1);
            p2 = img_percentile("imwfs", level2);
            val = (p2-p0)/p1; //+p90);
            printf("%lf %lf %lf -> %f\n", p0, p1, p2, val);
            valm = val;

            /*	if(valm>valp)
            		a = -amp;
            	else
            		a = amp;
            */

            a = (1.0/valp-1.0/valm)/(1.0/valp+1.0/valm)*ampl;
            printf("== ZERNIKE %ld / %ld ========== %f %f -> a = %f  [ampl = %f] ( %f <- %f)\n", zi, zimax, valp, valm, a, ampl, 0.5*(valp+valm), val0);


            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] += (ampl+a)*data.image[IDz].array.F[zi*dmsize2+ii];
            sem_post(data.image[IDdm5].semptr[0]);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;

            usleep(sleeptimeus);

            //			sleep(1);
        }

        printf("%ld -> %ld\n", IDdm5, IDdm6);
        data.image[IDdm5].md[0].write = 1;
        data.image[IDdm6].md[0].write = 1;
        for(ii=0; ii<2500; ii++)
        {
            data.image[IDdm6].array.F[ii] += data.image[IDdm5].array.F[ii];
            data.image[IDdm5].array.F[ii] = 0.0;
        }
        data.image[IDdm5].md[0].cnt0++;
        data.image[IDdm6].md[0].cnt0++;
        data.image[IDdm5].md[0].write = 0;
        data.image[IDdm6].md[0].write = 0;

        zimax ++;
        if(zimax>zimaxmax)
            zimax = zimaxmax;
    }

    return(0);
}







int SCExAOcontrol_optPSF(const char *WFScam_name, long NBmodesmax, float alpha)
{
	FILE *fp;
    long NBmodes;
    long mode;
    long modestart = 1;
    long ID;
    long NBframes = 20;
    double val, valp, valm, val0;
    double ampl;
    double a;
    char command[200];
    int r;
    long IDdm5, IDdm6;
    long ii, jj;
    long dmsize;
    long dmsize2;
    long IDm;
    long IDdisp;
    long sleeptimeus = 100000; // 100ms

	long iter = 0;
    double p0, p1, p2;
    float level0, level1, level2;
	double v0;
	double tot, tot1;
	double pv;
	long xsize, ysize;
	long cnt;
	long iter1 = 0;
	
	double ampl0 = 0.005;


	double beta = 0.99999; // regularization

	double ampcoeff = 10.0;


    level0 = 0.1;
    level1 = 0.2;
    level2 = 0.4;
    

    IDdm5 = read_sharedmem_image("dm00disp05");
    IDdm6 = read_sharedmem_image("dm00disp06");
    IDdisp = read_sharedmem_image("dm00disp");
    dmsize = data.image[IDdm5].md[0].size[0];
    dmsize2 = dmsize*dmsize;

    // prepare modes
    
    IDm = image_ID("modes");
    
    if(IDm==-1)
	{    
		IDm = mk_zer_seriescube("modes", dmsize, NBmodesmax, 0.5*dmsize);
		list_image_ID();
		printf("IDm = %ld\n", IDm);
		NBmodes = NBmodesmax;
	}
	else
		NBmodes = data.image[IDm].md[0].size[2];

	if(NBmodes>NBmodesmax)
		NBmodes = NBmodesmax;



	printf("Averaging %ld frames ,,,", NBframes);
	fflush(stdout);
//    ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "impsf", 6);
    ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "impsf", 0, 6);
    printf(" done\n");
    fflush(stdout);
    
            save_fits("impsf", "!./tmp/impsf.fits");
            p0 = img_percentile("impsf", level0);
            p1 = img_percentile("impsf", level1);
            p2 = img_percentile("impsf", level2);
 
	xsize = data.image[ID].md[0].size[0];
	ysize = data.image[ID].md[0].size[1];

    tot1 = 0.0;
    tot = 0.0;
    cnt = 0;
	v0 = 1.0*p2;
    for(ii=0;ii<xsize;ii++)
		for(jj=0;jj<ysize;jj++)
			{
				pv = data.image[ID].array.F[jj*xsize+ii];
				pv -= v0;
				if(pv>0.0)
					{
						tot1 += pow(pv, alpha);
						tot += pv;
						cnt++;
					}
			}
    val = tot1/pow(tot, alpha);
    val0 = val;
    
    
    
    
    ampl = ampl0;

    while(1)
    {
        ampl *= 0.95;
            if(ampl<0.1*ampl0)
                ampl = 0.1*ampl0;

        
        for(mode=modestart; mode<NBmodes; mode++)
        {
            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] += ampl*data.image[IDm].array.F[mode*dmsize2+ii];
            COREMOD_MEMORY_image_set_sempost_byID(IDdm5, -1);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;
           
            usleep(sleeptimeus);


//            ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "impsf", 6);
            ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "impsf", 0, 6);
            
            save_fits("impsf", "!./tmp/impsf.fits");
            p0 = img_percentile("impsf", level0);
            p1 = img_percentile("impsf", level1);
            p2 = img_percentile("impsf", level2);
            tot1 = 0.0;
			tot = 0.0;
			cnt = 0;
			v0 = 1.0*p2;
			for(ii=0;ii<xsize;ii++)
				for(jj=0;jj<ysize;jj++)
				{
				pv = data.image[ID].array.F[jj*xsize+ii];
				pv -= v0;
				if(pv>0.0)
					{
						tot1 += pow(pv, alpha);
						tot += pv;
						cnt++;
					}
				}
			val = tot1/pow(tot, alpha);
               
            valp = val;


            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] -= 2.0*ampl*data.image[IDm].array.F[mode*dmsize2+ii];            
            COREMOD_MEMORY_image_set_sempost_byID(IDdm5, -1);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;
           
            usleep(sleeptimeus);


//            ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "impsf", 6);
            ID = IMAGE_BASIC_streamaverage(WFScam_name, NBframes, "impsf", 0, 6);
            
            save_fits("impsf", "!./tmp/impsf.fits");
            p0 = img_percentile("impsf", level0);
            p1 = img_percentile("impsf", level1);
            p2 = img_percentile("impsf", level2);
            tot1 = 0.0;
			tot = 0.0;
			cnt = 0;
			v0 = 1.0*p2;
			for(ii=0;ii<xsize;ii++)
				for(jj=0;jj<ysize;jj++)
				{
				pv = data.image[ID].array.F[jj*xsize+ii];
				pv -= v0;
				if(pv>0.0)
					{
						tot1 += pow(pv, alpha);
						tot += pv;
						cnt++;
					}
				}
			val = tot1/pow(tot, alpha);
			if(cnt<1)
				{
					val = 0.0;
					printf("ERROR: image is zero \n");
					exit(0);
				}
			printf("=========== %f  %f  %ld  %f\n", tot, tot1, cnt, val);
			
            valm = val;


           
             a = ampcoeff * (valp - valm) / (valp + valm)*ampl;
            if(a<-ampl)
				a = -ampl;
			if(a>ampl)
				a = ampl;
            
            printf("==  MODE %ld / %ld ========== (%ld) %f %f -> a = %f  [ampl = %f] ( %f <- %f)\n", mode, NBmodes, cnt, valp, valm, a, ampl, 0.5*(valp+valm), val0);

			fp = fopen("log.txt", "a");
			fprintf(fp, "%8ld  %8ld  %4ld  %20f  %20f\n", iter1, iter, mode, 0.5*(valp+valm), val0);
			fclose(fp);
			
            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] += (ampl+a)*data.image[IDm].array.F[mode*dmsize2+ii];
            COREMOD_MEMORY_image_set_sempost_byID(IDdm5, -1);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;


            data.image[IDdm5].md[0].write = 1;
            for(ii=0; ii<dmsize2; ii++)
                data.image[IDdm5].array.F[ii] *= beta;
            COREMOD_MEMORY_image_set_sempost_byID(IDdm5, -1);
            sem_post(data.image[IDdisp].semptr[1]);
            data.image[IDdm5].md[0].cnt0++;
            data.image[IDdm5].md[0].write = 0;


            usleep(sleeptimeus);

            iter1++;
        }
        ampcoeff *= 0.9;
        if(ampcoeff<1.0)
			ampcoeff = 1.0;



        printf("%ld -> %ld\n", IDdm5, IDdm6);
		fflush(stdout);

		
		
        data.image[IDdm5].md[0].write = 1;
        data.image[IDdm6].md[0].write = 1;
        for(ii=0; ii<dmsize2; ii++)
        {
            data.image[IDdm6].array.F[ii] += data.image[IDdm5].array.F[ii];
            data.image[IDdm5].array.F[ii] = 0.0;
        }
        COREMOD_MEMORY_image_set_sempost_byID(IDdm5, -1);
        COREMOD_MEMORY_image_set_sempost_byID(IDdm6, -1);
        data.image[IDdm5].md[0].cnt0++;
        data.image[IDdm6].md[0].cnt0++;
        data.image[IDdm5].md[0].write = 0;
        data.image[IDdm6].md[0].write = 0;

		

        NBmodes ++;
        if(NBmodes>NBmodesmax)
            NBmodes = NBmodesmax;
        if(NBmodes>data.image[IDm].md[0].size[2])
			NBmodes = data.image[IDm].md[0].size[2];
    
		
    
    
		iter++;
    }

    return(0);
}





/** SAPHIRA image: process data cube into single frame
 *
 * full linear regression, up to saturation level
 *
 * */
int SCExAOcontrol_SAPHIRA_cam_process(const char *IDinname, const char *IDoutname)
{
    long IDout;
    long IDin;
    long xsize, ysize, zsize;
    uint32_t *sizeoutarray;
    long k;
    long ii, jj;
    float v0;
    long xysize;
    double v1, vk, vt, vv;
    long kk;
    long cnt0, cnt1, cnt2;
    int SATURATION = 65534;
    long iter;
    double eps = 1e-8;
    long k1;
    long IDintmp, IDsatmask, ID2dtmp;
    unsigned short int pvu, vcnt, vaveku;
    long vavevu;
    float vavev, vavek;
    long k1start;

    IDin = image_ID(IDinname);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];
    xysize = xsize*ysize;


    if(zsize>2)
        k1start = 1;
    else
        k1start = 0;

    sizeoutarray = (uint32_t*) malloc(sizeof(uint32_t)*3);
    sizeoutarray[0] = xsize;
    sizeoutarray[1] = ysize;
    sizeoutarray[2] = zsize;



    IDintmp = create_image_ID("intmp", 3, sizeoutarray, _DATATYPE_UINT16, 1, 0); // temporary buffer
    IDsatmask = create_image_ID("satmask", 3, sizeoutarray, _DATATYPE_UINT16, 1, 0); // saturation mask
    ID2dtmp = create_image_ID("saphira2dtmp", 2, sizeoutarray, _DATATYPE_FLOAT, 1, 0); // intermediate resutl
    IDout = create_image_ID(IDoutname, 2, sizeoutarray, _DATATYPE_FLOAT, 1, 0);
    COREMOD_MEMORY_image_set_createsem(IDoutname, 4);

    if(data.image[IDin].md[0].sem == 0)
    {
        printf("Error: no semaphore detected\n");
        exit(0);
    }


    // drive semaphore to zero
    while(sem_trywait(data.image[IDin].semptr[0])==0) {}



    iter = 0;


    while(1)
    {
        sem_wait(data.image[IDin].semptr[0]);
        while(sem_trywait(data.image[IDin].semptr[0])==0) {}

        k = data.image[IDin].md[0].cnt1;
        printf("%ld   slice %ld written [%ld] \n      ", iter, k, IDin);
        fflush(stdout);

        if(k == zsize-1)  // process cube
        {
            memcpy(data.image[IDintmp].array.UI16, data.image[IDin].array.UI16, sizeof(short)*xysize*zsize);
            for(ii=0; ii<xysize; ii++)
            {
                k1 = 0;
                v0 = 0.0;
                v1 = 0.0;
                vcnt = 0;
                vaveku = 0;
                vavevu = 0;
                for(k1=k1start; k1<zsize; k1++)
                {
                    pvu = data.image[IDintmp].array.UI16[k1*xysize+ii];
                    //	printf("[%d %u] ", pvu, pvu);
                    if(pvu<SATURATION)
                    {
                        data.image[IDsatmask].array.UI16[k1*xysize+ii] = 1;
                        vavevu += pvu;
                        vaveku += k1;
                        vcnt++;
                    }
                    else
                    {
                        data.image[IDsatmask].array.UI16[k1*xysize+ii] = 0;
                    }
                }
                vavev = 1.0*vavevu/vcnt;
                vavek = 1.0*vaveku/vcnt;

                for(k1=k1start; k1<zsize; k1++)
                {
                    pvu = data.image[IDintmp].array.UI16[k1*xysize+ii];
                    if(data.image[IDsatmask].array.UI16[k1*xysize+ii] == 1)
                    {
                        vk = 1.0*k1 - vavek;
                        vv = 1.0*pvu - vavev;
                        v0 += vk*vv;
                        v1 += vk*vk;
                    }
                }
                data.image[ID2dtmp].array.F[ii] = v0/(v1+eps);

            }

            iter++;
            printf("\n CUBE COMPLETED -> 2D image ready\n");
            data.image[IDout].md[0].write = 1;
            memcpy(data.image[IDout].array.F, data.image[ID2dtmp].array.F, sizeof(float)*xysize);
            if(data.image[IDout].md[0].sem > 0)
                sem_post(data.image[IDout].semptr[0]);
            data.image[IDout].md[0].cnt0 ++;
            data.image[IDout].md[0].write = 0;
        }
    }

    free(sizeoutarray);

    return(IDout);
}




long SCExAOcontrol_vib_ComputeCentroid(const char *IDin_name, const char *IDdark_name, const char *IDout_name)
{
    long IDout;
    long IDin, IDdark;
    long semtrig = 3;
    uint32_t *sizearray;
    long ii, jj, xsize, ysize, xysize;
    double val, vald, valx, valy, tot;
    int atype;
//    double valxdark, valydark, totdark;
	long iistart, iiend, jjstart, jjend;
	long boxrad = 50;


    IDin = image_ID(IDin_name);
    IDdark = image_ID(IDdark_name);

    atype = data.image[IDin].md[0].atype;
    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    xysize = xsize*ysize;

    // create output
    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);
    sizearray[0] = 2;
    sizearray[1] = 1;
    IDout = create_image_ID(IDout_name, 2, sizearray, _DATATYPE_FLOAT, 1, 0);
    COREMOD_MEMORY_image_set_createsem(IDout_name, 10);
    free(sizearray);

    // compute dark centroid
/*    tot = 0.0;
    valx = 0.0;
    valy = 0.0;
    for(ii=0; ii<xsize; ii++)
        for(jj=0; jj<ysize; jj++)
        {
            val = data.image[IDdark].array.F[jj*xsize+ii];
            valx += 1.0*ii*val;
            valy += 1.0*jj*val;
            tot += 1.0*val;
        }
    if(tot>1.0)
    {
		valxdark = valx/tot;
		valydark = valy/tot;
		totdark = tot;
	}
*/


    // drive semaphore to zero
    while(sem_trywait(data.image[IDin].semptr[semtrig])==0) {}

	iistart = 0;
	iiend = xsize;
	jjstart = 0;
	jjend = ysize;

    while(1)
    {
        sem_wait(data.image[IDin].semptr[semtrig]);
        
       // printf("New image\n");
       // fflush(stdout);

        tot = 0.0;
        valx = 0.0;
        valy = 0.0;

        switch (atype) {
        case _DATATYPE_UINT16 :
            for(ii=iistart; ii<iiend; ii++)
                for(jj=jjstart; jj<jjend; jj++)
                {
                    val = 1.0*data.image[IDin].array.UI16[jj*xsize+ii];
					vald = data.image[IDdark].array.F[jj*xsize+ii];
                    val -= vald;
                    valx += 1.0*ii*val;
                    valy += 1.0*jj*val;
                    tot += 1.0*val;
                }
            break;
        case _DATATYPE_FLOAT :
            for(ii=iistart; ii<iiend; ii++)
                for(jj=jjstart; jj<jjend; jj++)
                {
                    val = data.image[IDin].array.F[jj*xsize+ii];
                    vald = data.image[IDdark].array.F[jj*xsize+ii];
					val -= vald;
                    valx += 1.0*ii*val;
                    valy += 1.0*jj*val;
                    tot += 1.0*val;
                }
            break;
        }
      //  printf("%12f %12f %12f\n", valx, valy, tot);
        
		valx = valx/tot;
		valy = valy/tot;

		if(valx<0.0)
			valx = 0.0;
		if(valx>1.0*(xsize-1))
			valx = 1.0*(xsize-1);
			
		if(valy<0.0)
			valy = 0.0;
		if(valy>1.0*(ysize-1))
			valy = 1.0*(ysize-1);
			

        data.image[IDout].md[0].write = 1;
		data.image[IDout].array.F[0] = valx;
		data.image[IDout].array.F[1] = valy;
        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
        data.image[IDout].md[0].cnt0 ++;
        data.image[IDout].md[0].write = 0;
        
        
        iistart = (long) (valx-boxrad);
        iiend = (long) (valx+boxrad);
        jjstart = (long) (valy-boxrad);
        jjend = (long) (valy+boxrad);
        
        if(iistart<0)
			iistart = 0;
		if(iiend>xsize-1)
			iiend = xsize-1;

		if(jjstart<0)
			jjstart = 0;
		if(jjend>ysize-1)
			jjend = ysize-1;
        

		printf("tot = %20f    boxrad = %4ld  %f %f\n", tot, boxrad, valx, valy);
		if(tot<20000)
			boxrad = 200;
		else
			boxrad = 50;
    }

    return IDout;
}



// mode = 0: continuous acquisition
// mode = 1: 14400 points acquisition (120 sec) -> FITS file
// mode = 2: 1200 points acquisition, TT X calib
// mode = 3: 1200 points acquisition, TT Y calib

long SCExAOcontrol_vib_mergeData(const char *IDacc_name, const char *IDttpos_name, const char *IDout_name, int mode)
{
	long IDout;
	long IDacc;
	long IDttpos;
	long semtrig = 2;
	long NBacc;
	float gain = 0.005; // drive measurement back to zero... slow loop	
	float *valarray;
	float *valarrayave;
	long kk;
	FILE *fpout;
	int WriteFile = 1;
	
	uint32_t *sizearray;
	long iter = 0;	
	
	int initOK = 0;
	
	float TTamp = 0.5;
	long NBpt = 1200;
	long iter0;
	long NBpt0 = 1200; // warm up
	long ii;

	float TTx, TTy;

	char imname[200];
	char fname[200];
	long IDoutC;
	
	float accFactor = 1000.0;
	
	long ID_TTact;
	int outTT = 0;
	
	
	
	
	
	// CALIB NOTES
	//
	// imposX [pix]  =  -36.5  * TTx [V] 
	// imposY [pix]  =   35.0  * TTy [V] 
	//  
	// lag X = 1.7 frame 
	// lag Y = 1.9 frame
	//
	//
	// gain = 35.75 pix / V
	// delay = 1.8 frame
	//
	
	
	
	
	printf("MODE = %d\n", mode);
	
	ID_TTact = -1;
	if(mode>1)
	{
		// connect to actuators
		ID_TTact = image_ID("TToffload_modeval");
		if(ID_TTact != -1)
			outTT = 1;
	}
	
	printf("ID_TTact = %ld\n", ID_TTact);
	fflush(stdout);
	
	
	
	
	IDacc = image_ID(IDacc_name);
	NBacc = data.image[IDacc].md[0].size[0];
	
	IDttpos = image_ID(IDttpos_name);
	
	
	valarray = (float*) malloc(sizeof(float)*(NBacc+2));
	valarrayave = (float*) malloc(sizeof(float)*(NBacc+2));


   // create output
    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);
    sizearray[0] = NBacc+2;
    sizearray[1] = 1;
    IDout = create_image_ID(IDout_name, 2, sizearray, _DATATYPE_FLOAT, 1, 0);
    COREMOD_MEMORY_image_set_createsem(IDout_name, 10);
    free(sizearray);

	if(mode>1)
		WriteFile = 1;
		
	if(mode==1)
		{
			NBpt = 14400;
			sprintf(imname, "%sC", IDout_name);
			IDoutC = create_3Dimage_ID(imname, NBacc+2, 1, NBpt);
		}
		
	if(WriteFile == 1)
		fpout = fopen("accpos.dat", "w");
	
	 // drive semaphore to zero
    while(sem_trywait(data.image[IDacc].semptr[semtrig])==0) {}
	
	iter = 0;
	iter0 = 0;
	TTx = 0.0;
	TTy = 0.0;
	while(iter<NBpt)
	{
		sem_wait(data.image[IDacc].semptr[semtrig]);
		
		if(iter==0)
			{
				for(kk=0;kk<NBacc;kk++)
					valarrayave[kk] = data.image[IDacc].array.F[kk]*accFactor;
				valarrayave[NBacc] = data.image[IDttpos].array.F[0];
				valarrayave[NBacc+1] = data.image[IDttpos].array.F[1];
			}
		
		for(kk=0;kk<NBacc;kk++)
			valarray[kk] = data.image[IDacc].array.F[kk]*accFactor - valarrayave[kk]; 
		valarray[NBacc] = data.image[IDttpos].array.F[0] - valarrayave[NBacc];
		valarray[NBacc+1] = data.image[IDttpos].array.F[1] - valarrayave[NBacc+1];
		
		data.image[IDout].md[0].write = 1;
		for(kk=0;kk<NBacc+2;kk++)
			data.image[IDout].array.F[kk] = valarray[kk];
        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
        data.image[IDout].md[0].cnt0 ++;
        data.image[IDout].md[0].write = 0;
        
        if(mode==2)
			{
				if(iter==NBpt/4)
					TTx = TTamp;
				if(iter==NBpt/2)
					TTx = -TTamp;
				if(iter==3*NBpt/4)
					TTx = 0.0;
			}
	
        if(mode==3)
			{
				if(iter==NBpt/4)
					TTy = TTamp;
				if(iter==NBpt/2)
					TTy = -TTamp;
				if(iter==3*NBpt/4)
					TTy = 0.0;
			}
	
		if(outTT==1)
		{
	        data.image[ID_TTact].md[0].write = 1;
			data.image[ID_TTact].array.F[0] = TTx;
			data.image[ID_TTact].array.F[1] = TTy;
			COREMOD_MEMORY_image_set_sempost_byID(ID_TTact, -1);
			data.image[ID_TTact].md[0].cnt0 ++;
			data.image[ID_TTact].md[0].write = 0;
		}
		
	
		if((WriteFile == 1)&&(iter0>NBpt0))
			{
				fpout = fopen("accpos.dat", "a");
				fprintf(fpout, "%8ld  %+10.8f  %+10.8f  %+10.8f  %+10.8f  %+10.8f  %+10.8f  %+10.8f  %+10.8f\n", iter, TTx, TTy, data.image[IDout].array.F[0], data.image[IDout].array.F[1], data.image[IDout].array.F[2], data.image[IDout].array.F[3], data.image[IDout].array.F[4], data.image[IDout].array.F[5]);
				fclose(fpout);
			}
		
		if(mode==1)
			{
				for(kk=0;kk<NBacc+2;kk++)
					data.image[IDoutC].array.F[iter*(NBacc+2)+kk] = data.image[IDout].array.F[kk];
			}
		
		if(initOK==0)
			{
				for(kk=0;kk<NBacc;kk++)
					valarrayave[kk] = data.image[IDacc].array.F[kk]*accFactor;
				valarrayave[NBacc] = data.image[IDttpos].array.F[0];
				valarrayave[NBacc+1] = data.image[IDttpos].array.F[1];
				initOK = 1;
			}
		
		
		for(kk=0;kk<NBacc;kk++)
			valarrayave[kk] = (1.0-gain)*valarrayave[kk] + gain*data.image[IDacc].array.F[kk]*accFactor;
		valarrayave[NBacc] = (1.0-gain)*valarrayave[NBacc] + gain*data.image[IDttpos].array.F[0];
		valarrayave[NBacc+1] = (1.0-gain)*valarrayave[NBacc+1] + gain*data.image[IDttpos].array.F[1];
	
	
		iter0++;
		
		
		if((mode>0)&&(iter0>NBpt0))
			{
				gain = 0.0;
				iter ++;
			}
	}
	TTx = 0.0;
	TTy = 0.0;
	
	free(valarray);
	free(valarrayave);
	
	
	if(mode==1)
	{
		sprintf(fname, "!%s.fits", imname);
		save_fits(imname, fname);
	}
	
	return IDout;
}


