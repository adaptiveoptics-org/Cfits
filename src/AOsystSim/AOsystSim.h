/**
 * @file    AOsystSim.h
 * @brief   Function prototypes for AOsystSim
 * 
 * 
 * @author  O. Guyon
 * @date    18 Jun 2017
 *
 * 
 * @bug No known bugs.
 * 
 * @see https://github.com/oguyon/Cfits
 */


#ifndef _AOSYSTSIM_H
#define _AOSYSTSIM_H







int init_AOsystSim();

int AOsystSim_run(int syncmode, long DMindex, long delayus);



/* =============================================================================================== */
/* =============================================================================================== */
/** @name 1. Analytical high contrast PSF simulation
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

typedef struct {
    double alpha; // angle
    double alpha_ld;
    double alpha_arcsec;
    double lambda0; // wavelength at seeing measurement [m]
    double lambdai; // imaging wavelength (science) [m]
    double lambdawfs; // WFS wavelength [m]
    
    double CN2layer_h[20]; // [m]
    double CN2layer_coeff[20]; // sum of coeffs = 1
    
    double D; // telescope aperture [m]
    double r0; // Fried parameter at lambda0 [m]
    double windspeed; // [m/s]
    double betapWFS; // WFS efficiency
    double betaaWFS; // WFS efficiency
    double betapWFSsci; // WFS efficiency Sci
    double betaaWFSsci; // WFS efficiency Sci
    double Fwfs; // source flux [ph.s-1.m-2]
    double Fsci;
    
    double framedelay; // delay in unit of frames
    
    double f_wfs; // spatial frequency
    double f_0; // spatial frequency
    double f; // spatial frequency
    double hf; // sine wave component amplitude
    double X;
    double Y;
    double dX;
    double dY;
    double C0;
    double C1;
    double twfs;
    double twfssci;
    double hfc; // corrected sine wave component amplitude
    double twfs_opt; // optimal WFS exposure time - phase
    double twfssci_opt; // optimal WFS exposure time - phase
    double twfs_opt_amp; // optimal WFS exposure time - amplitude
    double hfca; // time lag term
    double hfcb; // WFS photon noise
    double C2; // quadratic sum of C2a and C2b
    double C2_wfs; // quadratic sum of C2a and C2b
    double C3; // corrected amplitude
    double C4; // chromatic phase
    double C5; // chromatic amplitude
    double C6; // refractive index
    double Csum;
    double Csum_detection; // 5 sigma
    
    // near-IR loop:
    double RIC_hfca; // Refractive Index Chromaticity : time lag
    double RIC_hfcb; // Refractive Index Chromaticity : photon noise
    double RIC_hfc;
    double TL_hfca; // time lag correction: time lag
    double TL_hfcb; // time lag correction: photon noise
    double TL_hfc;
    double C7; // post-nearIR correction time lag
    double C8; // post-nearIR correction 
    double C9; // C4 -> 
    double C10; // C5 ->
    double C11; // post-nearIR correction refractive index
    double Csum2; 
    double Csum2ave; 
   
} EXAOSIMCONF;

/** @brief simple AO filtering model using Fourier analysis
 *         simulates WFS integration, delay, noise (as a function of spatial frequency)
 *
 *  Open loop model
 * 
 *  AO simple filter is modeled by several 2D maps in spatial frequency:
 *  aosf_noise : noise per spatial frequency
 *  aosf_mult : signal throughput per spatial frequency
 *  aosf_gain : loop gain to be applied per spatial frequency
 */

int AOsystSim_simpleAOfilter(const char *IDin_name, const char *IDout_name);

int_fast8_t AOsystSim_extremeAO_contrast_sim();

///@}






/* =============================================================================================== */
/* =============================================================================================== */
/** @name 2. Telescope pupil
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */


/** @brief Make telescope pupil 
 * 
 * 
 */ 
long AOsystSim_mkTelPupDM(const char *ID_name, long msize, double xc, double xy, double rin, double rout, double pupPA, double spiderPA, double spideroffset, double spiderthick, double stretchx);


/** @brief Fit measured DM response to telescope pupil 
 * 
 * 
 */ 
long AOsystSim_fitTelPup(const char *ID_name, const char *IDtelpup_name);

///@}







/* =============================================================================================== */
/* =============================================================================================== */
/** @name 3. Wavefront 
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

int AOsystSim_mkWF_mkCONF(const char *fname);

int AOsystSim_mkWF(const char *CONF_FNAME);

///@}







/* =============================================================================================== */
/* =============================================================================================== */
/** @name 4. Wavefront Sensor
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

int AOsystSim_WFSsim_Pyramid(const char *inWFc_name, const char *outWFSim_name, double modampl, long modnbpts);

int AOsystSim_runWFS(long index, const char *IDout_name);

int AOsystSim_PyrWFS_mkCONF(const char *fname);

int AOsystSim_PyrWFS(const char *CONF_FNAME);

///@}







/* =============================================================================================== */
/* =============================================================================================== */
/** @name 5. Deformable mirror
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

int AOsystSim_DMshape(const char *IDdmctrl_name, const char *IDdmifc_name, const char *IDdm_name);

int AOsystSim_DM_mkCONF(const char *fname);

int AOsystSim_DM(const char *CONF_FNAME);


///@}





/* =============================================================================================== */
/* =============================================================================================== */
/** @name 6. Coronagraph and LOWFS
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

int AOsystSim_coroLOWFS(const char *CONF_FNAME);

///@}








/* =============================================================================================== */
/* =============================================================================================== */
/** @name 7. Focal plane WFS / C
 *  
 */
///@{                                                                                         
/* =============================================================================================== */
/* =============================================================================================== */

long AOsystSim_FPWFS_imsimul(double probeamp, double sepx, double sepy, double contrast, double wferramp, double totFlux, double DMgainErr, double RON, double CnoiseFloor);

int AOsystSim_FPWFS_mkprobes(const char *IDprobeA_name, const char *IDprobeB_name, long dmxsize, long dmysize, double CPAmax, double CPArmin, double CPArmax, double RMSampl, long modegeom);

int AOsystSim_FPWFS_sensitivityAnalysis(int mapmode, int mode, int optmode, int NBprobes);

///@}












#endif
