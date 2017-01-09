#include <fitsio.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct mach_timespec *t){
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#else
#include <time.h>
#endif



#include <math.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <ncurses.h>
#include <semaphore.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_blas.h>
#include <pthread.h>


#include "CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_tools/COREMOD_tools.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "linopt_imtools/linopt_imtools.h"
#include "AOloopControl/AOloopControl.h"
#include "image_filter/image_filter.h"
#include "info/info.h"
#include "ZernikePolyn/ZernikePolyn.h"
#include "linopt_imtools/linopt_imtools.h"
#include "image_gen/image_gen.h"
#include "statistic/statistic.h"

#include "FPAOloopControl/FPAOloopControl.h"

#ifdef HAVE_CUDA
#include "cudacomp/cudacomp.h"
#endif




# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000 
# endif








extern DATA data;



long NB_FPAOloopcontrol = 1;
long FPLOOPNUMBER = 0; // current loop index
int FPAOloopcontrol_meminit = 0;
int FPAOlooploadconf_init = 0;

#define FPAOconfname "/tmp/FPAOconf.shm"
FPAOLOOPCONTROL_CONF *FPAOconf; // configuration - this can be an array

float *arrayftmp;
unsigned short *arrayutmp;
int FPcamReadInit = 0;








long FPaoconfID_wfsim = -1;
long FPaoconfID_imWFS0 = -1;
long FPaoconfID_imWFS1 = -1;
int FPWFSatype;
long FPaoconfID_wfsdark = -1;

long FPaoconfID_dmC = -1;
long FPaoconfID_dmRM = -1;




int FPAO_loadcreateshm_log = 0; // 1 if results should be logged in ASCII file
FILE *FPAO_loadcreateshm_fplog;








// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string, not existing image
// 4: existing image
// 5: string 





int FPAOloopControl_loadconfigure_cli()
{
  if(CLI_checkarg(1,2)==0)
    {
      FPAOloopControl_loadconfigure(data.cmdargtoken[1].val.numl, 1, 10);
      return 0;
    }
  else
    return 1;
}







int FPAOloopControl_showparams_cli()
{
	FPAOloopControl_showparams(FPLOOPNUMBER);
}




int FPAOloopControl_set_hardwlatency_frame_cli()
{
  if(CLI_checkarg(1,1)==0)
    {
      FPAOloopControl_set_hardwlatency_frame(data.cmdargtoken[1].val.numf);
      return 0;
    }
  else
    return 1;
}



int FPAOloopControl_MeasureResp_level1_cli()
{
	if(CLI_checkarg(1,1)+CLI_checkarg(2,2)+CLI_checkarg(3,2)+CLI_checkarg(4,2)+CLI_checkarg(5,2)+CLI_checkarg(6,2)+CLI_checkarg(7,2)==0)
		{
			FPAOloopControl_MeasureResp_level1(data.cmdargtoken[1].val.numf, data.cmdargtoken[2].val.numl, data.cmdargtoken[3].val.numl, data.cmdargtoken[4].val.numl, data.cmdargtoken[5].val.numl, data.cmdargtoken[6].val.numl, data.cmdargtoken[7].val.numl);
			return 0;
		}
	else
		return 1;
}






int init_FPAOloopControl()
{
	FILE *fp;
	int r;
	
    if((fp=fopen("LOOPNUMBER","r"))!=NULL)
    {
        r = fscanf(fp,"%ld", &FPLOOPNUMBER);
        printf("LOOP NUMBER = %ld\n", FPLOOPNUMBER);
        fclose(fp);
    }
    else
        FPLOOPNUMBER = 0;


    strcpy(data.cmd[data.NBcmd].key,"FPaolloadconf");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = FPAOloopControl_loadconfigure_cli;
    strcpy(data.cmd[data.NBcmd].info,"load FPAO loop configuration");
    strcpy(data.cmd[data.NBcmd].syntax,"<loop #>");
    strcpy(data.cmd[data.NBcmd].example,"FPaolloadconf 1");
    strcpy(data.cmd[data.NBcmd].Ccall,"int FPAOloopControl_loadconfigure(long loopnb, 1, 10)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"FPaoconfshow");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = FPAOloopControl_showparams_cli;
    strcpy(data.cmd[data.NBcmd].info,"show FPAOconf parameters");
    strcpy(data.cmd[data.NBcmd].syntax,"no argument");
    strcpy(data.cmd[data.NBcmd].example,"FPaoconfshow");
    strcpy(data.cmd[data.NBcmd].Ccall,"int FPAOloopControl_showparams(long loop)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"FPaolsethlat");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = FPAOloopControl_set_hardwlatency_frame_cli;
    strcpy(data.cmd[data.NBcmd].info,"set FPAO hardware latency");
    strcpy(data.cmd[data.NBcmd].syntax,"<hardware latency [frame]>");
    strcpy(data.cmd[data.NBcmd].example,"FPaolsethlat 0.7");
    strcpy(data.cmd[data.NBcmd].Ccall,"int FPAOloopControl_set_hardwlatency_frame(float hardwlatency_frame)");
    data.NBcmd++;


    strcpy(data.cmd[data.NBcmd].key,"FPaoMeasRespl1");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = FPAOloopControl_MeasureResp_level1_cli;
    strcpy(data.cmd[data.NBcmd].info,"measure focal plane response, level 1");
    strcpy(data.cmd[data.NBcmd].syntax,"<ampl [um]> <delay frame [long]> <delayus [long]> <NBave> <NB frame excl> <initMode> <NBiter>");
    strcpy(data.cmd[data.NBcmd].example,"FPaoMeasRespl1 0.05 1 231 5 1 0 10");
    strcpy(data.cmd[data.NBcmd].Ccall,"long FPAOloopControl_MeasureResp_level1(float ampl, long delayfr, long delayRM1us, long NBave, long NBexcl, int FPAOinitMode, long NBiter)");
    data.NBcmd++;

    strcpy(data.module[data.NBmodule].name, __FILE__);
    strcpy(data.module[data.NBmodule].info, "FP AO loop control");
    data.NBmodule++;


    return 0;
}







/*** mode = 0 or 1. if mode == 1, simply connect */


long FPAOloopControl_InitializeMemory(int mode)
{
    int SM_fd;
    struct stat file_stat;
    int create = 0;
    int result;
    long *sizearray;
    char cntname[200];
    int k;
    FILE *fp;
    // FILE *fp1; // testing
    int tmpi;
    int ret;
    char fname[200];
	int loop;
	
	
    SM_fd = open(FPAOconfname, O_RDWR);
    if(SM_fd==-1)
    {
        printf("Cannot import file \"%s\" -> creating file\n", FPAOconfname);
        create = 1;
    }
    else
    {
        fstat(SM_fd, &file_stat);
        printf("File %s size: %zd\n", FPAOconfname, file_stat.st_size);
        if(file_stat.st_size!=sizeof(FPAOLOOPCONTROL_CONF)*NB_FPAOloopcontrol)
        {
            printf("File \"%s\" size is wrong -> recreating file\n", FPAOconfname);
            create = 1;
            close(SM_fd);
        }
    }

    if(create==1)
    {
        SM_fd = open(FPAOconfname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

        if (SM_fd == -1) {
            perror("Error opening file for writing");
            exit(0);
        }

        result = lseek(SM_fd, sizeof(FPAOLOOPCONTROL_CONF)*NB_FPAOloopcontrol-1, SEEK_SET);
        if (result == -1) {
            close(SM_fd);
            perror("Error calling lseek() to 'stretch' the file");
            exit(0);
        }

        result = write(SM_fd, "", 1);
        if (result != 1) {
            close(SM_fd);
            perror("Error writing last byte of the file");
            exit(0);
        }
    }


	
	
    FPAOconf = (FPAOLOOPCONTROL_CONF*) mmap(0, sizeof(FPAOLOOPCONTROL_CONF)*NB_FPAOloopcontrol, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
    if (FPAOconf == MAP_FAILED) {
        close(SM_fd);
        perror("Error mmapping the file");
        exit(0);
    }
    
   
   
   for(loop=0;loop<NB_FPAOloopcontrol; loop++)
	{
	// DM streams
	FPAOconf[loop].sizexDM = 0;
	FPAOconf[loop].sizeyDM = 0;
	
	// Focal plane image stream
	FPAOconf[loop].sizexWFS = 0;
	FPAOconf[loop].sizeyWFS = 0;

	}

	// Calibration
	FPAOloopcontrol_meminit = 1;


    return(0);
}






int FPAOloopControl_loadconfigure(long loop, int mode, int level)
{
   FILE *fp;
    char content[200];
    char name[200];
    char name1[200];
    char fname[200];
    long ID;
    long *sizearray;
    int vOK;
    int kw;
    long k;
    int r;
    int sizeOK;
    char command[500];
    int CreateSMim;
    long ID1tmp, ID2tmp;
    long ii;
    long kk, tmpl;
    char testdirname[200];
    
    FILE *fplog; // human-readable log of load sequence

    if((fplog=fopen("FPloadconf.log", "w"))==NULL)
    {
        printf("ERROR: file FPloadconf.log missing\n");
        exit(0);
    }
    FPAO_loadcreateshm_log = 1;
    FPAO_loadcreateshm_fplog = fplog;


    if(FPAOloopcontrol_meminit==0)
        FPAOloopControl_InitializeMemory(0);



    // printf("mode = %d\n", mode); // not used yet


    // Name definitions for shared memory

    sprintf(name, "FPaol%ld_dmC", loop);
    printf("FP loop DM control file name : %s\n", name);
    strcpy(FPAOconf[loop].dmCname, name);

    sprintf(name, "FPaol%ld_dmRM", loop);
    printf("FP loop DM RM file name : %s\n", name);
    strcpy(FPAOconf[loop].dmRMname, name);

    sprintf(name, "FPaol%ld_wfsim", loop);
    printf("FP loop WFS file name: %s\n", name);
    strcpy(FPAOconf[loop].WFSname, name);



    sizearray = (long*) malloc(sizeof(long)*3);


    // READ LOOP NAME

    if((fp=fopen("./conf/conf_LOOPNAME.txt","r"))==NULL)
    {
        printf("ERROR: file ./conf/conf_LOOPNAME.txt missing\n");
        exit(0);
    }
    r = fscanf(fp, "%s", content);
    printf("loop name : %s\n", content);
    fprintf(fplog, "FPAOconf[%ld].name = %s\n", loop, FPAOconf[loop].name);
    fclose(fp);
    fflush(stdout);
    strcpy(FPAOconf[loop].name, content);

    
   
   
	
	if((fp=fopen("./conf/conf_hardwlatency.txt", "r"))==NULL)
    {
        printf("WARNING: file ./conf/conf_hardwlatency.txt missing\n");
    }
    else
    {
        r = fscanf(fp, "%f", &FPAOconf[loop].hardwlatency);
        printf("hardwlatency : %f\n", FPAOconf[loop].hardwlatency);
        fclose(fp);
        fflush(stdout);
        fprintf(fplog, "AOconf[%ld].hardwlatency = %f\n", loop, FPAOconf[loop].hardwlatency);
   }
	
	FPAOconf[loop].hardwlatency_frame = FPAOconf[loop].hardwlatency * FPAOconf[loop].loopfrequ;
	






	if((fp=fopen("./conf/conf_loopfrequ.txt","r"))==NULL)
    {
        printf("WARNING: file ./conf/conf_loopfrequ.txt missing\n");
        printf("Using default loop speed\n");
        fprintf(fplog, "WARNING: file ./conf/conf_loopfrequ.txt missing. Using default loop speed\n");
        FPAOconf[loop].loopfrequ = 2000.0;
    }
    else
    {
        r = fscanf(fp, "%s", content);
        printf("loopfrequ : %f\n", atof(content));
        fclose(fp);
        fflush(stdout);
        FPAOconf[loop].loopfrequ = atof(content);
        fprintf(fplog, "FPAOconf[%ld].loopfrequ = %f\n", loop, FPAOconf[loop].loopfrequ);
    }




    // Connect to WFS camera
    // This is where the size of the WFS is fixed
    FPaoconfID_wfsim = read_sharedmem_image(FPAOconf[loop].WFSname);
    if(FPaoconfID_wfsim == -1)
        fprintf(fplog, "ERROR : cannot read shared memory stream %s\n", FPAOconf[loop].WFSname);
    else
        fprintf(fplog, "stream %s loaded as ID = %ld\n", FPAOconf[loop].WFSname, FPaoconfID_wfsim);


    FPAOconf[loop].sizexWFS = data.image[FPaoconfID_wfsim].md[0].size[0];
    FPAOconf[loop].sizeyWFS = data.image[FPaoconfID_wfsim].md[0].size[1];
    FPAOconf[loop].sizeWFS = FPAOconf[loop].sizexWFS*FPAOconf[loop].sizeyWFS;

    fprintf(fplog, "FPAO WFS stream size = %ld x %ld\n", FPAOconf[loop].sizexWFS, FPAOconf[loop].sizeyWFS);




    // The AOloopControl_xDloadcreate_shmim functions work as follows:
    // If file already loaded, use it (we assume it's already been properly loaded)
    // If not, attempt to read it from shared memory
    // If not available in shared memory, create it in shared memory
    // if "fname" exists, attempt to load it into the shared memory image

    sprintf(name, "FPaol%ld_wfsdark", loop);
    sprintf(fname, "./conf/FPaol%ld_wfsdark.fits", loop);
    FPaoconfID_wfsdark = AOloopControl_2Dloadcreate_shmim(name, fname, FPAOconf[loop].sizexWFS, FPAOconf[loop].sizeyWFS);
    
    


    // Connect to DM
    // Here the DM size is fixed
    //


    FPaoconfID_dmC = image_ID(FPAOconf[loop].dmCname);
    if(FPaoconfID_dmC==-1)
    {
        printf("connect to %s\n", FPAOconf[loop].dmCname);
        FPaoconfID_dmC = read_sharedmem_image(FPAOconf[loop].dmCname);
        if(FPaoconfID_dmC==-1)
        {
            printf("ERROR: cannot connect to shared memory %s\n", FPAOconf[loop].dmCname);
            exit(0);
        }
    }
    FPAOconf[loop].sizexDM = data.image[FPaoconfID_dmC].md[0].size[0];
    FPAOconf[loop].sizeyDM = data.image[FPaoconfID_dmC].md[0].size[1];
    FPAOconf[loop].sizeDM = FPAOconf[loop].sizexDM*FPAOconf[loop].sizeyDM;
    
    fprintf(fplog, "Connected to DM %s, size = %ld x %ld\n", FPAOconf[loop].dmCname, FPAOconf[loop].sizexDM, FPAOconf[loop].sizeyDM);



    FPaoconfID_dmRM = image_ID(FPAOconf[loop].dmRMname);
    if(FPaoconfID_dmRM==-1)
    {
        printf("connect to %s\n", FPAOconf[loop].dmRMname);
        FPaoconfID_dmRM = read_sharedmem_image(FPAOconf[loop].dmRMname);
        if(FPaoconfID_dmRM==-1)
        {
            printf("ERROR: cannot connect to shared memory %s\n", FPAOconf[loop].dmRMname);
            exit(0);
        }
    }
    fprintf(fplog, "stream %s loaded as ID = %ld\n", FPAOconf[loop].dmRMname, FPaoconfID_dmRM);







    sprintf(name, "FPaol%ld_imWFS0", loop);
    FPaoconfID_imWFS0 = AOloopControl_2Dloadcreate_shmim(name, " ", FPAOconf[loop].sizexWFS, FPAOconf[loop].sizeyWFS);
    COREMOD_MEMORY_image_set_createsem(name, 10);

    sprintf(name, "FPaol%ld_imWFS1", loop);
    FPaoconfID_imWFS1 = AOloopControl_2Dloadcreate_shmim(name, " ", FPAOconf[loop].sizexWFS, FPAOconf[loop].sizeyWFS);
    COREMOD_MEMORY_image_set_createsem(name, 10);


    list_image_ID();

	FPAOlooploadconf_init = 1;
    
    FPAO_loadcreateshm_log = 0;
    fclose(fplog);


    return(0);

}






/* ========================================================================= */
/*                    DISPLAY VALUES OF FPAOconf structure                   */
/* ========================================================================= */


int FPAOloopControl_showparams(long loop)
{
	printf("----------------------------------------------------\n");
	printf("loop number %ld\n", loop);
	printf("\n");
	printf("name       :    %s\n", FPAOconf[loop].name);
	
	printf("dm size    :  %ld x %ld\n", FPAOconf[loop].sizexDM, FPAOconf[loop].sizeyDM);
	printf("WFS size   :  %ld x %ld\n", FPAOconf[loop].sizexWFS, FPAOconf[loop].sizeyWFS);
	printf("loopfrequ  :  %.3f Hz\n", FPAOconf[loop].loopfrequ);
	printf("harw lat   :  %.6f sec = %.3f frame\n", FPAOconf[loop].hardwlatency, FPAOconf[loop].hardwlatency_frame);
	printf("\n");
	printf("PSF flux   : %g\n", FPAOconf[loop].fpim_normFlux);
	printf("PSF center : %.3f x %.3f\n", FPAOconf[loop].fpim_Xcent, FPAOconf[loop].fpim_Ycent);
//	printf("\n", FPAOconf[loop].);
	printf("----------------------------------------------------\n");
	
  return 0;
}






/* ========================================================================= */
/*                    SETTING VALUES OF FPAOconf structure                   */
/* ========================================================================= */



int FPAOloopControl_set_hardwlatency_frame(float hardwlatency_frame)
{
  if(FPAOloopcontrol_meminit==0)
    FPAOloopControl_InitializeMemory(1);

  FPAOconf[FPLOOPNUMBER].hardwlatency_frame = hardwlatency_frame;
  FPAOloopControl_showparams(FPLOOPNUMBER);

  return 0;
}














/** Read image from WFS camera
 *
 * supports ring buffer
 * puts image from camera buffer aoconfID_wfsim into aoconfID_imWFS1 (supplied by user)
 *
 *
 */

int FPAOloopControl_Read_cam_frame(long loop, int semindex)
{
    long imcnt;
    long ii;
    double totalinv;
    char name[200];
    int slice;
    char *ptrv;
    long double tmplv1;
    double tmpf;
    long IDdark;
    char fname[200];
    float resulttotal;
    int sval0, sval;
    void *status = 0;
    long i;
    int semval;
    int s;
    
    int WFSatype;
    long nelem;
 	
	
    WFSatype = data.image[FPaoconfID_wfsim].md[0].atype;
 
 
	if(FPcamReadInit==0)
    {
        arrayftmp = (float*) malloc(sizeof(float)*FPAOconf[loop].sizeWFS);
        arrayutmp = (unsigned short*) malloc(sizeof(unsigned short)*FPAOconf[loop].sizeWFS);

        sprintf(fname, "FPaol%ld_wfsdark", loop);
		FPaoconfID_wfsdark = image_ID(fname);

        // set semaphore to 0
        sem_getvalue(data.image[FPaoconfID_wfsim].semptr[semindex], &semval);
        printf("INITIALIZING SEMAPHORE %d   %s   (%d)\n", semindex, data.image[FPaoconfID_wfsim].md[0].name, semval);
        for(i=0; i<semval; i++)
            sem_trywait(data.image[FPaoconfID_wfsim].semptr[semindex]);
        
        FPcamReadInit = 1;
    }


    if(data.image[FPaoconfID_wfsim].sem==0)
    {
        while(FPAOconf[loop].WFScnt==data.image[FPaoconfID_wfsim].md[0].cnt0) // test if new frame exists
                usleep(5);
    }
    else
        sem_wait(data.image[FPaoconfID_wfsim].semptr[semindex]);

  
    

    slice = 0;
    if(data.image[FPaoconfID_wfsim].md[0].naxis==3) // ring buffer
    {
        slice = data.image[FPaoconfID_wfsim].md[0].cnt1;
        if(slice==-1)
            slice = data.image[FPaoconfID_wfsim].md[0].size[2];
    }

    switch (WFSatype) {
    case FLOAT :
        ptrv = (char*) data.image[FPaoconfID_wfsim].array.F;
        ptrv += sizeof(float)*slice* FPAOconf[loop].sizeWFS;
        memcpy(arrayftmp, ptrv,  sizeof(float)*FPAOconf[loop].sizeWFS);
        break;
    case USHORT :
        ptrv = (char*) data.image[FPaoconfID_wfsim].array.U;
        ptrv += sizeof(unsigned short)*slice* FPAOconf[loop].sizeWFS;
        memcpy (arrayutmp, ptrv, sizeof(unsigned short)*FPAOconf[loop].sizeWFS);
        break;
    default :
        printf("ERROR: DATA TYPE NOT SUPPORTED\n");
        exit(0);
        break;
    }
   




    // Dark subtract -> FPaoconfID_imWFS0

	nelem = FPAOconf[loop].sizeWFS;
	
        switch ( WFSatype ) {
        case USHORT :
# ifdef _OPENMP
            #pragma omp parallel num_threads(8) if (nelem>OMP_NELEMENT_LIMIT)
        {
# endif

# ifdef _OPENMP
            #pragma omp for
# endif
            for(ii=0; ii<nelem; ii++)
                data.image[FPaoconfID_imWFS0].array.F[ii] = ((float) arrayutmp[ii]) - data.image[FPaoconfID_wfsdark].array.F[ii];
# ifdef _OPENMP
        }
# endif
        break;
        case FLOAT :
# ifdef _OPENMP
            #pragma omp parallel num_threads(8) if (nelem>OMP_NELEMENT_LIMIT)
        {
# endif

# ifdef _OPENMP
            #pragma omp for
# endif
            for(ii=0; ii<nelem; ii++)
                data.image[FPaoconfID_imWFS0].array.F[ii] = arrayftmp[ii] - data.image[FPaoconfID_wfsdark].array.F[ii];
# ifdef _OPENMP
        }
# endif
        break;
        default :
            printf("ERROR: WFS data type not recognized\n");
            exit(0);
            break;
        }

        data.image[FPaoconfID_imWFS0].md[0].cnt0 ++;
        data.image[FPaoconfID_imWFS0].md[0].write = 0;
   
    
	FPaoconfID_imWFS1 = FPaoconfID_imWFS0;

    return(0);
}



















/** Measures image response to a series of DM patterns
 *
 * AOinitMode = 0:  create AO shared mem struct
 * AOinitMode = 1:  connect only to AO shared mem struct
 *  
 * 
 * INPUT : DMpoke_name : set of DM patterns
 * OUTPUT : WFSmap_name : WFS maps
 * */

long FPAO_Measure_WFSrespC(long loop, long delayfr, long delayRM1us, long NBave, long NBexcl, char *IDpokeC_name, char *IDoutC_name, int FPAOinitMode, long NBcycle)
{
    char fname[200];
    char name[200];
    char command[200];
    long *sizearray;
	long IDoutC;

    long NBiter = LONG_MAX; // runs until USR1 signal received
    long iter;
    int r;
    long IDpokeC;
    long NBpoke;
    long PokeIndex, PokeIndex1;
	long framesize;
	char *ptr0; // source
	float *arrayf;
    int RT_priority = 80; //any number from 0-99
    struct sched_param schedpar;
    int ret;
    long cntn;

	long ii, kk, kk1;




    schedpar.sched_priority = RT_priority;
    #ifndef __MACH__
    sched_setscheduler(0, SCHED_FIFO, &schedpar);
	#endif


	if(NBcycle < 1)
		NBiter = LONG_MAX; // runs until USR1 signal received
	else
		NBiter = NBcycle;
		
		

    sizearray = (long*) malloc(sizeof(long)*3);
	
	
	printf("INITIALIZE MEMORY (mode %d)....\n", FPAOinitMode);
    fflush(stdout);
    if(FPAOloopcontrol_meminit==0)
        FPAOloopControl_InitializeMemory(FPAOinitMode);
    FPAOloopControl_loadconfigure(FPLOOPNUMBER, 1, 2);
	


    printf("Importing DM response matrix channel shared memory ...\n");
    fflush(stdout);
    FPaoconfID_dmRM = read_sharedmem_image(FPAOconf[loop].dmRMname);

    printf("Importing WFS camera image shared memory ... \n");
	fflush(stdout);
    FPaoconfID_wfsim = read_sharedmem_image(FPAOconf[loop].WFSname);


	IDpokeC = image_ID(IDpokeC_name);
    NBpoke = data.image[IDpokeC].md[0].size[2];
    sizearray[0] = FPAOconf[loop].sizexWFS;
    sizearray[1] = FPAOconf[loop].sizeyWFS;
    sizearray[2] = NBpoke; 

	IDoutC = create_3Dimage_ID(IDoutC_name, sizearray[0], sizearray[1], sizearray[2]);


	arrayf = (float*) malloc(sizeof(float)*FPAOconf[loop].sizeDM);
	for(ii=0;ii<FPAOconf[loop].sizeDM;ii++)
		arrayf[ii] = 0.0;



	for(PokeIndex = 0; PokeIndex < NBpoke; PokeIndex++)
		for(ii=0; ii<FPAOconf[loop].sizeWFS; ii++)
			data.image[IDoutC].array.F[PokeIndex*FPAOconf[loop].sizeWFS+ii] = 0.0;


    cntn = 0;
    iter = 0;



	ptr0 = (char*) data.image[IDpokeC].array.F;
	framesize = sizeof(float)*FPAOconf[loop].sizexDM*FPAOconf[loop].sizeyDM;

    printf("STARTING response measurement...\n");
    fflush(stdout);

    while((iter<NBiter)&&(data.signal_USR1==0))
    {
        printf("iteration # %8ld    \n", iter);
        fflush(stdout);

        
        // initialize with first poke
        kk1 = 0;
		PokeIndex = 0;
		PokeIndex1 = 0;
        
        
        usleep(delayRM1us);    
        data.image[FPaoconfID_dmRM].md[0].write = 1;
        memcpy (data.image[FPaoconfID_dmRM].array.F, ptr0 + PokeIndex1*framesize, sizeof(float)*FPAOconf[loop].sizeDM);
        data.image[FPaoconfID_dmRM].md[0].cnt0++;
        data.image[FPaoconfID_dmRM].md[0].write = 0;
        FPAOconf[loop].DMupdatecnt ++;
        
        
        
        // WAIT FOR LOOP DELAY, PRIMING
		FPAOloopControl_Read_cam_frame(loop, 0);
        
		// read delayfr frames
        for(kk=0; kk<delayfr; kk++)               
            {
                FPAOloopControl_Read_cam_frame(loop, 0);
                kk1++;
                if(kk1==NBave)
                    {
                        kk1 = -NBexcl;
                        PokeIndex1++;
                            
                        if(PokeIndex1>NBpoke-1)
                            PokeIndex1 = NBpoke-1;

                        // POKE            
						usleep(delayRM1us);    
                        data.image[FPaoconfID_dmRM].md[0].write = 1;
                        memcpy (data.image[FPaoconfID_dmRM].array.F, ptr0 + PokeIndex1*framesize, sizeof(float)*FPAOconf[loop].sizeDM);
                        data.image[FPaoconfID_dmRM].md[0].cnt0++;
                        data.image[FPaoconfID_dmRM].md[0].write = 0;
                        FPAOconf[loop].DMupdatecnt ++;
                    }
            }
                
        
        
        
        
        while ((PokeIndex < NBpoke)&&(data.signal_USR1==0))
        {
            // INTEGRATION

            for(kk=0; kk<NBave+NBexcl; kk++)
            {
                FPAOloopControl_Read_cam_frame(loop, 0);
                if(kk<NBave)
                    for(ii=0; ii<FPAOconf[loop].sizeWFS; ii++)
                        data.image[IDoutC].array.F[PokeIndex*FPAOconf[loop].sizeWFS+ii] += data.image[FPaoconfID_imWFS1].array.F[ii];
                kk1++;
                if(kk1==NBave)
                    {
                        kk1 = -NBexcl;
                        PokeIndex1++;

                        if(PokeIndex1>NBpoke-1)
                            PokeIndex1 = NBpoke-1;
                        
            
                        usleep(delayRM1us);
                        data.image[FPaoconfID_dmRM].md[0].write = 1;
                        memcpy (data.image[FPaoconfID_dmRM].array.F, ptr0 + PokeIndex1*framesize, sizeof(float)*FPAOconf[loop].sizeDM);
                        data.image[FPaoconfID_dmRM].md[0].cnt0++;
                        data.image[FPaoconfID_dmRM].md[0].write = 0;
                        FPAOconf[loop].DMupdatecnt ++;
                    }
            }

            PokeIndex++;
        }
        cntn = NBave; // Number of images
            

        for(ii=0; ii<FPAOconf[loop].sizeDM; ii++)
            arrayf[ii] = 0.0;
        
        // zero DM channel
           
        usleep(delayRM1us);
        data.image[FPaoconfID_dmRM].md[0].write = 1;
        memcpy (data.image[FPaoconfID_dmRM].array.F, arrayf, sizeof(float)*FPAOconf[loop].sizeDM);
        data.image[FPaoconfID_dmRM].md[0].cnt0++;
        data.image[FPaoconfID_dmRM].md[0].write = 0;
        FPAOconf[loop].DMupdatecnt ++;
		
 
    } // end of iteration loop 

    free(arrayf);
    free(sizearray);


	for(PokeIndex = 0; PokeIndex < NBpoke; PokeIndex++)
		for(ii=0; ii<FPAOconf[loop].sizeWFS; ii++)
			data.image[IDoutC].array.F[PokeIndex*FPAOconf[loop].sizeWFS+ii] /= NBave*NBcycle;

    return(IDoutC);
}



















// LEVEL 1 CALIBRATION
// no coronagraph
// poke X and Y patterns
// extract:
// - PSF center
// - geometric transformation
// - contrast scaling at max spatial frequency





long FPAOloopControl_MeasureResp_level1(float ampl, long delayfr, long delayRM1us, long NBave, long NBexcl, int FPAOinitMode, long NBiter)
{
	// pokes X and Y patterns
	// period = 1 act

	long IDpokeC;
	long loop = FPLOOPNUMBER;
	long NBpokes = 5;
	long poke;
	
	long ii, jj, kk;
	
	
	
	if(FPAOloopcontrol_meminit==0)
        FPAOloopControl_InitializeMemory(0);
    FPAOloopControl_loadconfigure(loop, 1, 10);
	
	
	// CREATE POKE CUBE
	IDpokeC = create_3Dimage_ID("pokeC", FPAOconf[loop].sizexDM, FPAOconf[loop].sizeyDM, NBpokes);

	poke = 0;
	for(ii=0;ii<FPAOconf[loop].sizexDM; ii++)
		for(jj=0;jj<FPAOconf[loop].sizexDM; jj++)
			data.image[IDpokeC].array.F[poke*FPAOconf[loop].sizeDM + jj*FPAOconf[loop].sizexDM + ii] = 0.0;

	poke = 1;
	for(ii=0;ii<FPAOconf[loop].sizexDM; ii++)
		for(jj=0;jj<FPAOconf[loop].sizexDM; jj++)
			data.image[IDpokeC].array.F[poke*FPAOconf[loop].sizeDM + jj*FPAOconf[loop].sizexDM + ii] = ampl*(ii % 2);
	
	poke = 2;
	for(ii=0;ii<FPAOconf[loop].sizexDM; ii++)
		for(jj=0;jj<FPAOconf[loop].sizexDM; jj++)
			data.image[IDpokeC].array.F[poke*FPAOconf[loop].sizeDM + jj*FPAOconf[loop].sizexDM + ii] = -ampl*(ii % 2);

	poke = 3;
	for(ii=0;ii<FPAOconf[loop].sizexDM; ii++)
		for(jj=0;jj<FPAOconf[loop].sizexDM; jj++)
			data.image[IDpokeC].array.F[poke*FPAOconf[loop].sizeDM + jj*FPAOconf[loop].sizexDM + ii] = ampl*(jj % 2);
	
	poke = 4;
	for(ii=0;ii<FPAOconf[loop].sizexDM; ii++)
		for(jj=0;jj<FPAOconf[loop].sizexDM; jj++)
			data.image[IDpokeC].array.F[poke*FPAOconf[loop].sizeDM + jj*FPAOconf[loop].sizexDM + ii] = -ampl*(jj % 2);


	FPAO_Measure_WFSrespC(loop, delayfr, delayRM1us, NBave, NBexcl, "pokeC", "wfsRespC", FPAOinitMode, NBiter);

	save_fits("wfsRespC", "!wfsRespC.fits");
	
	
	return 0;
}


