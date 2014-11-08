#include <fitsio.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <ncurses.h>
#include <semaphore.h>

#include "CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "linopt_imtools/linopt_imtools.h"
#include "image_filter/image_filter.h"

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



/// CONFIGURATION
char WFScam_name[200];
long long WFScnt = 0;

long pXsize = 120;
long pYsize = 120;

long SCExAO_DM_STAGE_Xpos = 0;
long SCExAO_DM_STAGE_Ypos = 0;
long SCExAO_Pcam_Xpos = 170000;
long SCExAO_Pcam_Ypos = 66000;

float SCExAO_PZT_STAGE_Xpos = -5.0;
float SCExAO_PZT_STAGE_Xpos_min = -6.5;
float SCExAO_PZT_STAGE_Xpos_max = -3.5;

float SCExAO_PZT_STAGE_Ypos = -5.0;
float SCExAO_PZT_STAGE_Ypos_min = -6.5;
float SCExAO_PZT_STAGE_Ypos_max = -3.5;

// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string
// 4: existing image
//


int SCExAOcontrol_Average_image_cli()
{
	 if(CLI_checkarg(2,2)+CLI_checkarg(3,3)==0)
    {
		
      SCExAOcontrol_Average_image(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.numl, data.cmdargtoken[3].val.string);
      return 0;
    }
  else
    return 1;
}


int SCExAOcontrol_mv_DMstage_cli()
{
	 if(CLI_checkarg(1,2)+CLI_checkarg(2,2)==0)
    {
      SCExAOcontrol_mv_DMstage(data.cmdargtoken[1].val.numl, data.cmdargtoken[2].val.numl);
      return 0;
    }
  else
    return 1;
}


int SCExAOcontrol_PyramidWFS_AutoAlign_TT_cli()
{
	 if(CLI_checkarg(1,4)==0)
    {
      SCExAOcontrol_PyramidWFS_AutoAlign_TT(data.cmdargtoken[1].val.string);
      return 0;
    }
  else
    return 1;
}

int SCExAOcontrol_PyramidWFS_AutoAlign_cam_cli()
{
	 if(CLI_checkarg(1,4)==0)
    {
      SCExAOcontrol_PyramidWFS_AutoAlign_cam(data.cmdargtoken[1].val.string);
      return 0;
    }
  else
    return 1;
}


//int SCExAOcontrol_SAPHIRA_cam_process(char *IDinname, char *IDoutname)

int SCExAOcontrol_SAPHIRA_cam_process_cli()
{
	 if(CLI_checkarg(1,4)+CLI_checkarg(2,3)==0)
    {
      SCExAOcontrol_SAPHIRA_cam_process(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string);
      return 0;
    }
  else
    return 1;
}



int init_SCExAO_control()
{

  strcpy(data.module[data.NBmodule].name, __FILE__);
  strcpy(data.module[data.NBmodule].info, "SCExAO control");
  data.NBmodule++;


  strcpy(data.cmd[data.NBcmd].key,"scexaoaveim");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = SCExAOcontrol_Average_image_cli;
  strcpy(data.cmd[data.NBcmd].info,"take averaged camera image. Image in shared mem is <imname>.im.shm");
  strcpy(data.cmd[data.NBcmd].syntax,"<imname> <nbcoadd>");
  strcpy(data.cmd[data.NBcmd].example,"scexaoaveim cam1 100");
  strcpy(data.cmd[data.NBcmd].Ccall,"long SCExAOcontrol_Average_image(char *imname, long NbAve)");
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
  strcpy(data.cmd[data.NBcmd].syntax,"<wfscamname>");
  strcpy(data.cmd[data.NBcmd].example,"scexaopywfsttalign wfscam");
  strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_PyramidWFS_AutoAlign_TT(char *WFScam_name);");
  data.NBcmd++;

  strcpy(data.cmd[data.NBcmd].key,"scexaopywfscamalign");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = SCExAOcontrol_PyramidWFS_AutoAlign_cam_cli;
  strcpy(data.cmd[data.NBcmd].info,"move Camera to center pyrWFS");
  strcpy(data.cmd[data.NBcmd].syntax,"no arg");
  strcpy(data.cmd[data.NBcmd].example,"scexaopywfscamalign");
  strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_PyramidWFS_AutoAlign_cam();");
  data.NBcmd++;

  strcpy(data.cmd[data.NBcmd].key,"scexaosaphiraproc");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = SCExAOcontrol_SAPHIRA_cam_process_cli;
  strcpy(data.cmd[data.NBcmd].info,"process saphira camera images");
  strcpy(data.cmd[data.NBcmd].syntax,"<input> <output>");
  strcpy(data.cmd[data.NBcmd].example,"scexaosaphiraproc");
  strcpy(data.cmd[data.NBcmd].Ccall,"int SCExAOcontrol_SAPHIRA_cam_process(char *IDinname, char *IDoutname)");
  data.NBcmd++;




  // add atexit functions here
  
  
  return 0;
}




long SCExAOcontrol_Average_image(char *imname, long NbAve, char *IDnameout)
{
    long ID;
    long IDdark;
    long IDcam;
    int slice;
    long k;
    long xsize, ysize, xysize;
    char *ptrv;
    unsigned short *arrayutmp;
    long ii;
    long NBcoadd = 1;
    long kw;
	double darkv;
	long IDv;
	char imnameave[200];
	long long cntref;
	int semval;
	
	cntref = -1;
	
    IDcam = image_ID(imname);
    if(IDcam ==-1)
        IDcam = read_sharedmem_image(imname);


 

    xsize = data.image[IDcam].md[0].size[0];
    ysize = data.image[IDcam].md[0].size[1];
    xysize = xsize*ysize;

    ID = create_2Dimage_ID(IDnameout, xsize, ysize);

    arrayutmp = (unsigned short*) malloc(sizeof(unsigned short)*xysize);

/*if(data.image[IDcam].sem==1)
{	printf("Bringing semaphore to 0\n");
	fflush(stdout);
	while(sem_trywait(data.image[IDcam].semptr)==0)
	{}
}*/
/*	if(data.image[IDcam].sem==1)
		sem_init(data.image[IDcam].semptr, 0, 0);
	*/
    for(k=0; k<NbAve; k++)
    {
		if(data.image[IDcam].sem==0)
		{
			while(cntref==data.image[IDcam].md[0].cnt0) // test if new frame exists
				usleep(10);
		}
		else
			{
			sem_getvalue(data.image[IDcam].semptr, &semval);
			//printf("semaphore value = %d   \n", semval);
			//fflush(stdout);
			
		//	printf("Waiting for semaphore to post .... ");
		//	fflush(stdout);
			
			sem_wait(data.image[IDcam].semptr);
			
//			printf(" done\n");
	//		fflush(stdout);
			}
			
        slice = data.image[IDcam].md[0].cnt1;
        if(slice==-1)
            slice = data.image[IDcam].md[0].size[2]-1;
	//	printf("Reading slice %d\n", slice);
		
        ptrv = (char*) data.image[IDcam].array.U;
        ptrv += sizeof(unsigned short)*slice*xysize;
        memcpy (arrayutmp, ptrv, sizeof(unsigned short)*xysize);
        for(ii=0; ii<xysize; ii++)
            data.image[ID].array.F[ii] += (float) arrayutmp[ii];


        cntref = data.image[IDcam].md[0].cnt0;
    }

  for(ii=0; ii<xysize; ii++)
        data.image[ID].array.F[ii] /= NbAve*NBcoadd;

	
	if((IDdark=image_ID("wfsdark"))!=-1)
		{		
			
			for(ii=0; ii<xysize; ii++)
				data.image[ID].array.F[ii] -= data.image[IDdark].array.F[ii];
		}


    free(arrayutmp);

    return(ID);
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

int SCExAOcontrol_PyramidWFS_AutoAlign_TT_DM(char *WFScam_name)
{
    long ID;
    long xsize, ysize;
    long ii, jj;
    double tot00, tot01, tot10, tot11, tot;
	double xsig, ysig;
	long ttxpos, ttypos;
	double gain = 1.0;


    ID = SCExAOcontrol_Average_image(WFScam_name, 5000, "imwfs");
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


int SCExAOcontrol_PyramidWFS_AutoAlign_TT(char *WFScam_name)
{
    long ID;
    long xsize, ysize;
    long ii, jj;
    double tot00, tot01, tot10, tot11, tot;
    double tot00x, tot01x, tot10x, tot11x;
    double tot00y, tot01y, tot10y, tot11y;
    double xsig, ysig;
    long ttxpos, ttypos;
    double gain = 0.1;
    char command[200];
	int r; 
	double x, y;
	double totx, toty;

//        SCExAOcontrol_PyramidWFS_AutoAlign_TT_DM();
  // exit(0);
  
while(1)
{
        ID = SCExAOcontrol_Average_image(WFScam_name, 1000, "imwfs");
       xsize = data.image[ID].md[0].size[0];
        ysize = data.image[ID].md[0].size[1];

        printf("%ld x %ld image\n", xsize, ysize);

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
					x = 1.0*ii/(xsize/2)-0.5;
					y = 1.0*jj/(ysize/2)-0.5;
					tot00x += x*data.image[ID].array.F[jj*xsize+ii];
					tot00y += y*data.image[ID].array.F[jj*xsize+ii];
					tot00 += data.image[ID].array.F[jj*xsize+ii];
				}
				
        for(ii=xsize/2; ii<xsize; ii++)
            for(jj=0; jj<ysize/2; jj++)
                {
					x = 1.0*(ii-xsize/2)/(xsize/2)-0.5;
					y = 1.0*jj/(ysize/2)-0.5;
					tot10x += x*data.image[ID].array.F[jj*xsize+ii];
					tot10y += y*data.image[ID].array.F[jj*xsize+ii];
					tot10 += data.image[ID].array.F[jj*xsize+ii];
				}
				
        for(ii=0; ii<xsize/2; ii++)
            for(jj=ysize/2; jj<ysize; jj++)
                {
					x = 1.0*ii/(xsize/2)-0.5;
					y = 1.0*(jj-ysize/2)/(ysize/2)-0.5;
					tot01x += x*data.image[ID].array.F[jj*xsize+ii];
					tot01y += y*data.image[ID].array.F[jj*xsize+ii];					
					tot01 += data.image[ID].array.F[jj*xsize+ii];
				}
				
        for(ii=xsize/2; ii<xsize; ii++)
            for(jj=ysize/2; jj<ysize; jj++)
                {
					x = 1.0*(ii-xsize/2)/(xsize/2)-0.5;
					y = 1.0*(jj-ysize/2)/(ysize/2)-0.5;
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
		
		totx = 0.25*(tot00x+tot10x+tot10x+tot11x);
		toty = 0.25*(tot00y+tot10y+tot10y+tot11y);

		printf(" PUP X   %+6.4f %+6.4f %+6.4f %+6.4f  -> %+6.4f\n", tot00x, tot01x, tot10x, tot11x, totx);
		printf(" PUP Y   %+6.4f %+6.4f %+6.4f %+6.4f  -> %+6.4f\n", tot00y, tot01y, tot10y, tot11y, toty);

        xsig = tot01-tot10;
        ysig = tot11-tot00;
        printf(" sig = %6.4f  x %6.4f\n", xsig, ysig);

        /// 1 V step -> sig = 0.2 for modulation = 0.3
        SCExAO_PZT_STAGE_Xpos += gain*(xsig/0.2);
        SCExAO_PZT_STAGE_Ypos -= gain*(ysig/0.2);

		if(SCExAO_PZT_STAGE_Xpos<SCExAO_PZT_STAGE_Xpos_min)
			SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_min;
		if(SCExAO_PZT_STAGE_Xpos>SCExAO_PZT_STAGE_Xpos_max)
			SCExAO_PZT_STAGE_Xpos = SCExAO_PZT_STAGE_Xpos_max;

 		if(SCExAO_PZT_STAGE_Ypos<SCExAO_PZT_STAGE_Ypos_min)
			SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_min;
		if(SCExAO_PZT_STAGE_Ypos>SCExAO_PZT_STAGE_Ypos_max)
			SCExAO_PZT_STAGE_Ypos = SCExAO_PZT_STAGE_Ypos_max;

		// sig X
		sprintf(command, "analog_output.py voltage D %5.3f\n", SCExAO_PZT_STAGE_Xpos);
		printf("%s", command);
		r = system(command);
			
		// sig Y
		sprintf(command, "analog_output.py voltage C %5.3f\n", SCExAO_PZT_STAGE_Ypos);
		printf("%s", command);
		r = system(command);

        save_fits("imwfs", "!./tmp/imwfs_alignTT.fits");
}

    return(0);
}






/** assumes imref has been loaded */
int SCExAOcontrol_PyramidWFS_AutoAlign_cam(char *WFScam_name)
{
    FILE *fp;
    long ID, IDc;
    long IDref;
    long ii, jj;
    long brad = 30; // box radius
    double totx, toty, tot;
    double alpha = 20.0;
    double peak, v;
	double gain = 0.2;
	long stepx, stepy;
	int r;
	char command[200];
	long delayus = 1000000;
	long NBframes = 5000;



	/// read position of stages
	if((fp = fopen("./status/pcampos.txt", "r"))!=NULL)
	{
		r = fscanf(fp, "%ld %ld\n", &SCExAO_Pcam_Xpos, &SCExAO_Pcam_Ypos);
		fclose(fp);
	}
	
    IDref = image_ID("imref");
    
    while(1)
    {    
    ID = SCExAOcontrol_Average_image(WFScam_name, NBframes, "imwfs");
	save_fits("imwfs", "!./tmp/imwfs_aligncam.fits");
	
	
    tot = 0.0;
    for(ii=0; ii<pXsize*pYsize; ii++)
        tot += data.image[ID].array.F[ii];
    for(ii=0; ii<pXsize*pYsize; ii++)
        data.image[ID].array.F[ii] /= tot;

    /** compute offset */
    fft_correlation("imwfs", "imref", "outcorr");
    IDc = image_ID("outcorr");
	peak = 0.0;
	for(ii=0; ii<pXsize*pYsize; ii++)
		if(data.image[IDc].array.F[ii]>peak)
			peak = data.image[IDc].array.F[ii];
    
    for(ii=0; ii<pXsize*pYsize; ii++)
        if(data.image[IDc].array.F[ii]>0.0)
            data.image[IDc].array.F[ii] = pow(data.image[IDc].array.F[ii]/peak,alpha);
        else
            data.image[IDc].array.F[ii] = 0.0;

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
    delete_image_ID("outcorr");

    printf("  %6.4f  x  %6.4f\n", totx, toty);

	stepx = (long) (-gain*totx/0.7*10000.0);
	stepy = (long) (gain*toty/0.7*10000.0);

	printf("STEP : %ld %ld\n", stepx, stepy);

	SCExAO_Pcam_Xpos += stepx;
	SCExAO_Pcam_Ypos += stepy;

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
	}

    return(0);
}




/** SAPHIRA image */
int SCExAOcontrol_SAPHIRA_cam_process(char *IDinname, char *IDoutname)
{
    long IDout;
    long IDin;
    long xsize, ysize, zsize;
    long *sizeoutarray;
    double *coeffarray;
    long k;
    long IDtmp;
    long ii, jj;
    long ID2dtmp, ID3dtmp;
    float v0;
    long xysize;
    double v1, vk, vt, vv;
    long kk;
    long cnt0, cnt1, cnt2;
    float SATURATION = 25000;
    long kold;
    long iter;

    long IDavek, IDavev, IDavecnt, IDcnt, IDcnt1;
	int *satarray;

    IDin = image_ID(IDinname);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];
    xysize = xsize*ysize;


    sizeoutarray = (long*) malloc(sizeof(long)*3);
    sizeoutarray[0] = xsize;
    sizeoutarray[1] = ysize;
    sizeoutarray[2] = zsize;

	satarray = (int*) malloc(sizeof(long)*xysize);

    coeffarray = (double*) malloc(sizeof(double)*zsize);

    for(k=0; k<zsize; k++)
    {
        coeffarray[k] = 1.0/zsize;
    }


    ID2dtmp = create_image_ID("saphira2dtmp", 2, sizeoutarray, FLOAT, 1, 0); // intermediate resutl
    ID3dtmp = create_image_ID("saphira3dtmp", 3, sizeoutarray, FLOAT, 1, 0); // 3D cube stores camera image, goes to zero when saturation reached


    IDavek = create_image_ID("avek", 2, sizeoutarray, FLOAT, 1, 0);
    IDavev = create_image_ID("avev", 2, sizeoutarray, FLOAT, 1, 0);
    IDavecnt = create_image_ID("avecnt", 2, sizeoutarray, USHORT, 1, 0); // number of entries per pixel


    IDout = create_image_ID(IDoutname, 2, sizeoutarray, FLOAT, 1, 0);

    if(data.image[IDin].sem == 0)
    {
        printf("Error: no semaphore detected\n");
        exit(0);
    }


    // drive semaphore to zero
    while(sem_trywait(data.image[IDin].semptr)==0) {}


    printf("\n");
    for(ii=0; ii<xysize; ii++)
    {
        data.image[IDavek].array.F[ii] = 0.0;
        data.image[IDavev].array.F[ii] = 0.0;
        data.image[IDavecnt].array.U[ii] = 1;
		satarray[ii] = 0;
		}

    iter = 0;
    kold = -1;


    while(1)
    {
        sem_wait(data.image[IDin].semptr);
        while(sem_trywait(data.image[IDin].semptr)==0) {}

        k = data.image[IDin].md[0].cnt1;
        printf("%ld   slice %ld written [%ld] \n      ", iter, k, IDin);
        fflush(stdout);

        if(k<kold)
        {
            // complete last cube : compute missing pixels
            if(kold>0)
            {
                for(ii=0; ii<xysize; ii++)
                {
					v0 = 0.0;
					v1 = 0.0;
                   data.image[IDavek].array.F[ii] /= (data.image[IDavecnt].array.U[ii]-1);
                   data.image[IDavev].array.F[ii] /= (data.image[IDavecnt].array.U[ii]-1);
                   for(kk=1; kk<data.image[IDavecnt].array.U[ii]+1; kk++)
                           {
                              vk = 1.0*kk - data.image[IDavek].array.F[ii];
                              vv = 1.0*data.image[ID3dtmp].array.F[kk*xysize+ii] - data.image[IDavev].array.F[ii] ;
                              v0 += vk*vv;
                             v1 += vk*vk;
							
							if((iter==2)&&(ii==1000))
								printf("========= (%ld %f) -> %f %f  -> %f %f -> %f   [%f %f] [%d]\n", kk, data.image[ID3dtmp].array.F[kk*xysize+ii], vk, vv, v0, v1, v0/v1, data.image[IDavek].array.F[ii], data.image[IDavev].array.F[ii], data.image[IDavecnt].array.U[ii]);
                          }
                    
                    
                   data.image[ID2dtmp].array.F[ii] = v0/v1;
                   
                   
                   
                    //if(data.image[IDcnt1].array.U[ii]==1)
                    //{
                        /*        kavearray[ii] /= cntarray[ii];
                                vavearray[ii] /= cntarray[ii];
                                v0 = 0.0;
                                v1 = 0.0;
                                for(kk=0; kk<cntarray[ii]; kk++)
                                {
                                    vk = 1.0*kk - kavearray[ii];
                                    vv = 1.0*data.image[ID3dtmp].array.F[kk*xysize+ii]-vavearray[ii];
                                    v0 += vk*vv;
                                    v1 += vk;
                                }

                                data.image[ID2dtmp].array.F[ii] = v0/v1;
                                cntarray1[ii] = 0;*/


                    //}
                }
            }


            iter++;
            printf("\n CUBE COMPLETED -> 2D image ready\n");
            data.image[IDout].md[0].write = 1;
            memcpy(data.image[IDout].array.F, data.image[ID2dtmp].array.F, sizeof(float)*xysize);
            data.image[IDout].md[0].cnt0 ++;
            data.image[IDout].md[0].write = 0;

            if(iter>2)
            {
                save_fits(IDoutname, "!test.fits");
                save_fits("saphira3dtmp", "!test_saphira3dtmp.fits");
                save_fits("saphira2dtmp", "!test_saphira2dtmp.fits");
                save_fits("avek", "!test_avek.fits");
                save_fits("avev", "!test_avev.fits");
                save_fits("avecnt", "!test_avecnt.fits");
                exit(0);
            }

            cnt0 = 0;
            cnt1 = 0;
            cnt2 = 0;

            for(ii=0; ii<xysize; ii++)
            {
                data.image[IDcnt].array.U[ii] = 1;
                data.image[IDavek].array.F[ii] = 0.0;
                data.image[IDavev].array.F[ii] = 0.0;
                data.image[IDavecnt].array.U[ii] = 0;
                data.image[ID2dtmp].array.F[ii] = 0.0;
				data.image[ID3dtmp].array.F[ii] = 0.0;
				satarray[ii] = 0;
            }
        }


        data.image[ID2dtmp].md[0].write = 1;
        data.image[ID3dtmp].md[0].write = 1;
        cnt2 = 0;





	
		for(ii=0; ii<xysize; ii++)
		{
			v0 = 1.0*data.image[IDin].array.U[k*xysize+ii];
			if((v0>SATURATION)||(v0<0.0))
				{
					data.image[ID3dtmp].array.F[k*xysize+ii] = v0;
					satarray[ii] = 1;
				}
			else if (satarray[ii]==0)
				{
					data.image[ID3dtmp].array.F[k*xysize+ii] = v0;
					if(k>0)
					{
					data.image[IDavek].array.F[ii] += 1.0*k;
					data.image[IDavev].array.F[ii] += v0;
                    data.image[IDavecnt].array.U[ii]++;
					}
                }            				
		}

/*


        for(ii=0; ii<xysize; ii++)
        {
            cnt2++;
            if(k<data.image[IDcnt].array.F[ii])
            {
                cnt0++;
                v0 = 1.0*data.image[IDin].array.U[k*xysize+ii];

                if((v0>SATURATION))
                {
                    data.image[ID3dtmp].array.F[k*xysize+ii] = 0.0;
                    cntarray[ii] = k;
                }
                else
                {
                    data.image[ID3dtmp].array.F[k*xysize+ii] = v0;
                    data.image[IDavek].array.F[ii] += 1.0*k;
                    data.image[IDavev].array.F[ii] += v0;
                    data.image[IDavecnt].array.F[ii] += 1.0;
                }
            }
            else if (data.image[IDcnt1].array.F[ii]==1)
            {
                data.image[IDavek].array.F[ii] /= data.image[IDavecnt].array.F[ii];
                data.image[IDavev].array.F[ii] /= data.image[IDavecnt].array.F[ii];
                v0 = 0.0;
                v1 = 0.0;
                for(kk=0; kk<data.image[IDavecnt].array.F[ii]; kk++)
                {
                    vk = 1.0*kk - data.image[IDavek].array.F[ii];
                    vv = 1.0*data.image[ID3dtmp].array.F[kk*xysize+ii] - data.image[IDavev].array.F[ii];
                    v0 += vk*vv;
                    v1 += vk;
                }

                data.image[ID2dtmp].array.F[ii] = v0/v1;
                cntarray1[ii] = 0;
                cnt1++;
            }
            //	data.image[ID2dtmp].array.F[ii] = 1.0*cntarray1[ii];
        }

        printf(" %6ld  %6ld  %6ld   ", cnt2, cnt0, cnt1);
        fflush(stdout);
        cnt2 = 0;
*/
        data.image[ID2dtmp].md[0].cnt0++;
        data.image[ID3dtmp].md[0].cnt0++;

        data.image[ID2dtmp].md[0].write = 0;
        data.image[ID3dtmp].md[0].write = 0;


        kold = k;
    }
    printf("\n");

    free(sizeoutarray);
    free(coeffarray);




    return(IDout);
}



