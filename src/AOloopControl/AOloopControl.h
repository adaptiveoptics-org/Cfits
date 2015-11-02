#ifndef _AOLOOPCONTROL_H
#define _AOLOOPCONTROL_H





#define maxNBMB 100
#define MAX_NUMBER_TIMER 100

typedef struct
{
    struct timespec tnow;  // computed at time of sending DM commands
    double time_sec; // converted in second

    // SETUP
    int init; // has been initialized
    unsigned long long cnt;
    unsigned long long cntmax;
    unsigned long long DMupdatecnt;
    int kill; // set to 1 to kill computation loop

    char name[80];

    // Wavefront sensor camera
    char WFSname[80];
    float DarkLevel;
    long sizexWFS;
    long sizeyWFS;
    long sizeWFS;
    long activeWFScnt; // number of active WFS pixels
    long sizeWFS_active[100]; // only takes into account WFS pixels in use/active for each slice
    long long WFScnt;
    long long WFScntRM;
    int WFSnormalize; // 1 if each WFS frame should be normalized to 1
    float WFSnormfloor;
    float WFStotalflux; // after dark subtraction

    // DM
    char dmCname[80];
    char dmdispname[80];
    char dmRMname[80];
    long sizexDM;
    long sizeyDM;
    long sizeDM;
    long activeDMcnt; // number of active actuators
    long sizeDM_active; // only takes into account DM actuators that are active/in use

    // Modes
    char DMmodesname[80];
    long DMmodesNBblock; // number of mode blocks
    long NBmodes_block[100]; // number of modes within each block

    int init_wfsref0;    // WFS reference image loaded

    int init_RM;        // Response Matrix loaded
    int init_CM;        // Control Matrix loaded
    int init_CMc;       // combine control matrix computed
    int initmapping;
    char respMname[80];
    char contrMname[80];


    long NBDMmodes;
    float maxlimit; // maximum absolute value for mode values
    float mult; // multiplication coefficient to be applied at each loop iteration


    // LOOP CONTROL
    int on;  // goes to 1 when loop starts, put to 0 to turn loop off
    float gain; // overall loop gain
    long framesAve; // number of frames to average

    int status;
    int GPUstatus[50];
    unsigned int NBtimer; // number of active timers - 1 timer per status value
    struct timespec timer[MAX_NUMBER_TIMER];
    
    int RMstatus;
    // 2: wait for image

    // LOOP TUNING
    // BLOCKS OF MODES
    long NBMblocks; // number of mode blocks
    long indexmaxMB[maxNBMB];
    float gainMB[maxNBMB];
    float limitMB[maxNBMB];
    float multfMB[maxNBMB];


    // COMPUTATION
    int GPU; // 1 if matrix multiplication  done by GPU
    int GPUall; // 1 if scaling computations done by GPU
    int GPUusesem; // 1 if using semaphores to control GPU

    
    int AOLCOMPUTE_TOTAL_ASYNC; // 1 if performing image total in separate thread (runs faster, but image total dates from last frame)
    

    

    // LOOP TELEMETRY
    double RMSmodes;
    double RMSmodesCumul;
    long long RMSmodesCumulcnt;

    long logdataID; // image ID containing additional info that can be attached to a image stream log


    // semaphores for communication with GPU computing threads
    //sem_t *semptr; // semaphore for this image

} AOLOOPCONTROL_CONF;




// data passed to each thread
typedef struct
{
    long nelem;
    float *arrayptr;
    float *result; // where to white status
} THDATA_IMTOTAL;


int init_AOloopControl();


long AOloopControl_makeTemplateAOloopconf(long loopnb);
long AOloopControl_CrossProduct(char *ID1_name, char *ID2_name, char *IDout_name);
long AOloopControl_mkModes(char *ID_name, long msizex, long msizey, float CPAmax, float deltaCPA, double xc, double yx, double r0, double r1, int MaskMode, int BlockNB, float SVDlim);
int AOloopControl_camimage_extract2D_sharedmem_loop(char *in_name, char *out_name, long size_x, long size_y, long xstart, long ystart);
int compute_ControlMatrix(long loop, long NB_MODE_REMOVED, char *ID_Rmatrix_name, char *ID_Cmatrix_name, char *ID_VTmatrix_name, double Beta, long NB_MODE_REMOVED_STEP, float eigenvlim);
int AOloopControl_InitializeMemory();
void *compute_function_imtotal( void *ptr );
void *compute_function_dark_subtract( void *ptr );
int Average_cam_frames(long loop, long NbAve, int RM, int normalize, int PixelStreamMode);
//long AOloopControl_MakeDMModes(long loop, long NBmodes, char *IDname);
int AOloopControl_AveStream(char *IDname, double alpha, char *IDname_out_ave, char *IDname_out_AC);
long AOloopControl_loadCM(long loop, char *CMfname);

long AOloopControl_2Dloadcreate_shmim(char *name, char *fname, long xsize, long ysize);
long AOloopControl_3Dloadcreate_shmim(char *name, char *fname, long xsize, long ysize, long zsize);
int AOloopControl_loadconfigure(long loop, int mode, int level);

int AOloopControl_set_modeblock_gain(long loop, long blocknb, float gain, int add);// modal blocks

int set_DM_modes(long loop);
int set_DM_modesRM(long loop);
long AOloopControl_mkHadamardModes50(char *outname);
long AOloopControl_Hadamard_decodeRM(char *inname, char *Hmatname, char *indexname, char *outname);
long AOcontrolLoop_TestDMSpeed(char *dmname, long delayus, long NBpts, float ampl);
long AOcontrolLoop_TestSystemLatency(char *dmname, char *wfsname, long NBiter);
long AOloopControl_TestDMmodeResp(char *DMmodes_name, long index, float ampl, float fmin, float fmax, float fmultstep, float avetime, long dtus, char *DMmask_name, char *DMstream_in_name, char *DMstream_out_name, char *DMstream_meas_name, char *IDout_name);
long AOloopControl_TestDMmodes_Recovery(char *DMmodes_name, float ampl, char *DMmask_name, char *DMstream_in_name, char *DMstream_out_name, char *DMstream_meas_name, long tlagus, long NBave, char *IDout_name, char *IDoutrms_name, char *IDoutmeas_name, char *IDoutmeasrms_name);
long Measure_zonalRM(long loop, double ampl, double delays, long NBave, char *zrespm_name, char *WFSref_name, char *WFSmap_name, char *DMmap_name, long mode, int normalize);
int AOloopControl_ProcessZrespM(long loop, char *zrespm_name, char *WFSref0_name, char *WFSmap_name, char *DMmap_name, double rmampl, int normalize);
int AOloopControl_WFSzpupdate_loop(char *IDzpdm_name, char *IDzrespM_name, char *IDwfszp_name);
int AOloopControl_WFSzeropoint_sum_update_loop(long loopnb, char *ID_WFSzp_name, int NBzp, char *IDwfsref0_name, char *IDwfsref_name);

int Measure_Resp_Matrix(long loop, long NbAve, float amp, long nbloop, long fDelay, long NBiter);
int ControlMatrixMultiply( float *cm_array, float *imarray, long m, long n, float *outvect);
long compute_CombinedControlMatrix(char *IDcmat_name, char *IDmodes_name, char* IDwfsmask_name, char *IDdmmask_name, char *IDcmatc_name, char *IDcmatc_active_name);
int AOcompute(long loop, int normalize);
int AOloopControl_run();

long AOloopControl_sig2Modecoeff(char *WFSim_name, char *IDwfsref_name, char *WFSmodes_name, char *outname);
int AOloopControl_printloopstatus(long loop, long nbcol);
int AOloopControl_loopMonitor(long loop, double frequ, long nbcol);
int AOloopControl_statusStats();
int AOloopControl_showparams(long loop);

int AOloopControl_setLoopNumber(long loop);
int AOloopControl_loopkill();
int AOloopControl_loopon();
int AOloopControl_loopoff();
int AOloopControl_logon();
int AOloopControl_loopstep(long loop, long NBstep);
int AOloopControl_logoff();
int AOloopControl_loopreset();
int AOloopControl_setgain(float gain);
int AOloopControl_setWFSnormfloor(float WFSnormfloor);
int AOloopControl_setmaxlimit(float maxlimit);
int AOloopControl_setmult(float multcoeff);
int AOloopControl_setframesAve(long nbframes);




// "old" blocks (somewhat obsolete)
int AOloopControl_setgainrange(long m0, long m1, float gainval);
int AOloopControl_setlimitrange(long m0, long m1, float limval);
int AOloopControl_setmultfrange(long m0, long m1, float multfval);
int AOloopControl_setgainblock(long mb, float gainval);
int AOloopControl_setlimitblock(long mb, float limitval);
int AOloopControl_setmultfblock(long mb, float multfval);
int AOloopControl_resetRMSperf();
int AOloopControl_scanGainBlock(long NBblock, long NBstep, float gainStart, float gainEnd, long NBgain);

int AOloopControl_InjectMode( long index, float ampl );
int AOloopControl_AutoTune();


int AOloopControl_setparam(long loop, char *key, double value);



int AOloopControl_Measure_WFScam_PeriodicError(long loop, long NBframes, long NBpha, char *IDout_name); // OBSOLETE
int AOloopControl_Remove_WFScamPE(char *IDin_name, char *IDcorr_name, double pha); // OBSOLETE


int AOloopControl_DMmodulateAB(char *IDprobeA_name, char *IDprobeB_name, char *IDdmstream_name, char *IDrespmat_name, char *IDwfsrefstream_name, double delay, long NBprobes);

#endif
