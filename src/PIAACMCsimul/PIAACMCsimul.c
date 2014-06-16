#include <fitsio.h> 
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>

#include "CLIcore.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "COREMOD_iofits/COREMOD_iofits.h"

#include "info/info.h"
#include "fft/fft.h"
#include "image_gen/image_gen.h"
#include "WFpropagate/WFpropagate.h"
#include "statistic/statistic.h"
#include "linopt_imtools/linopt_imtools.h"
#include "OpticsMaterials/OpticsMaterials.h"
#include "image_filter/image_filter.h"

#include "PIAACMCsimul/PIAACMCsimul.h"
#include "OptSystProp/OptSystProp.h"



/**
 * @file PIAACMCsimul.c
 * @author Olivier Guyon
 */




extern DATA data;

#define SBUFFERSIZE 2000

///  Current configuration directory
char piaacmcconfdir[200];

OPTSYST *optsyst;
int optsystinit = 0;
long IDx, IDy, IDr, IDPA;


double LAMBDASTART = 0.7e-6;
double LAMBDAEND = 0.9e-6;
#define NBLAMBDA 5

MIRRORPIAACMCDESIGN *piaacmc;
OPTSYST *optsyst;

int focmMode = -1; // if != -1, compute only impulse response to corresponding zone

// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string
// 4: existing image
//

int PIAACMCsimul_run_cli()
{
  
  if(CLI_checkarg(1,2)==0)
    {
      PIAACMCsimul_run(data.cmdargtoken[1].val.numl);
      return 0;
    }
  else
    return 1;
}


/**
 * Initializes module
 */
int init_PIAACMCsimul()
{
  strcpy(data.module[data.NBmodule].name, __FILE__);
  strcpy(data.module[data.NBmodule].info, "PIAACMC system simulation");
  data.NBmodule++;
  


  strcpy(data.cmd[data.NBcmd].key,"piaacmcsimrun");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = PIAACMCsimul_run_cli;
  strcpy(data.cmd[data.NBcmd].info,"Simulate PIAACMC");
  strcpy(data.cmd[data.NBcmd].syntax,"<configuration index [long]>");
  strcpy(data.cmd[data.NBcmd].example,"piaacmcsimrun");
  strcpy(data.cmd[data.NBcmd].Ccall,"int PIAACMCsimul_run(int confindex)");
  data.NBcmd++;
 
   
  // add atexit functions here
  atexit(PIAACMCsimul_free);

  return 0;

}

/**
 * Frees memory for module
 */
void PIAACMCsimul_free( void )
{
  if(optsystinit ==1)
    {
      free(optsyst);
    }
}


























// ************************************************************************************
//
//           FOCAL PLANE MASK
//
// ************************************************************************************

/**
 * @param[out]  IDname  Name of output image
 */

long PIAACMCsimul_mkFPM_zonemap(char *IDname)
{
  long NBzones;
  long ID;
  double x, y, r;
  long ii, jj;
  long zi;
  long *sizearray;
  
  sizearray = (long*) malloc(sizeof(long)*2);
  sizearray[0] = piaacmc[0].fpmarraysize;
  sizearray[1] = piaacmc[0].fpmarraysize;
  ID = create_image_ID(IDname, 2, sizearray, USHORT, 0, 0);
  free(sizearray);

  //  ID = create_2Dimage_ID(IDname, piaacmc[0].fpmarraysize, piaacmc[0].fpmarraysize);
  for(ii=0;ii<piaacmc[0].fpmarraysize;ii++)
    for(jj=0;jj<piaacmc[0].fpmarraysize;jj++)
      {
	x = (2.0*ii-1.0*piaacmc[0].fpmarraysize)/piaacmc[0].fpmarraysize;
	y = (2.0*jj-1.0*piaacmc[0].fpmarraysize)/piaacmc[0].fpmarraysize;
	r = sqrt(x*x+y*y); 
	zi = (long) ceil((1.0-r)*piaacmc[0].NBrings);
	if(zi<0.1)
	  zi = 0;
	if(zi>piaacmc[0].NBrings)
	  zi = piaacmc[0].NBrings;
	data.image[ID].array.U[jj*piaacmc[0].fpmarraysize+ii] = (unsigned short int) zi;
      }
  piaacmc[0].focmNBzone = piaacmc[0].NBrings;

  return ID;
}




//
// makes 1-fpm CA
// if mode = -1, make whole 1-fpm
// if mode = zone, make only 1 zone 
//
long PIAACMCsimul_mkFocalPlaneMask(char *IDzonemap_name, char *ID_name, int mode)
{
  long ID;
  long IDz;
  long size;
  long nblambda;
  long k;
  long ii, jj;
  double x, y, r; // in meter
  long ii1, jj1;
  double fpscale; // [m/pix]
  int zi;
  double t, a, re, im, amp, pha;
  long size2;


  
  size = optsyst[0].size;
  size2 = size*size;
  nblambda = optsyst[0].nblambda;
 

  IDz = image_ID(IDzonemap_name);
  ID = create_3DCimage_ID(ID_name, size, size, nblambda);
 
  // CORONAGRAPHS_TDIAM/CORONAGRAPHS_PSCALE/CORONAGRAPHS_ARRAYSIZE

  for(k=0;k<nblambda;k++)
    {
      fpscale = (2.0*piaacmc[0].beamrad/piaacmc[0].pixscale)/piaacmc[0].size/piaacmc[0].fpzfactor*optsyst[0].lambdaarray[k]*piaacmc[0].Fratio;
      printf("LAMBDA = %10.5g m    SCALE = %10.5g m/pix   size=%4ld  rad=%g\n", optsyst[0].lambdaarray[k], fpscale, size, piaacmc[0].fpmRad);
      

      for(ii=0;ii<size;ii++)
	for(jj=0;jj<size;jj++)
	  {
	    x = (1.0*ii-size/2)*fpscale; // [m]
	    y = (1.0*jj-size/2)*fpscale; // [m]	    

	    ii1 = (long) ( (0.5 + 0.5*x/piaacmc[0].fpmRad)*piaacmc[0].fpmarraysize);
	    jj1 = (long) ( (0.5 + 0.5*y/piaacmc[0].fpmRad)*piaacmc[0].fpmarraysize);
	    if((ii1>-1)&&(ii1<piaacmc[0].fpmarraysize)&&(jj1>-1)&&(jj1<piaacmc[0].fpmarraysize))
	      {
		zi = data.image[IDz].array.U[jj1*piaacmc[0].fpmarraysize+ii1];
		t = data.image[piaacmc[0].zonezID].array.D[zi-1]; // thickness
		a = data.image[piaacmc[0].zoneaID].array.D[zi-1]; // amplitude transmission
	      }
	    else
	      {
		zi = 0;
		t = 0.0;
		a = 1.0;
	      }

	    if(mode == -1)
	      {
		if(zi>0.1)
		  {
		    amp = a;
		    pha = OPTICSMATERIALS_pha_lambda(piaacmc[0].fpmmaterial, t, optsyst[0].lambdaarray[k]);
		    re = amp*cos(pha);
		    im = amp*sin(pha);
		    data.image[ID].array.CF[k*size2+jj*size+ii].re = 1.0-re;
		    data.image[ID].array.CF[k*size2+jj*size+ii].im = -im;
		  }
		else
		  {
		    data.image[ID].array.CF[k*size2+jj*size+ii].re = 0.0;
		    data.image[ID].array.CF[k*size2+jj*size+ii].im = 0.0;
		  }
	      }
	    else // impusle response from single zone
	      {
		if(mode == zi)  
		  {
		    amp = 1.0;
		    pha = 0.0; //OPTICSMATERIALS_pha_lambda(piaacmc[0].fpmmaterial, t, optsyst[0].lambdaarray[k]);
		    re = amp*cos(pha);
		    im = amp*sin(pha);
		    data.image[ID].array.CF[k*size2+jj*size+ii].re = 1.0;
		    data.image[ID].array.CF[k*size2+jj*size+ii].im = 0.0;
		  }
		else
		  {
		    data.image[ID].array.CF[k*size2+jj*size+ii].re = 0.0;
		    data.image[ID].array.CF[k*size2+jj*size+ii].im = 0.0;
		  }
	      }
	    
	  }
    }

  return(ID);
}















//
// initializes the optsyst structure to simulate reflective PIAACMC system
//
void PIAACMCsimul_init( MIRRORPIAACMCDESIGN *design, long index, double TTxld, double TTyld )
{
  long k, i;
  long size;
  double x, y, PA;
  long ii, jj;
  long nblambda;
  long size2;
  double beamradpix;
  long kx, ky, kxy;
  long IDpiaaz0, IDpiaaz1;
  long surf;
  long IDa;
  char fname_pupa0[200];
  long ID;
  long elem;
  char fname[200];


  optsyst[0].nblambda = design[index].nblambda;
  nblambda = optsyst[0].nblambda;
  for(k=0;k<optsyst[0].nblambda;k++)
    optsyst[0].lambdaarray[k] = LAMBDASTART + (0.5+k)*(LAMBDAEND-LAMBDASTART)/optsyst[0].nblambda;


  optsyst[0].beamrad = design[index].beamrad; // 8mm
  optsyst[0].size = design[index].size;
  size = optsyst[0].size;
  size2 = size*size;
  optsyst[0].pixscale = design[index].pixscale;
  optsyst[0].DFTgridpad = 2; // 0 for full DFT sampling, >0 for faster execution

  beamradpix = optsyst[0].beamrad/optsyst[0].pixscale;
  printf("BEAM RADIUS = %f pix\n", beamradpix);



  // define optical elements and locations
  optsyst[0].NB_DM = 0;
  optsyst[0].NB_asphsurfm = 2;
  optsyst[0].NB_asphsurfr = 0;
  
  optsyst[0].NBelem = 100; // to be updated later 


  elem = 0;
  // ------------------- elem 0: input pupil -----------------------
  optsyst[0].elemtype[elem] = 1; // pupil mask
    // input pupil
  sprintf(fname_pupa0, "pupa0_%ld.fits", size);
  if(file_exists(fname_pupa0)==1)
    load_fits(fname_pupa0, "pupa0");
  IDa = image_ID("pupa0");
  if(IDa==-1)
    {
      printf("CREATING INPUT PUPIL\n");
      if(IDa!=-1)
	delete_image_ID("pupa0");
      IDa = create_3Dimage_ID("pupa0", size, size, nblambda);
      
      ID = image_ID("telpup");
      if(ID==-1)
	if(file_exists("telpup.fits")==1)
	  ID = load_fits("telpup.fits", "telpup");
      

      if(ID==-1)
	{
	  for(k=0;k<nblambda;k++)
	    for(ii=0;ii<size2;ii++)
	      {
		if((data.image[IDr].array.F[ii]>0.3)&&(data.image[IDr].array.F[ii]<1.0))
		  data.image[IDa].array.F[k*size2+ii] = 1.0;
		else
		  data.image[IDa].array.F[k*size2+ii] = 0.0;
	      }     
	}
      else
	 for(k=0;k<nblambda;k++)
	    for(ii=0;ii<size2;ii++)
	      {
		if(data.image[ID].array.F[ii]>0.5)
		  data.image[IDa].array.F[k*size2+ii] = 1.0;
		else
		  data.image[IDa].array.F[k*size2+ii] = 0.0;
	      }     

      sprintf(fname_pupa0, "!pupa0_%ld.fits", size);
      save_fl_fits("pupa0", fname_pupa0);      
    }
  optsyst[0].elemarrayindex[elem] = IDa;  
  optsyst[0].elemZpos[elem] = 0.0;



  elem++;





  // pointing (simulated as mirror)
  ID = create_2Dimage_ID("TTm", size, size);

  for(ii=0;ii<size;ii++)
    for(jj=0;jj<size;jj++)
      {
	x = (1.0*ii-0.5*size)/beamradpix;
	y = (1.0*jj-0.5*size)/beamradpix;
	data.image[ID].array.F[jj*size+ii] = 0.25*(TTxld*x+TTyld*y)*(LAMBDAEND+LAMBDASTART)*0.5;
      }
  save_fits("TTm","!TTm.fits");
  optsyst[0].elemtype[elem] = 3; // reflective mirror   
  optsyst[0].elemarrayindex[elem] = 0; // index
  optsyst[0].ASPHSURFMarray[0].surfID = ID; 
  optsyst[0].elemZpos[elem] = 0.0;
  elem++;

 
  IDpiaaz0 = image_ID("piaa0z");
  IDpiaaz1 = image_ID("piaa1z");


  // ------------------- elem 2: reflective PIAA M0  -----------------------  
  optsyst[0].elemtype[elem] = 3; // reflective PIAA M0   
  optsyst[0].elemarrayindex[elem] = 1; // index
  optsyst[0].ASPHSURFMarray[1].surfID = IDpiaaz0; 
  optsyst[0].elemZpos[elem] = 0.0;
  elem++;

  // ------------------- elem 3: reflective PIAA M1  -----------------------  
  optsyst[0].elemtype[elem] = 3; // reflective PIAA M1
  optsyst[0].elemarrayindex[elem] = 2;
  optsyst[0].ASPHSURFMarray[2].surfID = IDpiaaz1; 
  optsyst[0].elemZpos[elem] = design[index].piaasep;
  elem++;


  // ------------------- elem 4 opaque mask at reflective PIAA M1  -----------------------  
  optsyst[0].elemtype[elem] = 1; // opaque mask
  ID = make_disk("piaam1mask", size, size, 0.5*size, 0.5*size, design[index].r1lim*beamradpix);
  optsyst[0].elemarrayindex[elem] = ID;
  optsyst[0].elemZpos[elem] = design[index].piaasep;
  save_fits("piaam1mask", "!piaam1mask.fits");
  elem++;
  

  // --------------------  elem 5: focal plane mask ------------------------
  optsyst[0].elemtype[elem] = 5; // focal plane mask 
  optsyst[0].elemarrayindex[elem] = 0;
  
  list_image_ID();
  optsyst[0].FOCMASKarray[0].fpmID = PIAACMCsimul_mkFocalPlaneMask("fpmzmap", "piaacmcfpm", focmMode); // if -1, this is 1-fpm; otherwise, this is impulse response from single zone
  //  save_fits("fpmza", "!TESTfpmza.fits");
  //  save_fits("fpmzt", "!TESTfpmzt.fits");
  // save_fits("fpmzmap", "!TESTfpmzmap.fits");

  if(0)// testing
    {
      sprintf(fname, "!focma_%d.fits", focmMode);
      mk_amph_from_complex("piaacmcfpm", "fpma", "fpmp");
      save_fits("fpma", fname);
      save_fits("fpmp", "!fpmp.fits");
      delete_image_ID("fpma");
      delete_image_ID("fpmp");
      //      exit(0);
    }
  


  optsyst[0]. FOCMASKarray[0].zfactor = design[index].fpzfactor;
  optsyst[0].elemZpos[elem] = design[index].piaasep; // plane from which FT is done
  elem++;

  
  // --------------------  elem 6, 7: Lyot masks  ------------------------
  for(i=0;i<design[index].NBLyotStop;i++)
    {
      optsyst[0].elemtype[elem] = 1; // Lyot mask 
      optsyst[0].elemarrayindex[elem] = design[index].IDLyotStop[i];  
      optsyst[0].elemZpos[elem] =  design[index].LyotStop_zpos[i];
      elem++;
    }



  // --------------------  elem 8: inv PIAA1 ------------------------
  optsyst[0].elemtype[elem] = 3; // reflective PIAA M1
  optsyst[0].elemarrayindex[elem] = 2;
  optsyst[0].ASPHSURFMarray[2].surfID = IDpiaaz1; 
  optsyst[0].elemZpos[elem] = 0.0;
  elem++;

  // --------------------  elem 9: inv PIAA0 ------------------------
  optsyst[0].elemtype[elem] = 3; // reflective PIAA M0
  optsyst[0].elemarrayindex[elem] = 1;
  optsyst[0].ASPHSURFMarray[1].surfID = IDpiaaz0; 
  optsyst[0].elemZpos[elem] = design[index].piaasep;
  elem++;

  // --------------------  elem 9: back end mask  ------------------------

  optsyst[0].elemtype[elem] = 1; // Lyot mask 
  ID = make_disk("outmask", size, size, 0.5*size, 0.5*size, 0.92*design[index].beamrad/design[index].pixscale);
  optsyst[0].elemarrayindex[elem] = ID;  
  optsyst[0].elemZpos[elem] =  design[index].piaasep;
  elem++;
  


  optsyst[0].NBelem = elem;

  optsystinit = 1;
}











//
// RADIAL PIAACMC SYSTEM DESIGN (geometrical optics)
//



//
// load and fit radial apodization profile 
// modal basis is mk(r) : cos(r*k*M_PI/1.3) 
//
int PIAACMCsimul_load2DRadialApodization(char *IDapo_name, float beamradpix, float centralObs, char *IDapofit_name)
{
  long NBpts;
  long IDm;
  long sizem;
  long kmax = 10;
  long ID, IDmask, IDin;
  long ii, jj;
  long offset;
  long sizein;
  float eps = 1.0e-4;

  sizem = (long) (beamradpix*2);

  // CREATE MODES IF THEY DO NOT EXIST
  if((IDm=image_ID("APOmodesCos"))==-1)
    {
      IDm = linopt_imtools_makeCosRadModes("APOmodesCos", sizem, kmax, ApoFitCosFact*beamradpix, 1.0);  
      save_fits("APOmodesCos", "!APOmodesCos.fits");
    }
  
  // CREATE MASK AND CROP INPUT
  IDmask = create_2Dimage_ID("fitmaskapo", sizem, sizem);
  
  IDin = image_ID(IDapo_name);
  sizein = data.image[IDin].md[0].size[0];
  ID = create_2Dimage_ID("_apoincrop", sizem, sizem);
  offset = (sizein-sizem)/2;
  for(ii=0;ii<sizem;ii++)
    for(jj=0;jj<sizem;jj++)
      {
	data.image[ID].array.F[jj*sizem+ii] = data.image[IDin].array.F[(jj+offset)*sizein+(ii+offset)];
	if((data.image[ID].array.F[jj*sizem+ii]>eps)&&(ii%1==0)&&(jj%1==0))
	  data.image[IDmask].array.F[jj*sizem+ii] = 1.0;
      }


  save_fits("_apoincrop", "!_apoincrop.fits");
  save_fits("fitmaskapo", "!fitmaskapo.fits");
  linopt_imtools_image_fitModes("_apoincrop", "APOmodesCos", "fitmaskapo", 1.0e-8, IDapofit_name, 0);


  if(0) // test fit quality
    {
      linopt_imtools_image_construct("APOmodesCos", IDapofit_name, "testapofitsol");
      save_fits("testapofitsol", "!testapofitsol.fits");
      arith_image_sub("_apoincrop", "testapofitsol", "apofitres");
      arith_image_mult("apofitres", "fitmaskapo", "apofitresm");
      save_fits("apofitres", "!apofitres.fits");
      save_fits("apofitresm", "!apofitresm.fits");
      // linopt_imtools_image_fitModes("apofitres", "APOmodesCos", "fitmaskapo", 1.0e-5, "test2c", 0);
      //list_image_ID();
      info_image_stats("apofitresm", "");
    }

  delete_image_ID("_apoincrop");
  delete_image_ID("fitmaskapo");


  return 0;
}







/**
 * computes radial PIAA optics sag
 *
 * this function only works for circular PIAA
 * uses radial PIAACMC design to initialize PIAA optics shapes and focal plane mask
 */

int PIAACMCsimul_init_geomPIAA_rad(char *IDapofit_name)
{
  long i, ii, k;
  double *pup0;
  double *pup1;
  double *flux0cumul;
  double *flux1cumul;

  long IDcoeff;
  long nbcoeff;
  double r;
  FILE *fp;
  double total;

  // to convert r ro r1 (assymptotic outer radius on pup1)
  double coeffa = 3.0; // convergence rate from linear to assymptotic value
  double coeffa1 = 0.5; // convergence radius limit (added to 1.0)

  double r0, r1;
  double FLUX0_in = 0.0; // inside central obstruction
  double FLUX0_out = 0.0; // outside beam edge
  double FLUX1_in = 0.0; // inside central obstruction
  double FLUX1_out = 0.0; // outside beam edge
  double normcoeff;

  // inner profile adjustment
  double a, b, verr, bstep, x, eps1;
  double t0, t0cnt, value;
  int dir, odir;
  long NBistep;
  long iioffset;

  double fluxdens, F0, F1, F2;
  double dr0, dr1, ndr0, ndr1;
  double *piaar0;
  double *piaar1;

  long NBpoints;
  double *piaar00;
  double *piaar11;
  double *piaar01;
  double *piaar10;
  long cnt;
  double tmp;
  double epsilon = 0.000000000001;

  double *piaaM0z;
  double *piaaM1z;
  double r0c, r1c, dx, dy, dist, y3, r0n, slope, dz;

  char fname[200];

  pup0 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  pup1 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  flux0cumul = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  flux1cumul = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);


  // STEP 1: CREATE AMPLITUDE AND CUMULATIVE INTENSITY PROFILES


  // CREATE OUTPUT AMPLITUDE APODIZATION PROFILE AND ITS CUMUL

  IDcoeff = image_ID(IDapofit_name);
  nbcoeff = data.image[IDcoeff].md[0].size[0];
  printf("%ld coefficients\n", nbcoeff);

  total = 0.0;
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    {
      pup1[ii] = 0.0;
      r = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r1lim;
      if(r<1.0)
	r1 = r;
      else
	r1 = 1.0 + (r-1) / pow((1.0 + pow(1.0/coeffa1 * (r-1),coeffa)), 1.0/coeffa);

      for(k=0;k<nbcoeff;k++)
	pup1[ii] += data.image[IDcoeff].array.F[k]*cos(r1*k*M_PI/ApoFitCosFact);      
      if(r<piaacmc[0].centObs1)
	FLUX1_in += pup1[ii]*pup1[ii]*r;
      if(r>1.0)
	FLUX1_out += pup1[ii]*pup1[ii]*r;
      total += pup1[ii]*pup1[ii]*r;
      flux1cumul[ii] = total;
    }


  normcoeff = 1.0/(total-FLUX1_in-FLUX1_out);

  FLUX1_in *= normcoeff;
  FLUX1_out *= normcoeff;
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    {
      r = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r1lim;
      flux1cumul[ii] *= normcoeff;
    }


  printf("outer fluxes 1: %lf %lf\n", FLUX1_in, FLUX1_out);
  

 

 // CREATE FLUX0 
  
  total = 0.0;
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    {
      r = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r0lim;
      pup0[ii] = 1.0;
  
      if(r<piaacmc[0].centObs0)
	FLUX0_in += pup0[ii]*pup0[ii]*r;
      if(r>1.0)
	FLUX0_out += pup0[ii]*pup0[ii]*r;
      
      total += pup0[ii]*pup0[ii]*r;
      flux0cumul[ii] = total;      
    }
  normcoeff = 1.0/(total-FLUX0_in-FLUX0_out);

  FLUX0_in *= normcoeff;
  FLUX0_out *= normcoeff;

  printf("outer fluxes 0: %lf (%lf)    %lf\n", FLUX0_in, FLUX1_in, FLUX0_out);


  //
  // Compute inner pseudo profile 
  //
  b = 0.5;
  bstep = 0.1;
  verr = 1.0;
  NBistep = piaacmc[0].centObs0*piaacmc[0].NBradpts/piaacmc[0].r0lim;
  //  innerprof_cumul = (double*) malloc(sizeof(double)*NBistep);

  while(fabs(verr)>1.0e-9)
    {
      t0 = 0.0;
      t0cnt = 0.0;
      for(ii=0;ii<NBistep;ii++)
	{
	  x = 1.0*ii/NBistep;
	  r = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r0lim;
	  a = 0.5;
	  eps1 = 1e-8;
	  if(x<eps1)
	    x = eps1;
	  if(x>1.0-eps1)
	    x = 1.0-eps1;
	  pup0[ii] =  b + (1.0-b)*(0.5+atan(-a/b/(x*x) + a/pow(x-1.0,2))/M_PI);
	  t0 += r*pup0[ii]* pup0[ii];
	  flux0cumul[ii] = t0;
	}
      
      verr = t0*normcoeff - FLUX1_in;
      
      odir = dir;
      if(verr>0.0) // too much light
	{
	  b /= (1.0+bstep);
	  dir = -1.0;
	}
      else
	{
	  b *= (1.0+bstep);
	  dir = 1.0;
	}
      if(odir*dir<0.0)
	bstep *= 0.1;
      printf(".");
      fflush(stdout);
    }
  printf("\n");
  printf("TOTAL = %f -> %g (%g %g)\n", b, t0*normcoeff, bstep, verr);


  // outer region
  b = 0.5;
  bstep = 0.1;
  verr = 1.0;
  NBistep = piaacmc[0].NBradpts*(piaacmc[0].r0lim-1.0)/piaacmc[0].r0lim;
  //  innerprof_cumul = (double*) malloc(sizeof(double)*NBistep);
  iioffset = (long) (1.0*piaacmc[0].NBradpts/piaacmc[0].r0lim);
  NBistep = piaacmc[0].NBradpts-iioffset;
  while(fabs(verr)>1.0e-9)
    {
      t0 = 0.0;
      t0cnt = 0.0;
      for(ii=0;ii<NBistep;ii++)
	{
	  x = 1.0-1.0*ii/NBistep;
	  r = 1.0+1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r0lim;
	  a = 0.5;
	  eps1 = 1e-8;
	  if(x<eps1)
	    x = eps1;
	  if(x>1.0-eps1)
	    x = 1.0-eps1;
	  pup0[ii+iioffset] =  b + (1.0-b)*(0.5+atan(-a/b/(x*x) + a/pow(x-1.0,2))/M_PI);
	  t0 += r*pup0[ii+iioffset]* pup0[ii+iioffset];
	  flux0cumul[ii+iioffset] = t0;
	}
      
      verr = t0*normcoeff - FLUX1_out;
      
      odir = dir;
      if(verr>0.0) // too much light
	{
	  b /= (1.0+bstep);
	  dir = -1.0;
	}
      else
	{
	  b *= (1.0+bstep);
	  dir = 1.0;
	}
      if(odir*dir<0.0)
	bstep *= 0.1;
      printf(".");
      fflush(stdout);
    }
  printf("\n");
  printf("TOTAL = %f -> %g (%g %g)\n", b, t0*normcoeff, bstep, verr);



  total = 0.0;
  FLUX0_in = 0.0;
  FLUX0_out = 0.0;
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    {
      r = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r0lim;
      if(r<piaacmc[0].centObs0)
	FLUX0_in += pup0[ii]*pup0[ii]*r;
      if(r>1.0)
	FLUX0_out += pup0[ii]*pup0[ii]*r;
      
      total += pup0[ii]*pup0[ii]*r;
      flux0cumul[ii] = total;      
      flux0cumul[ii] *= normcoeff;;
    }
  FLUX0_in *= normcoeff;
  FLUX0_out *= normcoeff;
  
  printf("outer fluxes 0: %lf (%lf)    %lf\n", FLUX0_in, FLUX1_in, FLUX0_out);


  


  fp = fopen("pup01.prof", "w");
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    {
      r0 = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r0lim;
      r1 = 1.0*ii/piaacmc[0].NBradpts*piaacmc[0].r1lim;
      fprintf(fp, "%f %f %g %g %g %g\n", r0, r1, pup0[ii], pup1[ii], flux0cumul[ii], flux1cumul[ii]);
    }
  fclose(fp);

  

   
  // STEP 2: COMPUTE r0 - r1 CORRESPONDANCE

  piaar00 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts); // r0 as a function of r0 index
  piaar11 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts); // r1 as a function of r1 index
  piaar10 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts); // r1 as a function of r0 index
  piaar01 = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts); // r0 as a function of r1 index

  /* computing r0 and r1 */
  /* r0 and r1 are dimensionless */

  /* first, r0 is evenly distributed on the first optic */
  for(i=0;i<piaacmc[0].NBradpts;i++)
    {
      piaar00[i] = piaacmc[0].r0lim*i/piaacmc[0].NBradpts;
      piaar11[i] = piaacmc[0].r1lim*i/piaacmc[0].NBradpts;      
    }

  i=0;
  ii=0;  
  cnt = 0;
  piaar00[0] = 0.0;
  piaar10[0] = 0.0;
  fp = fopen("test0.txt", "w");
  for(i=1;i<piaacmc[0].NBradpts;i++)
    {
      F0 = flux0cumul[i];
      while((flux1cumul[ii]<flux0cumul[i])&&(ii<piaacmc[0].NBradpts))
	ii++;
      F1 = flux1cumul[ii-1];
      F2 = flux1cumul[ii];
      
      /* F0 = F1 + ( (F2-F1)/(ii^2-(ii-1)^2) * ((ii-1+x)^2-(ii-1)^2) ) */
      if(fabs(F2-F1)>0.0000000001)
	fluxdens = (F2-F1)/(2.0*ii-1.0);
      else
	fluxdens = 0.0000000001;
      x = sqrt((F0-F1)/fluxdens+(1.0*ii*ii-2.0*ii+1.0)) + 1.0 - 1.0*ii;

      piaar10[i] = piaacmc[0].r1lim*(1.0*ii-1.0+x)/piaacmc[0].NBradpts;
      fprintf(fp, "%lf %lf %lf\n", piaar00[i], piaar10[i], F0);
    }
  fclose(fp);
  


  i=0;
  ii=0;  
  cnt = 0;
  piaar01[0] = 0.0;
  piaar11[0] = 0.0;
  //  fp = fopen("test1.txt", "w");
  for(i=1;i<piaacmc[0].NBradpts;i++)
    {
      F0 = flux1cumul[i];
      while((flux0cumul[ii]<flux1cumul[i])&&(ii<piaacmc[0].NBradpts))
	ii++;
      F1 = flux0cumul[ii-1];
      F2 = flux0cumul[ii];
      
      /* F0 = F1 + ( (F2-F1)/(ii^2-(ii-1)^2) * ((ii-1+x)^2-(ii-1)^2) ) */
      if(fabs(F2-F1)>0.0000000001)
	fluxdens = (F2-F1)/(2.0*ii-1.0);
      else
	fluxdens = 0.0000000001;
      x = sqrt((F0-F1)/fluxdens+(1.0*ii*ii-2.0*ii+1.0)) + 1.0 - 1.0*ii;

      piaar01[i] = piaacmc[0].r0lim*(1.0*ii-1.0+x)/piaacmc[0].NBradpts;
      //  fprintf(fp, "%lf %lf %lf\n", piaar11[i], piaar01[i], F0);
    }
  //  fclose(fp);
  




  printf("======== Compute PIAA mirror shapes ============\n");
  piaaM0z = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  piaaM1z = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  
  piaaM0z[0] = 0.0;
  piaaM1z[0] = piaacmc[0].piaasep;


  for(i=0;i<piaacmc[0].NBradpts-1;i++)
    {
      r0c = piaar00[i];
      r1c = piaar10[i];
      dx = (r0c-r1c)*piaacmc[0].beamrad;
      dz = piaaM1z[i]-piaaM0z[i];
      dist = sqrt(dx*dx+dz*dz);
      y3 = dist - dz;
      if(fabs(dx)>0.000000001)
	slope = y3/dx;
      else
	slope = 0.0;
      r0n = piaacmc[0].r0lim*(i+1)/piaacmc[0].NBradpts;
      piaaM0z[i+1] = piaaM0z[i] + slope*(r0n-r0c)*piaacmc[0].beamrad;
      
      if(fabs(dx)>0.000000001)
	slope = y3/dx;
      else
	slope = 0.0;      
      piaaM1z[i+1] = piaaM1z[i] + slope*(piaar10[i+1]-r1c)*piaacmc[0].beamrad;
    }      

  sprintf(fname, "%s/PIAA_Mshapes.txt", piaacmcconfdir);
  fp = fopen(fname, "w");
  for(ii=0;ii<piaacmc[0].NBradpts;ii++)
    fprintf(fp, "%18.16f %18.16f %18.16f %18.16f\n", piaar00[ii]*piaacmc[0].beamrad, piaaM0z[ii], piaar10[ii]*piaacmc[0].beamrad, piaaM1z[ii]);
  fclose(fp);



  
  free(flux0cumul);
  free(flux1cumul);
  free(pup0);
  free(pup1);

  free(piaar0);
  free(piaar1);
  free(piaar00);
  free(piaar10);
  free(piaar01);
  free(piaar11);
  

  return(0);
}


















//
// make PIAA shapes from radial sag profile
//
int PIAACMCsimul_mkPIAAMshapes_from_RadSag(char *fname, char *ID_PIAAM0_name, char *ID_PIAAM1_name)
{
  FILE *fp;
  long size;
  long ii, jj;
  long ID_PIAAM0, ID_PIAAM1;
 
   long k;

  double x, y, r, r1;

  double *r0array;
  double *z0array;
  double *r1array;
  double *z1array;

  double alpha;
  double r00, r01;
  double val;

  double beamradpix;
  int ret;


  size = piaacmc[0].size;
  beamradpix = piaacmc[0].beamrad/piaacmc[0].pixscale;
  printf("SIZE = %ld, beamrad = %f pix, sep = %f m\n", size, beamradpix, piaacmc[0].piaasep);
  fflush(stdout);
  
 

  r0array = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  z0array = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  r1array = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);
  z1array = (double*) malloc(sizeof(double)*piaacmc[0].NBradpts);

  fp = fopen(fname, "r");
  for(k=0;k<piaacmc[0].NBradpts;k++)
    ret = fscanf(fp,"%lf %lf %lf %lf\n", &r0array[k], &z0array[k], &r1array[k], &z1array[k]);
  fclose(fp);

  //  for(k=0;k<nbpt;k++)
  //  printf("%ld %.8lf %.8lf %.8lf %.8lf\n", k, r0array[k], z0array[k], r1array[k], z1array[k]);
   

  for(k=0;k<piaacmc[0].NBradpts;k++)
    z1array[k] -= piaacmc[0].piaasep;
  



  ID_PIAAM0 = create_2Dimage_ID(ID_PIAAM0_name, size, size);
  ID_PIAAM1 = create_2Dimage_ID(ID_PIAAM1_name, size, size);
  
  printf("\n\n");
  for(ii=0;ii<size;ii++)
    {
      printf("\r %ld / %ld     ", ii, size);
      fflush(stdout);
      for(jj=0;jj<size;jj++)
	{
	  x = (1.0*ii-0.5*size)/beamradpix;
	  y = (1.0*jj-0.5*size)/beamradpix;
	  r = sqrt(x*x+y*y)*piaacmc[0].beamrad;

	  if(r<piaacmc[0].r0lim*piaacmc[0].beamrad)
	    {
	      k = 1;
	      while((r0array[k]<r)&&(k<piaacmc[0].NBradpts-2))
		k++;
	      r00 = r0array[k-1];
	      r01 = r0array[k];
	      alpha = (r-r00)/(r01-r00);
	      if(alpha>1.0)
		alpha = 1.0;
	      val = (1.0-alpha)*z0array[k-1] + alpha*z0array[k];
	      data.image[ID_PIAAM0].array.F[jj*size+ii] = val;
	    }
	  else
	    data.image[ID_PIAAM0].array.F[jj*size+ii] = 0.0;

	  if(r<piaacmc[0].r1lim*piaacmc[0].beamrad)
	    {
	      k = 1;
	      while((r1array[k]<r)&&(k<piaacmc[0].NBradpts-2))
		k++;
	      r00 = r1array[k-1];
	      r01 = r1array[k];
	      alpha = (r-r00)/(r01-r00);
	      if(alpha>1.0)
		alpha = 1.0;
	      val = (1.0-alpha)*z1array[k-1] + alpha*z1array[k];
	      data.image[ID_PIAAM1].array.F[jj*size+ii] = -val;//-piaacmc[0].piaasep);	
	    }
	  else
	    data.image[ID_PIAAM1].array.F[jj*size+ii] = 0.0;
	}
    }
  printf("\n\n");

  free(r0array);
  free(z0array);
  free(r1array);
  free(z1array);

  return 0;
}






//
// Detailed simulation of PIAACMC
// 
// 
//
// 


// transmits between rin and rout
long PIAAsimul_mkSimpleLyotStop(char *ID_name, float rin, float rout)
{
  long size;
  long size2;
  long ii, k;
  long ID;
  float r;


  size = piaacmc[0].size;
  size2 = size*size;

  ID = create_3Dimage_ID(ID_name, size, size, piaacmc[0].nblambda);      
  for(k=0;k<piaacmc[0].nblambda;k++)
    for(ii=0;ii<size2;ii++)
      {
	if((data.image[IDr].array.F[ii]<rout)&&(data.image[IDr].array.F[ii]>rin))
	  data.image[ID].array.F[k*size2+ii] = 1.0;
	else
	  data.image[ID].array.F[k*size2+ii] = 0.0;
      }   

  return(ID);
}




int PIAAsimul_initpiaacmc()
{
  float beamradpix;
  long NBpiaacmcdesign = 1;
  long ii, jj, k, i;
  double x, y;
  long size, size0;
  long Cmsize;
  long Fmsize;
  long ID, ID0, ID1;
  long size2;
  int r;
  char command[500];
  long IDv1, IDv2;
  char fname[200];
  char name[200];

  if(piaacmc == NULL)
    piaacmc = (MIRRORPIAACMCDESIGN*) malloc(sizeof(MIRRORPIAACMCDESIGN)*NBpiaacmcdesign);


  // Default Values for PIAACMC (will adopt them unless configuration file exists)

  piaacmc[0].nblambda = NBLAMBDA;

  piaacmc[0].beamrad = 0.01; // 20mm diam
  piaacmc[0].size = 1024;
  piaacmc[0].pixscale = 0.00005;
  piaacmc[0].piaasep = 0.5; // [m]
  piaacmc[0].fpzfactor = 8.0;
  piaacmc[0].Fratio = 30.0;

  piaacmc[0].centObs0 = 0.3; // input central obstruction
  piaacmc[0].centObs1 = 0.2; // output central obstruction
  piaacmc[0].NBradpts = 50000;
  piaacmc[0].r0lim = 1.1425; // outer radius after extrapolation, piaa mirror 0
  piaacmc[0].r1lim = 2.0; // outer radius after extrapolation, piaa mirror 1

  piaacmc[0].NBLyotStop = 2;
  for(i=0;i<10;i++)
    {
      piaacmc[0].LyotStop_zpos[i] = 0.0;
      piaacmc[0].IDLyotStop[i] = -1;
    }

  piaacmc[0].fpmaskradld = 1.2;
  piaacmc[0].fpmarraysize = 2048;
  piaacmc[0].fpmRad = 30.0e-6; // focal plane radius [m]
  piaacmc[0].NBrings = 4; // number of rings in focal plane mask
  piaacmc[0].fpmmaterial = 4;  // PMMA
  piaacmc[0].fpmaskamptransm = 1.0;

  piaacmc[0].CmodesID = -1; // Cosine radial mode
  piaacmc[0].FmodesID = -1; // Fourier 2D modes
  piaacmc[0].piaa0CmodesID = -1;
  piaacmc[0].piaa0FmodesID = -1;
  piaacmc[0].piaa1CmodesID = -1;
  piaacmc[0].piaa1FmodesID = -1;
  piaacmc[0].zonezID = -1;  // focm zone material thickness, double precision image
  piaacmc[0].zoneaID = -1;  // focm zone amplitude transmission, double precision image




  printf("Loading PIAACMC configuration\n");
  fflush(stdout);
  sprintf(command, "mkdir -p %s", piaacmcconfdir);
  r = system(command);
  r = PIAAsimul_loadpiaacmcconf(piaacmcconfdir);
  if(r==0)
    {
      printf("Saving default configuration\n");
      fflush(stdout);
      PIAAsimul_savepiaacmcconf(piaacmcconfdir);      
    }


  




  // create modes for aspheric optical surfaces description
  beamradpix = piaacmc[0].beamrad/piaacmc[0].pixscale;
  size = piaacmc[0].size;
  
  // x, y, r and PA coordinates in beam (for convenience & speed)
  IDx = create_2Dimage_ID("xcoord", size, size);
  IDy = create_2Dimage_ID("ycoord", size, size);
  IDr = create_2Dimage_ID("rcoord", size, size);
  IDPA = create_2Dimage_ID("PAcoord", size, size);
  for(ii=0; ii<size; ii++)
    for(jj=0; jj<size; jj++)
      {
	x = (1.0*ii-0.5*size)/beamradpix;
	y = (1.0*jj-0.5*size)/beamradpix;
	data.image[IDx].array.F[jj*size+ii] = x;
	data.image[IDy].array.F[jj*size+ii] = y;
	data.image[IDr].array.F[jj*size+ii] = sqrt(x*x+y*y);
	data.image[IDPA].array.F[jj*size+ii] = atan2(y,x);	
      }
  //  save_fl_fits("xcoord", "!xcoord.fits");
  //  save_fl_fits("ycoord", "!ycoord.fits");
  // save_fl_fits("rcoord", "!rcoord.fits");
  // save_fl_fits("PAcoord", "!PAcoord.fits");





  // ==================== CREATE MODES USED TO FIT AND DESCRIBE PIAA SHAPES ===============
  sprintf(fname, "%s/Cmodes.fits", piaacmcconfdir);
  piaacmc[0].CmodesID = image_ID("Cmodes");
  if(piaacmc[0].CmodesID==-1)
    piaacmc[0].CmodesID = load_fits(fname, "Cmodes");
  if(piaacmc[0].CmodesID==-1)
    {
      Cmsize = (long) (beamradpix*4);
      piaacmc[0].Cmsize = Cmsize;
      linopt_imtools_makeCosRadModes("Cmodes", Cmsize, 40, ApoFitCosFact*beamradpix, 2.0);
      piaacmc[0].CmodesID = image_ID("Cmodes");
      save_fits("Cmodes", fname);
    }
  piaacmc[0].NBCmodes = data.image[piaacmc[0].CmodesID].md[0].size[2];
  piaacmc[0].Cmsize = data.image[piaacmc[0].CmodesID].md[0].size[0];

  
  sprintf(fname, "%s/Fmodes.fits", piaacmcconfdir);
  piaacmc[0].FmodesID = image_ID("Fmodes");
  if(piaacmc[0].FmodesID==-1)
    piaacmc[0].FmodesID = load_fits(fname, "Fmodes");
  if(piaacmc[0].FmodesID==-1)
    {
      Fmsize = (long) (beamradpix*4);
      piaacmc[0].Fmsize = Fmsize;
      linopt_imtools_makeCPAmodes("Fmodes",  Fmsize, 5.0, 0.8, beamradpix, 2.0, 1);
      piaacmc[0].FmodesID = image_ID("Fmodes");
      save_fits("Fmodes", fname);
    }
  piaacmc[0].NBFmodes = data.image[piaacmc[0].FmodesID].md[0].size[2];      
  piaacmc[0].Fmsize = data.image[piaacmc[0].FmodesID].md[0].size[0];      
  



 
  // =================== IMPORT / CREATE PIAA SHAPES =====================

 
  piaacmc[0].piaa0CmodesID = image_ID("piaa0Cmodescoeff");
  piaacmc[0].piaa0FmodesID = image_ID("piaa0Fmodescoeff");
  piaacmc[0].piaa1CmodesID = image_ID("piaa1Cmodescoeff");
  piaacmc[0].piaa1FmodesID = image_ID("piaa1Fmodescoeff");
 
  if((piaacmc[0].piaa0CmodesID==-1)||( piaacmc[0].piaa0FmodesID==-1)||(piaacmc[0].piaa1CmodesID==-1)||( piaacmc[0].piaa1FmodesID==-1))
    {
      sprintf(fname, "%s/apo2Drad.fits", piaacmcconfdir);
      if(load_fits(fname, "apo2Drad")==-1)
	{
	  printf("Creating 2D apodization for idealized circular monochromatic PIAACMC\n");
	  fflush(stdout);
	  
	  // first iteration: half size image, 2x zoom
	  IDv1 = create_variable_ID("DFTZFACTOR", 2);
	  IDv2 = create_variable_ID("PNBITER", 20);	  
	  coronagraph_make_2Dprolateld(piaacmc[0].fpmaskradld, beamradpix*0.5, piaacmc[0].centObs1, "apotmp1", size/2);
	  
	  // expand solution to full size
	  basic_resizeim("apotmp1", "apostart", size, size);
	  delete_image_ID("apotmp1");

	  // full size, 4x zoom
	  IDv1 = create_variable_ID("DFTZFACTOR", 4);
	  IDv2 = create_variable_ID("PNBITER", 10);	  
	  coronagraph_make_2Dprolateld(piaacmc[0].fpmaskradld, beamradpix, piaacmc[0].centObs1, "apo", size);
	  chname_image_ID("apo", "apostart");
	  
	  // full size, 8x zoom
	  IDv1 = create_variable_ID("DFTZFACTOR", 8);
	  IDv2 = create_variable_ID("PNBITER", 5);	  
	  coronagraph_make_2Dprolateld(piaacmc[0].fpmaskradld, beamradpix, piaacmc[0].centObs1, "apo2Drad", size);

	  save_fits("apo2Drad", fname);	  	  
	}

      // load apodization profile and fit it a series of cosines
      PIAACMCsimul_load2DRadialApodization("apo2Drad", beamradpix, 0.3, "outApofit");                  

      // compute radial PIAA sag 
      PIAACMCsimul_init_geomPIAA_rad("outApofit");
      
      // make 2D sag maps
      sprintf(fname, "%s/PIAA_Mshapes.txt", piaacmcconfdir);
      PIAACMCsimul_mkPIAAMshapes_from_RadSag(fname, "piaam0z", "piaam1z");

      sprintf(fname, "!%s/piaam0z.fits", piaacmcconfdir);
      save_fits("piaam0z", fname);

      sprintf(fname, "!%s/piaam1z.fits", piaacmcconfdir);
      save_fits("piaam1z", fname);



     // crop piaam0z and piaam1z to Cmodes size
      ID0 = image_ID("Cmodes");
      size0 = data.image[ID0].md[0].size[0];
      ID1 = create_2Dimage_ID("piaa0zcrop", size0, size0);
      ID = image_ID("piaam0z");
      for(ii=0;ii<size0;ii++)
	for(jj=0;jj<size0;jj++)
	  data.image[ID1].array.F[jj*size0+ii] = data.image[ID].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)];
      ID1 = create_2Dimage_ID("piaa1zcrop", size0, size0);
      ID = image_ID("piaam1z");
      for(ii=0;ii<size0;ii++)
	for(jj=0;jj<size0;jj++)
	  data.image[ID1].array.F[jj*size0+ii] = data.image[ID].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)];

      make_disk("maskd", size0, size0, 0.5*size0, 0.5*size0, beamradpix);      
      make_2Dgridpix("gridpix", size0, size0, 1, 1, 0, 0);
      arith_image_mult("maskd", "gridpix", "maskfit");
      save_fits("maskfit", "!maskfit.fits");

      printf("--------- FITTING COSINE MODES ---------\n");
      fflush(stdout);

      linopt_imtools_image_fitModes("piaa0zcrop", "Cmodes", "maskfit", 1.0e-6, "piaa0Cmodescoeff", 0);
      save_fits("piaa0Cmodescoeff", "!piaa0Cmodescoeff.fits");
      linopt_imtools_image_fitModes("piaa1zcrop", "Cmodes", "maskfit", 1.0e-6, "piaa1Cmodescoeff", 0);
      save_fits("piaa1Cmodescoeff", "!piaa1Cmodescoeff.fits");

      linopt_imtools_image_construct("Cmodes", "piaa0Cmodescoeff", "piaa0Cz");
      save_fits("piaa0Cz", "!piaa0Cz.fits");
      linopt_imtools_image_construct("Cmodes", "piaa1Cmodescoeff", "piaa1Cz");
      save_fits("piaa1Cz", "!piaa1Cz.fits");

      ID0 = image_ID("piaa0Cz");
      size0 = data.image[ID0].md[0].size[0];
      ID1 = image_ID("piaam0z");
      ID = create_2Dimage_ID("piaa0Cres", size0, size0);
      for(ii=0;ii<size0;ii++)
	for(jj=0;jj<size0;jj++)
	  data.image[ID].array.F[jj*size0+ii] = data.image[ID1].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)]-data.image[ID0].array.F[jj*size0+ii];
      save_fits("piaa0Cres", "!piaa0Cres.fits");

      ID0 = image_ID("piaa1Cz");
      size0 = data.image[ID0].md[0].size[0];
      ID1 = image_ID("piaam1z");
      ID = create_2Dimage_ID("piaa1Cres", size0, size0);
      for(ii=0;ii<size0;ii++)
	for(jj=0;jj<size0;jj++)
	  data.image[ID].array.F[jj*size0+ii] = data.image[ID1].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)]-data.image[ID0].array.F[jj*size0+ii];
      save_fits("piaa1Cres", "!piaa1Cres.fits");
       



      printf("--------- FITTING FOURIER MODES ---------\n");
      fflush(stdout);

      linopt_imtools_image_fitModes("piaa0Cres", "Fmodes", "maskfit", 0.01, "piaa0Fmodescoeff", 0);
      //      save_fits("piaa0Fmodescoeff", "!piaa0Fmodescoeff.fits");
      linopt_imtools_image_fitModes("piaa1Cres", "Fmodes", "maskfit", 0.01, "piaa1Fmodescoeff", 0);
      // save_fits("piaa1Fmodescoeff", "!piaa1Fmodescoeff.fits");
      
      //linopt_imtools_image_construct("Fmodes", "piaa0Fmodescoeff", "piaa0Fz");
      //   save_fits("piaa0Fz", "!piaa0Fz.fits");
      //arith_image_sub("piaa0Cres", "piaa0Fz", "piaa0CFres");
      //save_fits("piaa0CFres", "!piaa0CFres.fits");
      delete_image_ID("piaa0zcrop");

      //linopt_imtools_image_construct("Fmodes", "piaa1Fmodescoeff", "piaa1Fz");
      //save_fits("piaa1Fz", "!piaa1Fz.fits");
      //arith_image_sub("piaa1Cres", "piaa1Fz", "piaa1CFres");
      //save_fits("piaa1CFres", "!piaa1CFres.fits");
      delete_image_ID("piaa1zcrop");

      delete_image_ID("maskfit");



      piaacmc[0].piaa0CmodesID = image_ID("piaa0Cmodescoeff");
      piaacmc[0].piaa0FmodesID = image_ID("piaa0Fmodescoeff");
      piaacmc[0].piaa1CmodesID = image_ID("piaa1Cmodescoeff");
      piaacmc[0].piaa1FmodesID = image_ID("piaa1Fmodescoeff");


      sprintf(fname, "!%s/piaa0Cmodes.fits", piaacmcconfdir);
      save_fits(data.image[piaacmc[0].piaa0CmodesID].md[0].name, fname);

      sprintf(fname, "!%s/piaa0Fmodes.fits", piaacmcconfdir);
      save_fits(data.image[piaacmc[0].piaa0FmodesID].md[0].name, fname);

      sprintf(fname, "!%s/piaa1Cmodes.fits", piaacmcconfdir);
      save_fits(data.image[piaacmc[0].piaa1CmodesID].md[0].name, fname);

      sprintf(fname, "!%s/piaa1Fmodes.fits", piaacmcconfdir);
      save_fits(data.image[piaacmc[0].piaa1FmodesID].md[0].name, fname);
    }




  // ============ MAKE FOCAL PLANE MASK ===============


  if(image_ID("fpmzmap")==-1)
    {
      PIAACMCsimul_mkFPM_zonemap("fpmzmap");
      sprintf(fname, "!%s/fpmzmap.fits", piaacmcconfdir);
      save_fits("fpmzmap", fname);
    }

  piaacmc[0].zonezID = image_ID("fpmzt");
  if(piaacmc[0].zonezID == -1)
    {
      piaacmc[0].zonezID = create_2Dimagedouble_ID("fpmzt", piaacmc[0].focmNBzone, 1);
      for(ii=0;ii<piaacmc[0].focmNBzone;ii++)
	data.image[piaacmc[0].zonezID].array.D[ii] = 1.0e-6;
      sprintf(fname, "!%s/fpmzt.fits", piaacmcconfdir);
      save_fits("fpmzt", fname);
    }

  piaacmc[0].zoneaID = image_ID("fpmza");
  if(piaacmc[0].zoneaID == -1)
    {
      piaacmc[0].zoneaID = create_2Dimagedouble_ID("fpmza", piaacmc[0].focmNBzone, 1);
      for(ii=0;ii<piaacmc[0].focmNBzone;ii++)
	data.image[piaacmc[0].zoneaID].array.D[ii] = piaacmc[0].fpmaskamptransm;          
      sprintf(fname, "!%s/fpmza.fits", piaacmcconfdir);
      save_fits("fpmza", fname);
    }





  // ============= MAKE LYOT STOPS =======================
  printf("LOADING/CREATING LYOT MASK\n");
  size2 = size*size;

  for(i=0;i<piaacmc[0].NBLyotStop;i++)
    {      
      sprintf(fname, "%s/LyotStop%ld.fits", piaacmcconfdir, i);
      sprintf(name, "lyotstop%ld", i);
     
      piaacmc[0].IDLyotStop[i] = image_ID(name);	      
      if(piaacmc[0].IDLyotStop[i]==-1)
	{
	  sprintf(fname, "!%s/LyotStop%ld.fits", piaacmcconfdir, i);
	  ID = PIAAsimul_mkSimpleLyotStop(name, piaacmc[0].centObs1+0.02, 0.98);
	  save_fl_fits(name, fname);      
	}
    }
  return(0);
}





int PIAACMCsimul_makePIAAshapes()
{
  long ID, ID0, ID1;
  long size, size0, size1;
  long ii, jj;

  size = piaacmc[0].size;

  // ============ construct PIAA shapes from fitting coefficients ==================

  // assemble piaa0z and piaa1z images
  ID0 = linopt_imtools_image_construct("Cmodes", "piaa0Cmodescoeff", "piaa0Cz");
  ID1 = linopt_imtools_image_construct("Fmodes", "piaa0Fmodescoeff", "piaa0Fz");
  ID = image_ID("piaa0z");
  if(ID==-1)
    ID = create_2Dimage_ID("piaa0z", size, size);
  size0 = data.image[ID0].md[0].size[0];
  size1 = data.image[ID1].md[0].size[0];
  for(ii=0;ii<size*size;ii++)
    data.image[ID].array.F[ii] = 0.0;
  for(ii=0;ii<size0;ii++)
    for(jj=0;jj<size0;jj++)
      data.image[ID].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)] += data.image[ID0].array.F[jj*size0+ii];
  for(ii=0;ii<size1;ii++)
    for(jj=0;jj<size1;jj++)
      data.image[ID].array.F[(jj+(size-size1)/2)*size+(ii+(size-size1)/2)] += data.image[ID1].array.F[jj*size1+ii];
  save_fits("piaa0Cz", "!piaa0Cz.fits");
  save_fits("piaa0Fz", "!piaa0Fz.fits");
  save_fits("piaa0z", "!piaa0z.fits");
  delete_image_ID("piaa0Cz");
  delete_image_ID("piaa0Fz");



  ID0 = linopt_imtools_image_construct("Cmodes", "piaa1Cmodescoeff", "piaa1Cz");
  ID1 = linopt_imtools_image_construct("Fmodes", "piaa1Fmodescoeff", "piaa1Fz");
  ID = image_ID("piaa1z");
  if(ID==-1)
    ID = create_2Dimage_ID("piaa1z", size, size);
  for(ii=0;ii<size*size;ii++)
    data.image[ID].array.F[ii] = 0.0;
  size0 = data.image[ID0].md[0].size[0];
  size1 = data.image[ID1].md[0].size[0];
  for(ii=0;ii<size0;ii++)
    for(jj=0;jj<size0;jj++)
      data.image[ID].array.F[(jj+(size-size0)/2)*size+(ii+(size-size0)/2)] += data.image[ID0].array.F[jj*size0+ii];
  for(ii=0;ii<size1;ii++)
    for(jj=0;jj<size1;jj++)
      data.image[ID].array.F[(jj+(size-size1)/2)*size+(ii+(size-size1)/2)] += data.image[ID1].array.F[jj*size1+ii];
  save_fits("piaa1Cz", "!piaa1Cz.fits");
  save_fits("piaa1Fz", "!piaa1Fz.fits");
  save_fits("piaa1z", "!piaa1z.fits");
  delete_image_ID("piaa1Cz");
  delete_image_ID("piaa1Fz");

  return 0;
}





double PIAACMCsimul_computePSF(float xld, float yld)
{
  double x, y;
  long IDa, IDp;
  long size;
  long nblambda;
  long size2;
  long ii, jj, k;
  long IDpiaa1z, IDpiaa2z;
  long elem;
  long kl;

  char fname_piaa1z[200];
  char fname_piaa2z[200];
  char fname_pupa0[200];
  char fname_pupp0[200];
  char fname[200];

  long ID;
  long index;

  double proplim = 1.0e-4;
  double total;

  long size0, size1;
  long Cmsize, Fmsize;


  // how to measure quality
  float focscale; // l/D per pix
  float scoringIWA = 1.5;
  float scoringOWA = 8.0;
  float scoringIWAx = 0.5;
  long IDsm;
  float r;

  double value;



  printf("PIAACMC system simulation\n");

  size = piaacmc[0].size;
  size2 = size*size;


  focscale = (2.0*piaacmc[0].beamrad/piaacmc[0].pixscale)/piaacmc[0].size;


  // ========== initializes optical system to piaacmc design ===========
  PIAACMCsimul_init(piaacmc, 0, xld, yld);
  PIAACMCsimul_makePIAAshapes();



  // ============ perform propagations ================
  OptSystProp_run(optsyst, 0, 0, optsyst[0].NBelem);
  
  
  printf("FOCAL PLANE SCALE = %f l/d per pix\n", focscale);
  



  // CREATE SCORING MASK
  printf("FOCAL PLANE SCALE = %f l/d per pix\n", focscale);
  IDsm = create_2Dimage_ID("scoringmask", size, size);
  for(ii=0;ii<size;ii++)
    for(jj=0;jj<size;jj++)
      {
	x = (1.0*ii-0.5*size)*focscale;
	y = (1.0*jj-0.5*size)*focscale;
	r = sqrt(x*x+y*y);
	if((r>scoringIWA)&&(r<scoringOWA)&&(x>scoringIWAx))
	  data.image[IDsm].array.F[jj*size+ii] = 1.0;
	if((x>scoringIWA)&&(fabs(y)<scoringIWA*0.5)&&(r<50.0))
	  data.image[IDsm].array.F[jj*size+ii] = 1.0;
      }
  save_fits("scoringmask","!scoringmask.fits");
  

  linopt_imtools_mask_to_pixtable("scoringmask", "pixindex", "pixmult");

  linopt_imtools_Image_to_vec("psfc", "pixindex", "pixmult", "imvect");
 
  save_fits("imvect", "!imvect.fits");
  
 
  value = 0.0;
  ID = image_ID("imvect");
  for(ii=0;ii<data.image[ID].md[0].nelement;ii++)
    value += data.image[ID].array.F[ii]*data.image[ID].array.F[ii];


  for(elem=0;elem<optsyst[0].NBelem;elem++)
    printf("    FLUX %3ld   %12.4lf %8.6lf\n", elem, optsyst[0].flux[elem], optsyst[0].flux[elem]/optsyst[0].flux[0]);
  value = value/size/size/optsyst[0].flux[0];

  printf("Average contrast = %g\n", value/(arith_image_total("scoringmask")*focscale*focscale));

  return(value);
}




int PIAAsimul_savepiaacmcconf(char *dname)
{
  char command[200];
  int r;
  FILE *fp;
  char fname[200];
  long i;


  sprintf(command, "mkdir -p %s", dname);
  r = system(command);
  
  sprintf(fname,"%s/piaacmcparams.conf", dname);
  fp = fopen(fname, "w");

  
  fprintf(fp, "%10.6f   beamrad\n", piaacmc[0].beamrad);
  fprintf(fp, "%10ld    size\n", piaacmc[0].size);
  fprintf(fp, "%10.6g   pixscale\n", piaacmc[0].pixscale);
  fprintf(fp, "%10.6f   piaasep\n", piaacmc[0].piaasep);

  fprintf(fp, "%10.6f   centObs0\n", piaacmc[0].centObs0);
  fprintf(fp, "%10.6f   centObs1\n", piaacmc[0].centObs1);
  fprintf(fp, "%10ld   NBradpts\n", piaacmc[0].NBradpts);
  fprintf(fp, "%10.6f   r0lim\n", piaacmc[0].r0lim);
  fprintf(fp, "%10.6f   r1lim\n", piaacmc[0].r1lim);


  

  fprintf(fp, "%10ld    NBLyotStop\n", piaacmc[0].NBLyotStop);
  for(i=0;i<10;i++)
    {
      if(i<piaacmc[0].NBLyotStop)
	{
	  sprintf(fname, "!%s/LyotStop%ld.fits", dname, i);
	  save_fits(data.image[piaacmc[0].IDLyotStop[i]].md[0].name, fname);
	  fprintf(fp, "%10.6lf   LyotStop_zpos %ld\n", piaacmc[0].LyotStop_zpos[i], i);
	}
      else
	fprintf(fp, "%10.6lf   LyotStop_zpos %ld\n", piaacmc[0].LyotStop_zpos[i], i);
    }


  sprintf(fname, "!%s/piaa0Cmodes.fits", dname);
  save_fits(data.image[piaacmc[0].piaa0CmodesID].md[0].name, fname);
  sprintf(fname, "!%s/piaa0Fmodes.fits", dname);
  save_fits(data.image[piaacmc[0].piaa0FmodesID].md[0].name, fname);
  sprintf(fname, "!%s/piaa1Cmodes.fits", dname);
  save_fits(data.image[piaacmc[0].piaa1CmodesID].md[0].name, fname);
  sprintf(fname, "!%s/piaa1Fmodes.fits", dname);
  save_fits(data.image[piaacmc[0].piaa1FmodesID].md[0].name, fname);


  fprintf(fp, "%10.6f    fpmaskradld\n", piaacmc[0].fpmaskradld);
  fprintf(fp, "%10ld    focmNBzone\n", piaacmc[0].focmNBzone);
  fprintf(fp, "%10.6f   Fratio\n", piaacmc[0].Fratio);
  sprintf(fname, "!%s/fpm_zonez.fits", dname);
  save_fits(data.image[piaacmc[0].zonezID].md[0].name, fname);
  fprintf(fp, "%10.6f    fpmaskamptransm\n", piaacmc[0].fpmaskamptransm);
  sprintf(fname, "!%s/fpm_zonea.fits", dname);
  save_fits(data.image[piaacmc[0].zoneaID].md[0].name, fname);

  fprintf(fp, "%10.6f   fpzfactor\n", piaacmc[0].fpzfactor);
  fprintf(fp, "%10.6g   fpmRad\n", piaacmc[0].fpmRad);
  fprintf(fp, "%10ld    NBrings\n", piaacmc[0].NBrings);
  fprintf(fp, "%10ld    fpmarraysize \n", piaacmc[0].fpmarraysize);
  fprintf(fp, "%10d    fpmmaterial\n", piaacmc[0].fpmmaterial);
  
  fclose(fp);
  


  
  return(0);
}




int PIAAsimul_loadpiaacmcconf(char *dname)
{
  char command[200];
  int r;
  FILE *fp;
  char fname[200];
  char imname[200];
  long i;
  
  int tmpi;
  long tmpl;
  float tmpf;
  double tmplf;

  

  sprintf(fname,"%s/piaacmcparams.conf", dname);

  fp = fopen(fname, "r");
  if(fp==NULL)
    {
      printf("Configuration file \"%s\" does not exist (yet), using previously set configuration\n", fname);
      fflush(stdout);
      r = 0;
    }
  else
    {
      r = fscanf(fp, "%f   beamrad\n", &tmpf);
      piaacmc[0].beamrad = tmpf;
      
      r = fscanf(fp, "%ld    size\n", &tmpl);
      piaacmc[0].size = tmpl;
      
      r = fscanf(fp, "%g   pixscale\n", &tmpf);
      piaacmc[0].pixscale = tmpf;
      
      r = fscanf(fp, "%f   piaasep\n", &tmpf);
      piaacmc[0].piaasep = tmpf;
      
      r = fscanf(fp, "%lf   centObs0\n", &tmplf);
      piaacmc[0].centObs0 = tmplf;
      
      r = fscanf(fp, "%lf   centObs1\n", &tmplf);
      piaacmc[0].centObs1 = tmplf;
      
      r = fscanf(fp, "%ld   NBradpts\n", &tmpl);
      piaacmc[0].NBradpts = tmpl;
      
      r = fscanf(fp, "%lf   r0lim\n", &tmplf);
      piaacmc[0].r0lim = tmplf;
      
      r = fscanf(fp, "%lf   r1lim\n", &tmplf);
      piaacmc[0].r1lim = tmplf;
      
      
      
      
      r = fscanf(fp, "%10ld    NBLyotStop\n", &piaacmc[0].NBLyotStop);
      for(i=0;i<10;i++)
	{
	  if(i<piaacmc[0].NBLyotStop)
	    {
	      sprintf(fname, "%s/LyotStop%ld.fits", dname, i);
	      sprintf(imname, "lyotstop%ld", i);
	      piaacmc[0].IDLyotStop[i] = load_fits(fname, imname);
	      r = fscanf(fp, "%lf   LyotStop_zpos %ld\n", &tmplf, &tmpl);
	      piaacmc[0].LyotStop_zpos[i] = tmplf;
	    }
	  else
	    {
	      r = fscanf(fp, "%lf   LyotStop_zpos %ld\n", &tmplf, &tmpl);
	      piaacmc[0].LyotStop_zpos[i] = tmplf;
	      printf("LYOT STOP %ld POS : %lf\n", i, tmplf);
	    }
	}
      
      
      sprintf(fname, "%s/piaa0Cmodes.fits", dname);
      piaacmc[0].piaa0CmodesID = load_fits(fname, "piaa0Cmodescoeff");
      
      sprintf(fname, "%s/piaa0Fmodes.fits", dname);
      piaacmc[0].piaa0FmodesID = load_fits(fname, "piaa0Fmodescoeff");
      
      sprintf(fname, "%s/piaa1Cmodes.fits", dname);
      piaacmc[0].piaa1CmodesID = load_fits(fname, "piaa1Cmodescoeff");
      
      sprintf(fname, "%s/piaa1Fmodes.fits", dname);
      piaacmc[0].piaa1FmodesID = load_fits(fname, "piaa1Fmodescoeff");
      



      r = fscanf(fp, "%f    fpmaskradld\n", &tmpf);
      piaacmc[0].fpmaskradld = tmpf;

      r = fscanf(fp, "%ld    focmNBzone\n",  &tmpl);
      piaacmc[0].focmNBzone = tmpl;
      
      r = fscanf(fp, "%f   Fratio\n",      &tmpf);
      piaacmc[0].Fratio = tmpf;
      
      sprintf(fname, "%s/fpm_zonez.fits", dname);
      delete_image_ID("fpmzt");
      piaacmc[0].zonezID = load_fits(fname, "fpmzt");

      r = fscanf(fp, "%f   fpmaskamptransm\n",    &tmpf);
      piaacmc[0].fpmaskamptransm = tmpf;

      sprintf(fname, "%s/fpm_zonea.fits", dname);
      delete_image_ID("fpmza");
      piaacmc[0].zoneaID = load_fits(fname, "fpmza");
      
      r = fscanf(fp, "%f   fpzfactor\n",   &tmpf);
      piaacmc[0].fpzfactor = tmpf;
      
      r = fscanf(fp, "%f   fpmRad\n",      &tmpf);
      piaacmc[0].fpmRad = tmpf;
      
      r = fscanf(fp, "%ld    NBrings\n",     &tmpl);
      piaacmc[0].NBrings = tmpl;
      
      r = fscanf(fp, "%ld    fpmarraysize \n", &tmpl);
      piaacmc[0].fpmarraysize = tmpl;
      
      r = fscanf(fp, "%d    fpmmaterial\n",  &tmpi);
      piaacmc[0].fpmmaterial = tmpi;
      
      r = 1;

      fclose(fp);
    }

  return(r);
}




long PIAACMCsimul_mkLyotMask(char *IDincoh_name, char *IDmc_name, char *IDzone_name, double throughput, char *IDout_name)
{
  long ID;
  long IDmc, IDincoh, IDzone;
  double val, val1;
  double rsl;
  long iter, NBiter;
  long ii;
  long xsize, ysize;
  long IDout;
  float sigma = 6.0;
  int filter_size = 10;

  NBiter = 10;

  
  printf("IDincoh_name : %s   %ld\n", IDincoh_name, image_ID(IDincoh_name));
  printf("IDmc_name    : %s   %ld\n", IDmc_name, image_ID(IDmc_name));
  printf("IDzone_name  : %s   %ld\n", IDzone_name, image_ID(IDzone_name));

  IDincoh = gauss_filter(IDincoh_name, "incohg", sigma, filter_size);
  IDmc = gauss_filter(IDmc_name, "mcg", sigma, filter_size);
  
  printf("STEP 0\n");
  fflush(stdout);


  //  IDincoh = image_ID(IDincoh_name);
  // IDmc = image_ID(IDmc_name);
  IDzone = image_ID(IDzone_name);
  xsize = data.image[IDmc].md[0].size[0];
  ysize = data.image[IDmc].md[0].size[1];

  IDout = create_2Dimage_ID(IDout_name, xsize, ysize);

  printf("STEP 1\n");
  fflush(stdout);

  // normalize both images to 1.0
  val = 0.0;
  for(ii=0;ii<xsize*ysize;ii++)
    val += data.image[IDmc].array.F[ii];
  for(ii=0;ii<xsize*ysize;ii++)
    data.image[IDmc].array.F[ii] /= val;

  val = 0.0;
  for(ii=0;ii<xsize*ysize;ii++)
    val += data.image[IDincoh].array.F[ii];
  for(ii=0;ii<xsize*ysize;ii++)
    data.image[IDincoh].array.F[ii] /= val;

  printf("STEP 1\n");
  fflush(stdout);


  rsl = 1.0;
  for(iter=0;iter<NBiter;iter++)
    {
      val = 0.0;
      val1 = 0.0;
      
      for(ii=0;ii<xsize*ysize;ii++)
	{
	  if((data.image[IDzone].array.F[ii]>-1)&&(data.image[IDincoh].array.F[ii]/data.image[IDmc].array.F[ii]>rsl))
	    {
	      val += data.image[IDincoh].array.F[ii];
	      val1 += data.image[IDmc].array.F[ii];
	      data.image[IDout].array.F[ii] = 1.0;
	    }
	  else
	    data.image[IDout].array.F[ii] = 0.0;
	}
      printf("rsl = %f  ->  %f %f\n", rsl, val, val1);
      if(val>throughput) // too much light came through
	rsl *= 1.1;
      else
	rsl *= 0.9;
    }

  delete_image_ID("incohg");
  delete_image_ID("mcg");

  return(IDout);
}



//
// Lyot stops positions from zmin to zmax relative to current, working back (light goes from 0 to zmax)
// FluxTOT: total flux in current plane
// FluxLim: max flux allowed from star
// 
double PIAACMCsimul_optimizeLyotStop(char *IDamp_name, char *IDpha_name, char *IDincoh_name, float zmin, float zmax, double throughput, long NBz, long NBmasks)
{
  // initial guess places Lyot stops regularly from zmin to zmax
  // light propagates from zmin to zmax
  // we start with a single mask in zmax, and work back
  //

  double ratio;

  long ID, IDa, IDp;
  long nblambda; // number of wavelengths, read from input cube
  float *zarray;
  long l;
  double zprop;
  
  char nameamp[200];
  char namepha[200];
  char nameint[200];
  char fname[200];
  long xsize, ysize;
  long ii, jj, k, m;

  float *rinarray;
  float *routarray;
  float dr = 0.02;
  double tot;


  double *totarray;
  double *tot2array;
  long IDzone;
  double x, y, r;

  double zbest, valbest, val;
  long lbest;

  long IDincoh, IDint, IDmc;
  float rsl;
  long iter;
  long NBiter = 100;
  double val1;
  long IDm;
  char name[200];
  
  FILE *fp;


  zarray = (float*) malloc(sizeof(float)*NBz);

  rinarray = (float*) malloc(sizeof(float)*NBmasks);
  routarray = (float*) malloc(sizeof(float)*NBmasks);

  totarray = (double*) malloc(sizeof(double)*NBmasks*NBz);
  tot2array = (double*) malloc(sizeof(double)*NBmasks*NBz);


  routarray[0] = 1.0;
  rinarray[0] = 1.0 - 1.0/NBmasks;
  for(m=1;m<NBmasks;m++)
    {
      routarray[m] = rinarray[m-1];
      rinarray[m] = routarray[m] - 1.0/NBmasks;
    }
  rinarray[NBmasks-1] = 0.0;

  for(m=0;m<NBmasks;m++)
    printf("annulus %ld : %f - %f\n", m, routarray[m], rinarray[m]);


  IDa = image_ID(IDamp_name);
  IDp = image_ID(IDpha_name);
  IDincoh = image_ID(IDincoh_name);

  xsize = data.image[IDa].md[0].size[0];
  ysize = data.image[IDa].md[0].size[1];

  if(data.image[IDa].md[0].naxis==3)
    nblambda = data.image[IDa].md[0].size[2];
  else
    nblambda = 1;

  IDzone = create_2Dimage_ID("LMzonemap", xsize, ysize);
  for(ii=0;ii<xsize;ii++)
    for(jj=0;jj<ysize;jj++)
      {
	data.image[IDzone].array.F[jj*xsize+ii] = -2;
	x = (1.0*ii-0.5*xsize)/(piaacmc[0].beamrad/piaacmc[0].pixscale);
	y = (1.0*jj-0.5*xsize)/(piaacmc[0].beamrad/piaacmc[0].pixscale);
	r = sqrt(x*x+y*y);
	for(m=0;m<NBmasks;m++)
	  if((r>rinarray[m])&&(r<routarray[m]))
	    data.image[IDzone].array.F[jj*xsize+ii] = m;
      }
  save_fits("LMzonemap", "!LMzonemap.fits");
  // initialize zarray
  for(l=0;l<NBz;l++)
    zarray[l] = zmin + (zmax-zmin)*l/(NBz-1);
  
  save_fits(nameint, fname);
  
  for(l=0;l<NBz;l++)
    {
      sprintf(nameamp, "LMPamp%02ld", l);
      sprintf(namepha, "LMPpha%02ld", l);
      zprop = zarray[l];
      OptSystProp_propagateCube(optsyst, 0, IDamp_name, IDpha_name, nameamp, namepha, zprop);

      // collapse broadband intensities
      sprintf(nameint, "LMPint%02ld", l);
      IDa = image_ID(nameamp);
      ID = create_2Dimage_ID(nameint, xsize, ysize);
 
      
      for(m=0;m<NBmasks;m++)
	{
	  tot2array[l*NBmasks+m] = 0.0;
	  totarray[l*NBmasks+m] = 0.0;
	}

      for(ii=0;ii<xsize*ysize;ii++)	
	{
	  m = (long) (data.image[IDzone].array.F[ii]+0.1);
	  for(k=0;k<nblambda;k++)
	    {
	      data.image[ID].array.F[ii] += data.image[IDa].array.F[k*xsize*ysize+ii]*data.image[IDa].array.F[k*xsize*ysize+ii];      
	      if((m>-1)&&(m<NBmasks))
		{
		  totarray[l*NBmasks+m] += data.image[ID].array.F[ii];
		  tot2array[l*NBmasks+m] += data.image[ID].array.F[ii]*data.image[ID].array.F[ii];
		}
	    }
	}
      sprintf(fname, "!LMPint%02ld.fits", l);
      save_fits(nameint, fname);

      delete_image_ID(nameamp);
      delete_image_ID(namepha);
    }


  IDmc = create_2Dimage_ID("Lcomb", xsize, ysize);

  fp = fopen("LyotMasks_zpos.txt", "w");
  for(m=0;m<NBmasks;m++)
    {
      valbest = 0.0;
      lbest = 0;
      zbest = 0.0;
      for(l=0;l<NBz;l++)
	{
	  val =  tot2array[l*NBmasks+m]/(totarray[l*NBmasks+m]*totarray[l*NBmasks+m]);
	  printf("MASK %ld   z= %f  ->  %g   ( %g %g) \n", m, zarray[l], val, tot2array[l*NBmasks+m], totarray[l*NBmasks+m]);
	  if(val>valbest)
	    {
	      valbest = val;
	      zbest = zarray[l];
	      lbest = l;
	    }
	}
      printf("   BEST CONJUGATION : %ld %f\n", lbest, zbest);
      fprintf(fp, "%02ld %f\n", lbest, zbest);
      sprintf(nameint, "LMPint%02ld", lbest);
      IDint = image_ID(nameint);

      for(ii=0;ii<xsize*ysize;ii++)
	if(m==data.image[IDzone].array.F[ii])
	  data.image[IDmc].array.F[ii] = data.image[IDint].array.F[ii];
    }
  fclose(fp);

  save_fits("Lcomb", "!Lcomb.fits");
  
  ID = PIAACMCsimul_mkLyotMask(IDincoh_name, "Lcomb", "LMzonemap", throughput, "LMask");
  delete_image_ID("Lcomb");

  for(m=0;m<NBmasks;m++)
    {
      sprintf(name, "optLM%02ld", m);
      IDm = create_2Dimage_ID(name, xsize, ysize);
      for(ii=0;ii<xsize;ii++)
	for(jj=0;jj<ysize;jj++)
	  {
	    x = (1.0*ii-0.5*xsize)/(piaacmc[0].beamrad/piaacmc[0].pixscale);
	    y = (1.0*jj-0.5*xsize)/(piaacmc[0].beamrad/piaacmc[0].pixscale);
	    r = sqrt(x*x+y*y);
	    
	    if((r>rinarray[m]-dr)&&(r<routarray[m]+dr))
	      data.image[IDm].array.F[jj*xsize+ii] = data.image[ID].array.F[jj*xsize+ii];
	    else
	      data.image[IDm].array.F[jj*xsize+ii] = 1.0;
	  }
      sprintf(fname, "!optLM%02ld.fits", m);
      save_fits(name, fname);
    }

 
  free(totarray);
  free(tot2array);
  free(rinarray);
  free(routarray);
  free(zarray);
  
  delete_image_ID("LMzonemap");

  return(ratio);
}










int PIAACMCsimul_run(long confindex)
{
  long NBparam;

  int paramtype[1000]; // FLOAT or DOUBLE
  double *paramval[1000]; // array of pointers, double
  float *paramvalf[1000]; // array of pointers, float
  double paramrefval[1000]; 

  double paramdelta[1000]; 
  double parammaxstep[1000]; // maximum single iteration step
  double parammin[1000]; // minimum value
  double parammax[1000]; // maximum value

  double paramdeltaval[1000];
  double val, valref;
  long i, ii, jj;
  long ID, IDref, IDa;
  long IDmodes;
  long xsize, ysize;
  long k;

  double loopgain = 0.1;
  long iter;
  long NBiter = 1000;

  FILE *fp;

  long IDfpmresp, IDref1;
  double t, a, dpha, re, im, amp, pha;
  int zi;

  char fname[200];

  long IDm, ID1D, ID1Dref;
  long size1Dvec;

  
  // OPTIMIZATION PARAMETERS
  int REGPIAASHAPES = 0;
  float piaa0C_regcoeff = 0.0e-7; // regularization coeff
  float piaa1C_regcoeff = 0.0e-7; // regularization coeff
  
  float piaa0C_regcoeff_alpha = 1.0; // regularization coeff power
  float piaa1C_regcoeff_alpha = 1.0; // regularization coeff power

  int r;

  piaacmc = NULL;


  optsyst = (OPTSYST*) malloc(sizeof(OPTSYST));


  sprintf(piaacmcconfdir, "piaacmcconf%03ld", confindex);
  PIAAsimul_initpiaacmc();
  PIAACMCsimul_makePIAAshapes();
  

  //  PIAAsimul_savepiaacmcconf("piaacmc0");
  //  PIAAsimul_loadpiaacmcconf("piaacmc0");
  // PIAAsimul_savepiaacmcconf("piaacmc1");
  //exit(0);

  //piaacmc[0].nblambda = 1;

  // set up optimization parameters for linear optimization
  NBparam = 0;
  
  if(0) // Lyot mask #0 position
    {
      paramtype[NBparam] = DOUBLE;
      paramval[NBparam] = &piaacmc[0].LyotStop_zpos[0];
      paramdelta[NBparam] = 0.05;
      parammaxstep[NBparam] = 0.05;
      parammin[NBparam] = 0.0;
      parammax[NBparam] = 2.5;
      NBparam++;
    }

  if(0) // Lyot mask #1 position
    {
      paramtype[NBparam] = DOUBLE;
      paramval[NBparam] = &piaacmc[0].LyotStop_zpos[1];
      paramdelta[NBparam] = 0.05;
      parammaxstep[NBparam] = 0.05;
      parammin[NBparam] = -0.5;
      parammax[NBparam] = 0.5;
      NBparam++;
    }

  if(0) // Focal plane mask radius
    {
      paramtype[NBparam] = DOUBLE;
      paramval[NBparam] = &piaacmc[0].fpmRad;
      paramdelta[NBparam] = 1.0e-6;
      parammaxstep[NBparam] = 5.0e-6;
      parammin[NBparam] = 1.0e-6;
      parammax[NBparam] = 1.0e-4;
      NBparam++;
    }

  if(1) // Focal plane material thickness
    {
      for(k=0;k<data.image[piaacmc[0].zonezID].md[0].size[0];k++)
	{	  
	  paramtype[NBparam] = DOUBLE;
	  paramval[NBparam] = &data.image[piaacmc[0].zonezID].array.D[k];
	  paramdelta[NBparam] = 1.0e-9;
	  parammaxstep[NBparam] = 1.0e-7;
	  parammin[NBparam] = -1.0e-5;
	  parammax[NBparam] = 1.0e-5;
	  NBparam++;
	}
    }

  if(1) // Focal plane material transmission
    {
      for(k=0;k<data.image[piaacmc[0].zoneaID].md[0].size[0];k++)
	{	
	  paramtype[NBparam] = DOUBLE;
  	  paramval[NBparam] = &data.image[piaacmc[0].zoneaID].array.D[k];
	  paramdelta[NBparam] = 1.0e-4;
	  parammaxstep[NBparam] = 5.0e-2;
	  parammin[NBparam] = 1.0e-5;
	  parammax[NBparam] = 0.99;
	  NBparam++;
	}
    }

  
  if(1)  // PIAA shapes
    {
      for(k=0;k<data.image[piaacmc[0].piaa0CmodesID].md[0].size[0];k++)
	{
	  paramtype[NBparam] = FLOAT;
	  paramvalf[NBparam] = &data.image[piaacmc[0].piaa0CmodesID].array.F[k];
	  paramdelta[NBparam] = 1.0e-9;
	  parammaxstep[NBparam] = 1.0e-8;
	  parammin[NBparam] = -1.0e-7;
	  parammax[NBparam] = 1.0e-7;
	  NBparam++;
	}

      for(k=0;k<data.image[piaacmc[0].piaa1CmodesID].md[0].size[0];k++)
	{
	  paramtype[NBparam] = FLOAT;
	  paramvalf[NBparam] = &data.image[piaacmc[0].piaa1CmodesID].array.F[k];
	  paramdelta[NBparam] = 1.0e-9;
	  parammaxstep[NBparam] = 1.0e-8;
	  parammin[NBparam] = -1.0e-7;
	  parammax[NBparam] = 1.0e-7;
	  NBparam++;
	}
    }
  

 







  // Compute Reference 

  /*  load_fits("Lcomb.fits", "Lcomb");
  load_fits("OAincoh.fits", "OAincoh");
  load_fits("LMzonemap.fits", "LMzonemap");
  PIAACMCsimul_mkLyotMask("OAincoh", "Lcomb", "LMzonemap", 0.85, "LMask");
  save_fits("LMask", "!LMask.fits");
  exit(0);
  */

  optsyst[0].FOCMASKarray[0].mode = 1; // use 1-fpm 
  valref = PIAACMCsimul_computePSF(0.0, 0.0);
  exit(0);
 

  if(1) // optimize Lyot stops
    {
      PIAACMCsimul_computePSF(3.0, 0.0);
      IDa = image_ID("WFamp_005");
      xsize = data.image[IDa].md[0].size[0];
      ysize = data.image[IDa].md[0].size[1];
      
      ID = create_2Dimage_ID("OAincoh", xsize, ysize);
      for(ii=0;ii<xsize*ysize;ii++)
	for(k=0;k<optsyst[0].nblambda;k++)
	  data.image[ID].array.F[ii] += data.image[IDa].array.F[k*xsize*ysize+ii]*data.image[IDa].array.F[k*xsize*ysize+ii]/4;
      
      PIAACMCsimul_computePSF(-3.0, 0.0);
      IDa = image_ID("WFamp_005");
      for(ii=0;ii<xsize*ysize;ii++)
	for(k=0;k<optsyst[0].nblambda;k++)
	  data.image[ID].array.F[ii] += data.image[IDa].array.F[k*xsize*ysize+ii]*data.image[IDa].array.F[k*xsize*ysize+ii]/4;

      PIAACMCsimul_computePSF(0.0, 3.0);
      IDa = image_ID("WFamp_005");
      for(ii=0;ii<xsize*ysize;ii++)
	for(k=0;k<optsyst[0].nblambda;k++)
	  data.image[ID].array.F[ii] += data.image[IDa].array.F[k*xsize*ysize+ii]*data.image[IDa].array.F[k*xsize*ysize+ii]/4;

      PIAACMCsimul_computePSF(0.0, -3.0);
      IDa = image_ID("WFamp_005");
      for(ii=0;ii<xsize*ysize;ii++)
	for(k=0;k<optsyst[0].nblambda;k++)
	  data.image[ID].array.F[ii] += data.image[IDa].array.F[k*xsize*ysize+ii]*data.image[IDa].array.F[k*xsize*ysize+ii]/4;

      save_fits("OAincoh", "!OAincoh.fits");

      PIAACMCsimul_computePSF(0.0, 0.0);
      PIAACMCsimul_optimizeLyotStop("WFamp_005", "WFpha_005", "OAincoh", -0.8, -0.2, 0.9, 20, 3);
      delete_image_ID("OAincoh");
      list_image_ID();
      exit(0);
    }

  printf("Reference = %g\n", valref);
  chname_image_ID("imvect", "vecDHref");
  ID = image_ID("vecDHref");
  xsize = data.image[ID].md[0].size[0];
  ysize = data.image[ID].md[0].size[1];
  save_fits("vecDHref", "!vecDHref.fits");
  size1Dvec = data.image[ID].md[0].nelement;
  if(REGPIAASHAPES==1)
    {
      size1Dvec += data.image[piaacmc[0].piaa0CmodesID].md[0].size[0];
      size1Dvec += data.image[piaacmc[0].piaa1CmodesID].md[0].size[0];
    }

  // re-package vector into 1D array and add regularization terms
  IDm = create_2Dimage_ID("DHmask", size1Dvec, 1);
  ID1Dref = create_2Dimage_ID("vecDHref1D", size1Dvec, 1);
  
  ID = image_ID("vecDHref");
  for(ii=0;ii<data.image[ID].md[0].nelement; ii++)
    {
      data.image[ID1Dref].array.F[ii] = data.image[ID].array.F[ii]; 
      data.image[IDm].array.F[ii] = 1.0;
    }

  if(REGPIAASHAPES == 1)
    {
      ID = piaacmc[0].piaa0CmodesID;
      for(jj=0;jj<data.image[piaacmc[0].piaa0CmodesID].md[0].size[0];jj++)
	{
	  data.image[ID1Dref].array.F[ii] = piaa0C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa0C_regcoeff_alpha);
	  data.image[IDm].array.F[ii] = 1.0;
	  ii++;
	}
      
      ID = piaacmc[0].piaa1CmodesID;
      for(jj=0;jj<data.image[piaacmc[0].piaa1CmodesID].md[0].size[0];jj++)
	{
	  data.image[ID1Dref].array.F[ii] = piaa1C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa1C_regcoeff_alpha);
       data.image[IDm].array.F[ii] = 1.0;
       ii++;
	}
    }
  delete_image_ID("vecDHref");

  fp = fopen("val.opt", "w");
  fclose(fp);
 

  //  r = system("cp psfi.fits psfi_ref.fits");
  
  exit(0);




  // FOCAL PLANE MASK THICKNESS OPTIMIZATION
  if(0)
    {
      if(1)
	{
	  IDfpmresp = create_3Dimage_ID("FPMresp", xsize, ysize, data.image[piaacmc[0].zonezID].md[0].size[0]+1);
	  
	  focmMode = data.image[piaacmc[0].zonezID].md[0].size[0]+10;
	  optsyst[0].FOCMASKarray[0].mode = 1; // 1-fpm
	  val = PIAACMCsimul_computePSF(0.0, 0.0);
	  printf("val = %g\n", val);
	  ID = image_ID("imvect");
	  for(ii=0;ii<data.image[ID].md[0].nelement;ii++)
	    data.image[IDfpmresp].array.F[ii] = data.image[ID].array.F[ii];	      
	  
	  for(k=1;k<data.image[piaacmc[0].zonezID].md[0].size[0]+1;k++)
	    {
	      focmMode = k;
	      optsyst[0].FOCMASKarray[0].mode = 0; 
	      val = PIAACMCsimul_computePSF(0.0, 0.0);
	      ID = image_ID("imvect");
	      for(ii=0;ii<data.image[ID].md[0].nelement;ii++)
		{
		  data.image[IDfpmresp].array.F[k*data.image[ID].md[0].nelement+ii] = data.image[ID].array.F[ii];	
		  data.image[IDfpmresp].array.F[ii] -= data.image[ID].array.F[ii];
		}
	      save_fits("FPMresp","!FPMresp.fits");
	    }
	  focmMode = -1;
	}
      else
	IDfpmresp = load_fits("FPMresp.fits", "FPMresp");
      

      if(1)
	{
	  IDref1 = create_2Dimage_ID("vecDHref1", xsize, ysize);
	  for(ii=0;ii<xsize*ysize;ii++)
	    data.image[IDref1].array.F[ii] = data.image[IDfpmresp].array.F[ii];
	  
	  
	  for(zi=1;zi<data.image[piaacmc[0].zonezID].md[0].size[0]+1;zi++)
	    {	  
	      t = data.image[piaacmc[0].zonezID].array.D[zi-1];	  
	      a = data.image[piaacmc[0].zoneaID].array.D[zi-1];
	      
	      for(k=0;k<optsyst[0].nblambda;k++)
		{
		  dpha = OPTICSMATERIALS_pha_lambda(piaacmc[0].fpmmaterial, t, optsyst[0].lambdaarray[k]);	  
		  printf("ZONE %3d    t = %10.5g   lambda = %10.5g   dpha = %10.5g\n", zi, t, optsyst[0].lambdaarray[k], dpha);
		  fflush(stdout);
		  for(ii=0;ii<xsize;ii+=2)
		    {
		      re = data.image[IDfpmresp].array.F[zi*xsize*ysize+k*xsize+ii];
		      im = data.image[IDfpmresp].array.F[zi*xsize*ysize+k*xsize+ii+1];
		      amp = sqrt(re*re+im*im);
		      pha = atan2(im,re);
		      
		      amp *= a;
		      pha += dpha;
		      
		      data.image[IDref1].array.F[k*xsize+ii] += amp*cos(pha);
		      data.image[IDref1].array.F[k*xsize+ii+1] += amp*sin(pha);	  
		    }
		}
	    }
	  save_fits("vecDHref1", "!vecDHref1.fits");
	}
    }



  //
  // RANDOM SCAN AROUND CURRENT POINT
  //
  if(0)
    {
     optsyst[0].FOCMASKarray[0].mode = 1; // use 1-fpm 
     if(paramtype[i]==FLOAT)
       paramrefval[i] = *(paramvalf[i]);
     else
       paramrefval[i] = *(paramval[i]);

     val = PIAACMCsimul_computePSF(0.0, 0.0);
     fp = fopen("param_rand.opt", "w");
     fprintf(fp, "%15g ", val);
     for(i=0;i<NBparam;i++)	   
       {
	 if(paramtype[i]==FLOAT)
	   fprintf(fp, " %8f", *(paramvalf[i]));
	 else
	   fprintf(fp, " %8lf", *(paramval[i]));
       }
     fprintf(fp,"\n");
     fclose(fp);

     while(1)
       {
	 for(i=0;i<NBparam;i++)
	   {
	     if(paramtype[i]==FLOAT)	   
	       *(paramvalf[i]) = (float) (parammin[i]+ran1()*(parammax[i]-parammin[i]));
	     else
	       *(paramval[i]) = (double) (parammin[i]+ran1()*(parammax[i]-parammin[i]));	 
	   }
	 val = PIAACMCsimul_computePSF(0.0, 0.0);
	 fp = fopen("param_rand.opt", "a");
	 fprintf(fp, "%15g ", val);
	 for(i=0;i<NBparam;i++)	   
	   {
	     if(paramtype[i]==FLOAT)
	       fprintf(fp, " %8f", *(paramvalf[i]));
	     else
	       fprintf(fp, " %8lf", *(paramval[i]));
	   }
	 fprintf(fp,"\n");
	 fclose(fp);
	}
    }
  


  //
  // LINEAR OPTIMIZATION AROUND CURRENT POINT
  //
	      
  for(iter=0;iter<NBiter;iter++)
    {
      if(1)
	{
	  IDmodes = create_3Dimage_ID("DHmodes", size1Dvec, 1, NBparam);
	  for(i=0;i<NBparam;i++)
	    {
	      optsyst[0].FOCMASKarray[0].mode = 1; // use 1-fpm 
	      if(paramtype[i]==FLOAT)
		{
		  *(paramvalf[i]) += (float) paramdelta[i];
		  val = PIAACMCsimul_computePSF(0.0, 0.0);
		}
	      else
		{
		  *(paramval[i]) += paramdelta[i];
		  val = PIAACMCsimul_computePSF(0.0, 0.0);
		}

	      sprintf(fname,"!imvect_%02ld.fits", i);
	      save_fits("imvect", fname);
	      ID = image_ID("imvect");



	      // re-package vector into 1D array and add regularization terms
	      ID1D = create_2Dimage_ID("imvect1D", size1Dvec, 1);

	      for(ii=0;ii<data.image[ID].md[0].nelement; ii++)
		data.image[ID1D].array.F[ii] = data.image[ID].array.F[ii]; 

	      if(REGPIAASHAPES==1)
		{		    
		  ID = piaacmc[0].piaa0CmodesID;
		  for(jj=0;jj<data.image[piaacmc[0].piaa0CmodesID].md[0].size[0];jj++)
		    {
		      data.image[ID1D].array.F[ii] = piaa0C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa0C_regcoeff_alpha);
		      ii++;
		    }

		  ID = piaacmc[0].piaa1CmodesID;
		  for(jj=0;jj<data.image[piaacmc[0].piaa1CmodesID].md[0].size[0];jj++)
		    {
		      data.image[ID1D].array.F[ii] = piaa1C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa1C_regcoeff_alpha);
		      ii++;
		    }
		}
	      delete_image_ID("imvect");
	      
	      
	      
	      if(paramtype[i]==FLOAT)
		*(paramvalf[i]) -= (float) paramdelta[i];
	      else
		*(paramval[i]) -= paramdelta[i];
	    


	      for(ii=0;ii<data.image[ID1D].md[0].nelement;ii++)
		data.image[IDmodes].array.F[i*data.image[ID1D].md[0].nelement+ii] = (data.image[ID1D].array.F[ii] - data.image[ID1Dref].array.F[ii]);
	      
	      
	      printf("%3ld %g %g\n", i, val, valref);
	      
	      ID = create_2Dimage_ID("DHmodes2D", size1Dvec, NBparam);
	      for(ii=0;ii<data.image[IDmodes].md[0].nelement;ii++)
		data.image[ID].array.F[ii] = data.image[IDmodes].array.F[ii];
	      save_fits("DHmodes2D", "!DHmodes.fits");	      
	      delete_image_ID("DHmodes2D");
	    }
	}
      else
	IDmodes = load_fits("DHmodes.fits", "DHmodes");
      
      
      

      linopt_imtools_image_fitModes("vecDHref1D", "DHmodes", "DHmask", 1.0e-5, "optcoeff", 0);


      // delete_image_ID("vecDHref1D");
    
      ID = image_ID("optcoeff");
      for(i=0;i<NBparam;i++)
	{
	  paramdeltaval[i] = -loopgain*data.image[ID].array.F[i]*paramdelta[i];
	  if(paramdeltaval[i]<-parammaxstep[i])
	    paramdeltaval[i] = -parammaxstep[i];
	  if(paramdeltaval[i]>parammaxstep[i])
	    paramdeltaval[i] = parammaxstep[i];

	  if(paramtype[i]==FLOAT)
	    *(paramvalf[i]) += (float) paramdeltaval[i];
	  else
	    *(paramval[i]) += paramdeltaval[i];
	}

      val = PIAACMCsimul_computePSF(0.0, 0.0);
      r = system("cp psfi.fits psfi_ref.fits");
      printf(" %g -> %g\n", valref, val);
      list_image_ID();
    

      delete_image_ID("DHmodes");


      ID1Dref = image_ID("vecDHref1D"); //create_2Dimage_ID("vecDHref1D", size1Dvec, 1);      
      ID = image_ID("imvect");
      for(ii=0;ii<data.image[ID].md[0].nelement; ii++)
	data.image[ID1Dref].array.F[ii] = data.image[ID].array.F[ii]; 


      if(REGPIAASHAPES==1)
	{
	  ID = piaacmc[0].piaa0CmodesID;
	  for(jj=0;jj<data.image[piaacmc[0].piaa0CmodesID].md[0].size[0];jj++)
	    {
	      data.image[ID1Dref].array.F[ii] = piaa0C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa0C_regcoeff_alpha);
	      ii++;
	    }
	  
	  ID = piaacmc[0].piaa1CmodesID;
	  for(jj=0;jj<data.image[piaacmc[0].piaa1CmodesID].md[0].size[0];jj++)
	    {
	      data.image[ID1Dref].array.F[ii] = piaa1C_regcoeff*data.image[ID].array.F[jj]*pow(1.0*jj,piaa1C_regcoeff_alpha);
	      ii++;
	    }
	}
      delete_image_ID("imvect");
  
      printf("Writing results\n");
      fflush(stdout);

      ID1Dref = image_ID("vecDHref1D");

      ID = image_ID("optcoeff");
      fp = fopen("param.opt", "w");
      for(i=0;i<NBparam;i++)
	{
	  if(paramtype[i]==FLOAT)
	    fprintf(fp, "%5ld %20g %20g %20g %20g\n", i, *(paramvalf[i]), data.image[ID].array.F[i], data.image[ID].array.F[i]*paramdelta[i], paramdeltaval[i]);
	  else
	    fprintf(fp, "%5ld %20g %20g %20g %20g\n", i, *(paramval[i]), data.image[ID].array.F[i], data.image[ID].array.F[i]*paramdelta[i], paramdeltaval[i]);

	}      
      fclose(fp);
      delete_image_ID("optcoeff");

      fp = fopen("val.opt", "a");
      fprintf(fp, "%ld %g %g\n", iter, val, valref);
      fclose(fp);


      
      PIAAsimul_savepiaacmcconf("piaacmc1");
    }

  free(piaacmc);

  return 0;
}



// NOTES

/*

Initialization steps



PIAAsimul_initpiaacmc()

main simulation parameters (pixel scale zoom factor)
PIAA separation, Fratio
Pupil central obstruction at input and output
focal plane mask radius, material

Creates modes used for fitting PIAA shapes

Creates PIAA shapes

initialize focal plane array of thickness and amplitude transmission
make focal plane mask zones




PIAACMCsimul_init( MIRRORPIAACMCDESIGN *design, long index, double TTxld, double TTyld )

sets up optsyst
creates input pupil slope
makes focal plane mask





PIAACMCsimul_ComputePSF()

runs PIAACMCsimul_init()







*/
