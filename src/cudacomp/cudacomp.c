#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <sched.h>



#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct timespec *t){
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



#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>

#include <assert.h>



#ifdef HAVE_CUDA

#include <cuda_runtime_api.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <device_types.h>
#include <pthread.h>
#include <cusolverDn.h>

 #endif


#include "CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "info/info.h"
#include "cudacomp/cudacomp.h"

#include "linopt_imtools/linopt_imtools.h" // for testing



# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000
# endif

int FORCESEMINIT = 1;




extern DATA data;

    struct timespec tnow;
    struct timespec tdiff;
    double tdiffv;

int IDtimerinit = 0;
long IDtiming = -1; // index to image where timing should be written




#ifdef HAVE_CUDA
int deviceCount;

GPUMATMULTCONF gpumatmultconf[10]; // supports up to 10 configurations


cudaError_t error;
cublasStatus_t stat;
float cublasSgemv_alpha = 1.0;
float cublasSgemv_beta  = 0.0;





#endif



// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string
// 4: existing image
//

#ifdef HAVE_CUDA
int CUDACOMP_test_cli()
{
    if(CLI_checkarg(1,2)+CLI_checkarg(2,2)+CLI_checkarg(3,2)+CLI_checkarg(4,2)==0)
        GPUcomp_test(data.cmdargtoken[1].val.numl, data.cmdargtoken[2].val.numl, data.cmdargtoken[3].val.numl, data.cmdargtoken[4].val.numl);
    else
        return 1;
}
#endif




int init_cudacomp()
{
    long i;
#ifdef HAVE_CUDA
    for(i=0; i<10; i++) {
        gpumatmultconf[i].init = 0;
        gpumatmultconf[i].alloc = 0;
    }
#endif

    strcpy(data.module[data.NBmodule].name,__FILE__);
    strcpy(data.module[data.NBmodule].info,"CUDA wrapper for AO loop");
    data.NBmodule++;


#ifdef HAVE_CUDA
    strcpy(data.cmd[data.NBcmd].key,"cudacompinit");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = CUDACOMP_init;
    strcpy(data.cmd[data.NBcmd].info,"init CUDA comp");
    strcpy(data.cmd[data.NBcmd].syntax,"no argument");
    strcpy(data.cmd[data.NBcmd].example,"cudacompinit");
    strcpy(data.cmd[data.NBcmd].Ccall,"int CUDACOMP_init()");
    data.NBcmd++;

    strcpy(data.cmd[data.NBcmd].key,"cudacomptest");
    strcpy(data.cmd[data.NBcmd].module,__FILE__);
    data.cmd[data.NBcmd].fp = CUDACOMP_test_cli;
    strcpy(data.cmd[data.NBcmd].info,"test CUDA comp");
    strcpy(data.cmd[data.NBcmd].syntax,"<NB actuators [long]> <NB modes [long]> <NB pixels [long]> <NB GPU [long]>");
    strcpy(data.cmd[data.NBcmd].example,"cudacomptest");
    strcpy(data.cmd[data.NBcmd].Ccall,"GPUcomp_test(long NBact, long NBmodes, long WFSsize, long GPUcnt)");
    data.NBcmd++;


#endif
    // add atexit functions here

    return 0;
}










#ifdef HAVE_CUDA
int CUDACOMP_init()
{
    int device;
    struct cudaDeviceProp deviceProp;

    cudaGetDeviceCount(&deviceCount);
    printf("%d devices found\n", deviceCount);
    printf("\n");
    for (device = 0; device < deviceCount; ++device) {
        cudaGetDeviceProperties(&deviceProp, device);
        printf("Device %d [ %20s ]  has compute capability %d.%d.\n",
               device, deviceProp.name, deviceProp.major, deviceProp.minor);
        printf("  Total amount of global memory:                 %.0f MBytes (%llu bytes)\n", (float)deviceProp.totalGlobalMem/1048576.0f, (unsigned long long) deviceProp.totalGlobalMem);
        printf("  (%2d) Multiprocessors\n", deviceProp.multiProcessorCount);
        printf("  GPU Clock rate:                                %.0f MHz (%0.2f GHz)\n", deviceProp.clockRate * 1e-3f, deviceProp.clockRate * 1e-6f);
        printf("\n");
    }

    return(0);
}

















////////////////////////////////////////////////////////////////////////////////
//! Compute reference data set matrix multiply on CPU
//! dmVec = cMat * wfsVec
////////////////////////////////////////////////////////////////////////////////


void matrixMulCPU(float *cMat, float *wfsVec, float *dmVec, int M, int N)
{
    long n, m;
    float sum;
    long index;
    long i;

    printf("Conventional mat mult %d %d\n", M, N);
    for(m=0; m<M; m++)
    {
        dmVec[m] = 0.0;
        for(n=0; n<N; n++)
        {
            index = m*N+n;
            dmVec[m] += cMat[index]*wfsVec[n];
        }
        //cMat[n*M+m]*wfsVec[n];
    }

    printf("cMat  : ");
    for(i=0; i<5; i++)
        printf("%f ", cMat[i]);
    printf(" ... ");
    for(i=N*M-5; i<N*M; i++)
        printf("%f ", cMat[i]);
    printf("\n");

    printf("wfsVec: ");
    for(n=0; n<5; n++)
        printf("%f ", wfsVec[n]);
    printf(" ... ");
    for(n=N-5; n<N; n++)
        printf("%f ", wfsVec[n]);
    printf("\n");

}







int GPUloadCmat(int index)
{
    int device;
    int n, m;
    long ID;
    
    
    printf("LOADING MATRIX TO GPU ... ");
    fflush(stdout);

    // TEST
    ID = create_2Dimage_ID("mgputest", gpumatmultconf[index].N, gpumatmultconf[index].M);
    for(n=0;n<gpumatmultconf[index].N;n++)
        for(m=0;m<gpumatmultconf[index].M;m++)
            data.image[ID].array.F[m*gpumatmultconf[index].N+n] = gpumatmultconf[index].cMat[m*gpumatmultconf[index].N+n];
    save_fits("mgputest","!MgpuTest.fits");
    delete_image_ID("mgputest");
    

    for(device = 0; device < gpumatmultconf[index].NBstreams; device++)
    {
        for (n=gpumatmultconf[index].Noffset[device]; n<gpumatmultconf[index].Noffset[device]+gpumatmultconf[index].Nsize[device]; n++) {
            if(gpumatmultconf[index].orientation==0)
            {
                for (m=0; m<gpumatmultconf[index].M; m++) {
                    gpumatmultconf[index].cMat_part[device][(n-gpumatmultconf[index].Noffset[device])*gpumatmultconf[index].M+m] = gpumatmultconf[index].cMat[m*gpumatmultconf[index].N+n];
                }
            }
            else
            {
                for (m=0; m<gpumatmultconf[index].M; m++) {
                    gpumatmultconf[index].cMat_part[device][(n-gpumatmultconf[index].Noffset[device])*gpumatmultconf[index].M+m] = gpumatmultconf[index].cMat[n*gpumatmultconf[index].M+m];
                }
            }
        }
    }

    for(device=0; device<gpumatmultconf[index].NBstreams; device++)
    {
        cudaSetDevice(gpumatmultconf[index].GPUdevice[device]);
        error = cublasSetMatrix (gpumatmultconf[index].M, gpumatmultconf[index].Nsize[device], sizeof(float), gpumatmultconf[index].cMat_part[device], gpumatmultconf[index].M, gpumatmultconf[index].d_cMat[device], gpumatmultconf[index].M);
        if (error != cudaSuccess)
        {
            printf("cudblasSetMatrix returned error code %d, line(%d)\n", error, __LINE__);
            exit(EXIT_FAILURE);
        }
    }
    printf("done\n");
    fflush(stdout);

    return(0);
}






/** setup matrix multiplication using multiple GPUs */
/*
 * 
 *  IDoutdmmodes_name  = alpha * IDcontrM_name x IDwfsim_name
 * 
 * upon setup, IDwfsim_name is the WFS ref and initWFSref = 0
 * 
*/

int GPU_loop_MultMat_setup(int index, char *IDcontrM_name, char *IDwfsim_name, char *IDoutdmmodes_name, long NBGPUs, int *GPUdevice, int orientation, int USEsem, int initWFSref, long loopnb)
{
    long IDcontrM, IDwfsim, IDwfsref;
    long *sizearraytmp;
    int device;
    struct cudaDeviceProp deviceProp;
    int n, m;
    char sname[200];
    char name[200];
    int ptn;

    long cnt0;
    long cnt;

    long NBiter = 100000;
    long iter = 0;
    
    int cmatdim = 2; // 2D or 3D
    

    

     
    if(gpumatmultconf[index].init == 0)
    {

        printf("STARTING SETUP %d .....\n", index);
        fflush(stdout);
    
 
        if(IDtimerinit == 0)
            {
                sprintf(name, "aol%ld_looptiming", loopnb);
                IDtiming = image_ID(name);
            
            if(IDtiming==-1)
                IDtiming = create_2Dimage_ID(name, 50, 1);
            }
 
 
        
        if(gpumatmultconf[index].alloc == 1)
        {
            GPU_loop_MultMat_free(index);
            gpumatmultconf[index].alloc = 0;
        }

        if(USEsem==1)
            gpumatmultconf[index].sem = 1;
        else
            gpumatmultconf[index].sem = 0;

        printf("USEsem = %d\n", USEsem);
        fflush(stdout);
        


        gpumatmultconf[index].orientation = orientation;

        printf("input CM name : %s\n", IDcontrM_name);
        fflush(stdout);
        //sleep(2);
        gpumatmultconf[index].CM_ID = image_ID(IDcontrM_name);

        printf("CM_ID = %ld\n", gpumatmultconf[index].CM_ID);
        fflush(stdout);
        //	sleep(2);

        gpumatmultconf[index].CM_cnt = data.image[gpumatmultconf[index].CM_ID].md[0].cnt0;



        /// Load Control Matrix
        IDcontrM = image_ID(IDcontrM_name);
        //		printf("control matrix loaded: IDcontrM = %ld\n", IDcontrM);
        //        fflush(stdout);
        //	sleep(2);


        if(orientation==0)
        {
            if(data.image[IDcontrM].md[0].naxis==3)
                {
                    gpumatmultconf[index].M = data.image[IDcontrM].md[0].size[2];
                    gpumatmultconf[index].N = data.image[IDcontrM].md[0].size[0] * data.image[IDcontrM].md[0].size[1];
                    cmatdim = 3;
                }
            else
                {
                    gpumatmultconf[index].M = data.image[IDcontrM].md[0].size[1];
                    gpumatmultconf[index].N = data.image[IDcontrM].md[0].size[0];
                    cmatdim = 2;
               }
            printf("[0] [%ld] M = %d\n", IDcontrM, gpumatmultconf[index].M);
            printf("[0] [%ld] N = %d\n", IDcontrM, gpumatmultconf[index].N);
        }
        else
        {
            if(data.image[IDcontrM].md[0].naxis==3)
            {
                gpumatmultconf[index].M = data.image[IDcontrM].md[0].size[0] * data.image[IDcontrM].md[0].size[1];
                gpumatmultconf[index].N = data.image[IDcontrM].md[0].size[2];
                cmatdim = 3;
            }
            else
            {
                  gpumatmultconf[index].M = data.image[IDcontrM].md[0].size[0];
                gpumatmultconf[index].N = data.image[IDcontrM].md[0].size[1];              
                cmatdim = 2;
            }

            printf("[1] [%ld] M = %d\n", IDcontrM, gpumatmultconf[index].M);
            printf("[1] [%ld] N = %d\n", IDcontrM, gpumatmultconf[index].N);
        }

        gpumatmultconf[index].cMat =  data.image[IDcontrM].array.F;
        

        /// Load Input vectors
        IDwfsim = image_ID(IDwfsim_name);
        gpumatmultconf[index].wfsVec = data.image[IDwfsim].array.F;
        IDwfsref = image_ID(IDwfsim_name);
        gpumatmultconf[index].wfsRef = data.image[IDwfsref].array.F;

        if(orientation == 0)
        {
            
            printf("[0] Input vector size: %ld %ld\n", data.image[IDwfsim].md[0].size[0], data.image[IDwfsim].md[0].size[1]);
            
            if(data.image[IDwfsim].md[0].size[0]*data.image[IDwfsim].md[0].size[1]!=gpumatmultconf[index].N)
            {
                printf("ERROR: CONTRmat and WFSvec size not compatible: %ld %d\n", data.image[IDwfsim].md[0].size[0]*data.image[IDwfsim].md[0].size[1], gpumatmultconf[index].N);
                fflush(stdout);
                sleep(2);
                exit(0);
            }
        }
        else
        {
            printf("[1] Input vector size: %ld \n", data.image[IDwfsim].md[0].size[0]);
            if(data.image[IDwfsim].md[0].size[0]!=gpumatmultconf[index].N)
            {
                printf("ERROR: CONTRmat and WFSvec size not compatible: %ld %d\n", data.image[IDwfsim].md[0].size[0], gpumatmultconf[index].N);
                fflush(stdout);
                sleep(2);
                exit(0);
            }
        }


        if((gpumatmultconf[index].IDout = image_ID(IDoutdmmodes_name)) == -1)
        {
            sizearraytmp = (long*) malloc(sizeof(long)*2);
            sizearraytmp[0] = gpumatmultconf[index].M;
            sizearraytmp[1] = 1;
            gpumatmultconf[index].IDout = create_image_ID(IDoutdmmodes_name, 2, sizearraytmp, FLOAT, 1, 10);
            free(sizearraytmp);
        }
        else
        {
            if(data.image[gpumatmultconf[index].IDout].md[0].size[0] * data.image[gpumatmultconf[index].IDout].md[0].size[1] != gpumatmultconf[index].M)
            {
                printf("ERROR: CONTRmat and WFSvec size not compatible: %ld %d\n", data.image[gpumatmultconf[index].IDout].md[0].size[0] * data.image[gpumatmultconf[index].IDout].md[0].size[1], gpumatmultconf[index].M);
                printf("gpumatmultconf[index].IDout = %ld\n", gpumatmultconf[index].IDout);
                list_image_ID();
                fflush(stdout);
                sleep(2);
                exit(0);
            }
        }

        gpumatmultconf[index].dmVecTMP = data.image[gpumatmultconf[index].IDout].array.F;




        cudaGetDeviceCount(&deviceCount);
        printf("%d devices found\n", deviceCount);
        printf("\n");
        for (device = 0; device < deviceCount; ++device) {
            cudaGetDeviceProperties(&deviceProp, device);
            printf("Device %d [ %20s ]  has compute capability %d.%d.\n",
                   device, deviceProp.name, deviceProp.major, deviceProp.minor);
            printf("  Total amount of global memory:                 %.0f MBytes (%llu bytes)\n", (float)deviceProp.totalGlobalMem/1048576.0f, (unsigned long long) deviceProp.totalGlobalMem);
            printf("  (%2d) Multiprocessors\n", deviceProp.multiProcessorCount);
            printf("  GPU Clock rate:                                %.0f MHz (%0.2f GHz)\n", deviceProp.clockRate * 1e-3f, deviceProp.clockRate * 1e-6f);
            printf("\n");
        }


        gpumatmultconf[index].NBstreams = deviceCount;
        if(NBGPUs<deviceCount)
            gpumatmultconf[index].NBstreams = NBGPUs;
        



        gpumatmultconf[index].Nsize = (int*) malloc(sizeof(long)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].Noffset = (int*) malloc(sizeof(long)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].Noffset[0] = 0;
        for(device=1; device<gpumatmultconf[index].NBstreams; device++)
        {
            gpumatmultconf[index].Noffset[device] = gpumatmultconf[index].Noffset[device-1] + (long) (gpumatmultconf[index].N/gpumatmultconf[index].NBstreams);
            gpumatmultconf[index].Nsize[device-1] = gpumatmultconf[index].Noffset[device] - gpumatmultconf[index].Noffset[device-1];
        }
        gpumatmultconf[index].Nsize[gpumatmultconf[index].NBstreams-1] = gpumatmultconf[index].N-gpumatmultconf[index].Noffset[gpumatmultconf[index].NBstreams-1];
     
     
        printf("Allocating physical GPUs to streams\n");
        fflush(stdout);
     
        gpumatmultconf[index].GPUdevice = (int*) malloc(sizeof(int)*NBGPUs);
        for (device = 0; device < gpumatmultconf[index].NBstreams; device++)
        {
            printf("stream %2d  ->  GPU device %2d\n", device, GPUdevice[device]);
            fflush(stdout);
            gpumatmultconf[index].GPUdevice[device] = GPUdevice[device];
        }

        printf("-----------------------------------------------------\n");
        for(device=0; device<gpumatmultconf[index].NBstreams; device++)
        {
            printf("DEVICE %2d  [%2d]:  %5d -> %5d  (%d)\n", device, gpumatmultconf[index].GPUdevice[device], gpumatmultconf[index].Noffset[device], gpumatmultconf[index].Noffset[device]+gpumatmultconf[index].Nsize[device], gpumatmultconf[index].Nsize[device]);
            fflush(stdout);
        }
        printf("-----------------------------------------------------\n");




        // device (GPU)
        gpumatmultconf[index].d_cMat = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].d_wfsVec = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].d_dmVec = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].d_wfsRef = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams); // WFS reference
        gpumatmultconf[index].d_dmRef = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);  // DM reference

        gpumatmultconf[index].stream = (cudaStream_t*) malloc(sizeof(cudaStream_t)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].handle = (cublasHandle_t*) malloc(sizeof(cublasHandle_t)*gpumatmultconf[index].NBstreams);


        // host (computer)
        gpumatmultconf[index].cMat_part = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].wfsVec_part = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].dmVec_part = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].wfsRef_part = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams); // WFS reference
        gpumatmultconf[index].dmRef_part = (float **) malloc(sizeof(float*)*gpumatmultconf[index].NBstreams);  // DM reference (for checking only)

        gpumatmultconf[index].refWFSinit = (int*) malloc(sizeof(int)*gpumatmultconf[index].NBstreams);


        gpumatmultconf[index].semptr1 = (sem_t **) malloc(sizeof(sem_t*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].semptr2 = (sem_t **) malloc(sizeof(sem_t*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].semptr3 = (sem_t **) malloc(sizeof(sem_t*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].semptr4 = (sem_t **) malloc(sizeof(sem_t*)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].semptr5 = (sem_t **) malloc(sizeof(sem_t*)*gpumatmultconf[index].NBstreams);
 

        for(device = 0; device < gpumatmultconf[index].NBstreams; device++)
        {
            gpumatmultconf[index].cMat_part[device] = (float*) malloc(sizeof(float)*gpumatmultconf[index].M*gpumatmultconf[index].Nsize[device]);
            gpumatmultconf[index].wfsVec_part[device] = (float*) malloc(sizeof(float)*gpumatmultconf[index].Nsize[device]);
            gpumatmultconf[index].wfsRef_part[device] = (float*) malloc(sizeof(float)*gpumatmultconf[index].Nsize[device]);
            gpumatmultconf[index].dmVec_part[device] = (float*) malloc(sizeof(float)*gpumatmultconf[index].M);
            gpumatmultconf[index].dmRef_part[device] = (float*) malloc(sizeof(float)*gpumatmultconf[index].M);

            sprintf(sname, "i%d_gpu%d_sem1", index, device);
            if ((gpumatmultconf[index].semptr1[device] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                perror("semaphore initilization");
                exit(0);
            }
            sem_init(gpumatmultconf[index].semptr1[device], 1, 0);

            sprintf(sname, "i%d_gpu%d_sem2", index, device);
            if ((gpumatmultconf[index].semptr2[device] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                perror("semaphore initilization");
                exit(0);
            }
            sem_init(gpumatmultconf[index].semptr2[device], 1, 0);

            sprintf(sname, "i%d_gpu%d_sem3", index, device);
            if ((gpumatmultconf[index].semptr3[device] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                perror("semaphore initilization");
                exit(0);
            }
            sem_init(gpumatmultconf[index].semptr3[device], 1, 0);

            sprintf(sname, "i%d_gpu%d_sem4", index, device);
            if ((gpumatmultconf[index].semptr4[device] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                perror("semaphore initilization");
                exit(0);
            }
            sem_init(gpumatmultconf[index].semptr4[device], 1, 0);

            sprintf(sname, "i%d_gpu%d_sem5", index, device);
            if ((gpumatmultconf[index].semptr5[device] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                perror("semaphore initilization");
                exit(0);
            }
            sem_init(gpumatmultconf[index].semptr5[device], 1, 0);

        }
        

        for (device = 0; device < gpumatmultconf[index].NBstreams; device++)
        {
            cudaSetDevice(GPUdevice[device]);
            cudaStreamCreate( &gpumatmultconf[index].stream[device]);
        }

        for(device=0; device<gpumatmultconf[index].NBstreams; device++)
        {
            cudaSetDevice(GPUdevice[device]);

            // ALLOCATE MEMORY ON DEVICE

            error = cudaMalloc((void **) &gpumatmultconf[index].d_cMat[device], sizeof(float)*gpumatmultconf[index].M*gpumatmultconf[index].Nsize[device]);
            if (error != cudaSuccess)
            {
                printf("cudaMalloc d_cMat returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }


            error = cudaMalloc((void **) &gpumatmultconf[index].d_wfsVec[device], sizeof(float)*gpumatmultconf[index].Nsize[device]);
            if (error != cudaSuccess)
            {
                printf("cudaMalloc d_wfsVec returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }

            error = cudaMalloc((void **) &gpumatmultconf[index].d_wfsRef[device], sizeof(float)*gpumatmultconf[index].Nsize[device]);
            if (error != cudaSuccess)
            {
                printf("cudaMalloc d_wfsRef returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }

            error = cudaMalloc((void **) &gpumatmultconf[index].d_dmVec[device], sizeof(float)*gpumatmultconf[index].M);
            if (error != cudaSuccess)
            {
                printf("cudaMalloc d_dmVec returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }

            error = cudaMalloc((void **) &gpumatmultconf[index].d_dmRef[device], sizeof(float)*gpumatmultconf[index].M);
            if (error != cudaSuccess)
            {
                printf("cudaMalloc d_dmVec returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }


            stat = cublasCreate(&gpumatmultconf[index].handle[device]);
            if (stat != CUBLAS_STATUS_SUCCESS) {
                printf ("CUBLAS initialization failed\n");
                return EXIT_FAILURE;
            }

        }

        for(device = 0; device < gpumatmultconf[index].NBstreams; device++)
            for (n=gpumatmultconf[index].Noffset[device]; n<gpumatmultconf[index].Noffset[device]+gpumatmultconf[index].Nsize[device]; n++)
                {
                    gpumatmultconf[index].wfsVec_part[device][n-gpumatmultconf[index].Noffset[device]] = gpumatmultconf[index].wfsVec[n];
                    gpumatmultconf[index].wfsRef_part[device][n-gpumatmultconf[index].Noffset[device]] = gpumatmultconf[index].wfsRef[n];
                }

    // copy memory to devices
    for(device=0; device<gpumatmultconf[index].NBstreams; device++)
        {
            error = cudaMemcpy(gpumatmultconf[index].d_wfsVec[device], gpumatmultconf[index].wfsVec_part[device], sizeof(float)*gpumatmultconf[index].Nsize[device], cudaMemcpyHostToDevice);
            if (error != cudaSuccess)
            {
                printf("cudaMemcpy d_wfsVec wfsVec returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }
            
            
            
            printf("COPY wfsRef_part to d_wfsRef\n");
            fflush(stdout);
            error = cudaMemcpy(gpumatmultconf[index].d_wfsRef[device], gpumatmultconf[index].wfsRef_part[device], sizeof(float)*gpumatmultconf[index].Nsize[device], cudaMemcpyHostToDevice);
            if (error != cudaSuccess)
            {
                printf("cudaMemcpy d_wfsRef wfsRef returned error code %d, line(%d)\n", error, __LINE__);
                exit(EXIT_FAILURE);
            }
        }

        GPUloadCmat(index);


        printf("SETUP %d DONE, READY TO START COMPUTATIONS  ", index);
        fflush(stdout);

        gpumatmultconf[index].iret = (int*) malloc(sizeof(int)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].thdata = (THDATA*) malloc(sizeof(THDATA)*gpumatmultconf[index].NBstreams);
        gpumatmultconf[index].threadarray = (pthread_t*) malloc(sizeof(pthread_t)*gpumatmultconf[index].NBstreams);
    
        for(m=0; m<gpumatmultconf[index].M; m++)
            gpumatmultconf[index].dmVecTMP[m] = 0.0;

        cnt = 0;
        iter = 0;
        gpumatmultconf[index].init = 1;

        printf(". . . \n");
        fflush(stdout);
    }
    
    for(device=0; device<gpumatmultconf[index].NBstreams; device++)
       gpumatmultconf[index].refWFSinit[device] = initWFSref;
    
   // printf("CONFIGURATION DONE \n");
   // fflush(stdout);
    
    return(0);
}







// increments status by 4
int GPU_loop_MultMat_execute(int index, int *status, int *GPUstatus, float alpha, float beta)
{
    int m;
    int ptn;
    int statustot;
    int semval;
    long cnt;


    cublasSgemv_alpha = alpha;
    cublasSgemv_beta = beta;


    *status = *status + 1;  // ->7
    clock_gettime(CLOCK_REALTIME, &tnow);
    tdiff = info_time_diff(data.image[IDtiming].md[0].wtime, tnow);
    tdiffv = 1.0*tdiff.tv_sec + 1.0e-9*tdiff.tv_nsec;
    data.image[IDtiming].array.F[*status] = tdiffv;
    

    if(index==0) /// main CM multiplication loop
    {
        //	gpumatmultconf[index].NBstreams = 6;
        if(gpumatmultconf[index].CM_cnt != data.image[gpumatmultconf[index].CM_ID].md[0].cnt0)
            if(data.image[gpumatmultconf[index].CM_ID].md[0].write == 0)
            {
                printf("New CM detected (cnt : %ld)\n", data.image[gpumatmultconf[index].CM_ID].md[0].cnt0);
                GPUloadCmat(index);
                gpumatmultconf[index].CM_cnt = data.image[gpumatmultconf[index].CM_ID].md[0].cnt0;
            }
    }
    
    
 
    // index is the matrix multiplication index (unique to each matrix multiplication stream operation)
    // ptn is the thread number = GPU device number


//    if((gpumatmultconf[index].sem==0)||



    if(gpumatmultconf[index].gpuinit==0)
    {
        printf("GPU pthread create, index = %d    %d %d\n", index, gpumatmultconf[index].sem, gpumatmultconf[index].gpuinit);//TEST
        fflush(stdout);
                
        for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
        {
            gpumatmultconf[index].thdata[ptn].thread_no = ptn;
            gpumatmultconf[index].thdata[ptn].numl0 = ptn*ptn;
            gpumatmultconf[index].thdata[ptn].cindex = index;
            gpumatmultconf[index].thdata[ptn].status = GPUstatus;
            gpumatmultconf[index].iret[ptn] = pthread_create( &gpumatmultconf[index].threadarray[ptn], NULL, compute_function, (void*) &gpumatmultconf[index].thdata[ptn]);
            if(gpumatmultconf[index].iret[ptn])
            {
                fprintf(stderr,"Error - pthread_create() return code: %d\n", gpumatmultconf[index].iret[ptn]);
                exit(EXIT_FAILURE);
            }
        }
        gpumatmultconf[index].gpuinit = 1;
    }
    *status = *status + 1;  // -> 8
    clock_gettime(CLOCK_REALTIME, &tnow);
    tdiff = info_time_diff(data.image[IDtiming].md[0].wtime, tnow);
    tdiffv = 1.0*tdiff.tv_sec + 1.0e-9*tdiff.tv_nsec;
    data.image[IDtiming].array.F[*status] = tdiffv;
 


    if(gpumatmultconf[index].sem==0)
    {
        for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
            pthread_join( gpumatmultconf[index].threadarray[ptn], NULL);
    }
    else
    {
        for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
        {
            sem_post(gpumatmultconf[index].semptr1[ptn]); // START COMPUTATION
            sem_post(gpumatmultconf[index].semptr4[ptn]);
        }

        for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
            sem_wait(gpumatmultconf[index].semptr5[ptn]); // WAIT FOR RESULT

        // for safety, set semaphores to zerosem_getvalue(data.image[IDarray[i]].semptr[s], &semval);
        if(FORCESEMINIT==1)
            for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
            {
                sem_getvalue(gpumatmultconf[index].semptr5[ptn], &semval);
                for(cnt=0; cnt<semval; cnt++)
                    sem_trywait(gpumatmultconf[index].semptr5[ptn]);
            }

    }


    // SUM RESULTS FROM SEPARATE GPUs
    *status = *status + 1;  // -> 9
    clock_gettime(CLOCK_REALTIME, &tnow);
    tdiff = info_time_diff(data.image[IDtiming].md[0].wtime, tnow);
    tdiffv = 1.0*tdiff.tv_sec + 1.0e-9*tdiff.tv_nsec;
    data.image[IDtiming].array.F[*status] = tdiffv;
 
    data.image[gpumatmultconf[index].IDout].md[0].write = 0;

    for(m=0; m<gpumatmultconf[index].M; m++)
        gpumatmultconf[index].dmVecTMP[m] = 0.0; 

    

    for(ptn=0; ptn<gpumatmultconf[index].NBstreams; ptn++)
    {
        for(m=0; m<gpumatmultconf[index].M; m++)
            gpumatmultconf[index].dmVecTMP[m] += gpumatmultconf[index].dmVec_part[ptn][m];
    }
  
    if(data.image[gpumatmultconf[index].IDout].sem > 0)
        sem_post(data.image[gpumatmultconf[index].IDout].semptr[0]);
        
    if(data.image[gpumatmultconf[index].IDout].sem > 1)
        sem_post(data.image[gpumatmultconf[index].IDout].semptr[1]);



    data.image[gpumatmultconf[index].IDout].md[0].write = 0;
    data.image[gpumatmultconf[index].IDout].md[0].cnt0++;

    *status = *status + 1; // -> 10
    clock_gettime(CLOCK_REALTIME, &tnow);
    tdiff = info_time_diff(data.image[IDtiming].md[0].wtime, tnow);
    tdiffv = 1.0*tdiff.tv_sec + 1.0e-9*tdiff.tv_nsec;
    data.image[IDtiming].array.F[*status] = tdiffv;
 
 
    return(0);
}









int GPU_loop_MultMat_free(int index)
{
    int device;

    cudaFree(gpumatmultconf[index].d_cMat);
    cudaFree(gpumatmultconf[index].d_dmVec);
    cudaFree(gpumatmultconf[index].d_wfsVec);
    cudaFree(gpumatmultconf[index].d_wfsRef);
    cudaFree(gpumatmultconf[index].d_dmRef);
    free(gpumatmultconf[index].stream);

    for(device=0; device<gpumatmultconf[index].NBstreams; device++)
    {
        // free memory for stream
        cublasDestroy(gpumatmultconf[index].handle[device]);
        free(gpumatmultconf[index].cMat_part[device]);
        free(gpumatmultconf[index].wfsVec_part[device]);
        free(gpumatmultconf[index].dmVec_part[device]);
    }

    free(gpumatmultconf[index].cMat_part);
    free(gpumatmultconf[index].dmVec_part);
    free(gpumatmultconf[index].wfsVec_part);

    free(gpumatmultconf[index].Nsize);
    free(gpumatmultconf[index].Noffset);

    free(gpumatmultconf[index].iret);
    free(gpumatmultconf[index].threadarray);
    free(gpumatmultconf[index].thdata);

    free(gpumatmultconf[index].refWFSinit);

    free(gpumatmultconf[index].GPUdevice);

    return(0);
}






/* 
 *
 * sequence of events :
 * 
 * wait semptr1              (wait for input image data)
 * transfer input CPU -> GPU  
 * post semptr2
 * COMPUTE
 * post semptr3
 * wait semptr4
 * 
 * 
 * 
 *  
 */


void *compute_function( void *ptr )
{
    THDATA *thdata;
    int device;
    int n, m;
    int index;
    char *ptr0; // source
    char *ptr1; // dest
    float *ptr0f; // test
    int *ptrstat;
    long IDtest;
    int k;
    int kmax = 10;
    char fname[200];
    long long iter;
    long long itermax = 1;
    float imtot;
    float alphatmp;
    float betatmp;
    int semval;
    long cnt;
    FILE *fptest;
    long ii;

    float alpharef, betaref;

    thdata = (THDATA*) ptr;
    device = thdata->thread_no;
    index = thdata->cindex;

    ptrstat = (int*) ((char*) thdata->status + sizeof(int)*device + sizeof(int)*10*index);

    *ptrstat = 1;



    ptr0 = (char*) gpumatmultconf[index].wfsVec;
    ptr0 += sizeof(float)*gpumatmultconf[index].Noffset[device];
    ptr0f = (float*) ptr0;

    if(index==0)
        cudaSetDevice(gpumatmultconf[index].GPUdevice[device]);

    cublasSetStream( gpumatmultconf[index].handle[device], gpumatmultconf[index].stream[device] );



    if(gpumatmultconf[index].sem==1)
        itermax = -1;
    else
        itermax = 1;

    iter = 0;
    while(iter != itermax)
    {
        // copy DM reference to output to prepare computation:   d_dmVec <- d_dmRef
        error = cudaMemcpy(gpumatmultconf[index].d_dmVec[device], gpumatmultconf[index].d_dmRef[device], sizeof(float)*gpumatmultconf[index].M, cudaMemcpyDeviceToDevice);
        if (error != cudaSuccess)
        {
            printf("cudaMemcpy d_wfsVec wfsVec returned error code %d, line(%d)\n", error, __LINE__);
            exit(EXIT_FAILURE);
        }

        *ptrstat = 2; // wait for image
        if(gpumatmultconf[index].sem==1)
        {
            sem_wait(gpumatmultconf[index].semptr1[device]);
            
            if(FORCESEMINIT==1)
            {
                sem_getvalue(gpumatmultconf[index].semptr1[device], &semval);
                for(cnt=0; cnt<semval; cnt++)
                    sem_trywait(gpumatmultconf[index].semptr1[device]);
            }
        }

        *ptrstat = 3; // transfer: prt0 -> d_wfsVec
        stat = cublasSetVector(gpumatmultconf[index].Nsize[device], sizeof(float), (float*) ptr0, 1, gpumatmultconf[index].d_wfsVec[device], 1);
        if (stat != CUBLAS_STATUS_SUCCESS)
        {
            fprintf(stderr, "!!!! device access error (read C)\n");
            if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
            if(stat == CUBLAS_STATUS_INVALID_VALUE)
                printf("   CUBLAS_STATUS_INVALID_VALUE\n");
            if(stat == CUBLAS_STATUS_MAPPING_ERROR)
                printf("   CUBLAS_STATUS_MAPPING_ERROR\n");
            exit(EXIT_FAILURE);
        }






        if(gpumatmultconf[index].refWFSinit[device] == 0) // compute DM reference (used when reference changes)
        {
            *ptrstat = 4; // compute

            if(gpumatmultconf[index].sem==1)
                sem_post(gpumatmultconf[index].semptr2[device]);


            printf("%d  GPU %d: compute reference product\n", index, device);
            fflush(stdout);
 
//            alphatmp = cublasSgemv_alpha;
//            betatmp = cublasSgemv_beta;

        // MOVE THIS TO CPU AS A SEPARATE THREAD TO AVOID LOOP PAUSE ??
    //        cublasSgemv_alpha = 1.0;
    //        cublasSgemv_beta = 0.0;
            alpharef = 1.0;
            betaref = 0.0;
            stat = cublasSgemv(gpumatmultconf[index].handle[device], CUBLAS_OP_N, gpumatmultconf[index].M, gpumatmultconf[index].Nsize[device], &alpharef, gpumatmultconf[index].d_cMat[device], gpumatmultconf[index].M, gpumatmultconf[index].d_wfsVec[device], 1, &betaref, gpumatmultconf[index].d_dmRef[device], 1);
            if (stat != CUBLAS_STATUS_SUCCESS)
            {
                printf("cublasSgemv returned error code %d, line(%d)\n", stat, __LINE__);
                if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                    printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
                if(stat == CUBLAS_STATUS_INVALID_VALUE)
                    printf("   CUBLAS_STATUS_INVALID_VALUE\n");
                if(stat == CUBLAS_STATUS_ARCH_MISMATCH)
                    printf("   CUBLAS_STATUS_ARCH_MISMATCH\n");
                if(stat == CUBLAS_STATUS_EXECUTION_FAILED)
                    printf("   CUBLAS_STATUS_EXECUTION_FAILED\n");
                exit(EXIT_FAILURE);
            }
  //          cublasSgemv_alpha = alphatmp;
  //          cublasSgemv_beta = betatmp;

            gpumatmultconf[index].refWFSinit[device] = 1;


           if(gpumatmultconf[index].sem==1)
                sem_post(gpumatmultconf[index].semptr3[device]);

            *ptrstat = 5; // transfer result

            if(gpumatmultconf[index].sem==1)
            {
                sem_wait(gpumatmultconf[index].semptr4[device]);
                if(FORCESEMINIT==1)
                    {
                        sem_getvalue(gpumatmultconf[index].semptr4[device], &semval);
                        for(cnt=0; cnt<semval; cnt++)
                            sem_trywait(gpumatmultconf[index].semptr4[device]);
                    }
            }


            // copy d_dmRef -> dmRef_part
            stat = cublasGetVector(gpumatmultconf[index].M, sizeof(float), gpumatmultconf[index].d_dmRef[device], 1, gpumatmultconf[index].dmRef_part[device], 1);
            if (stat != CUBLAS_STATUS_SUCCESS)
            {
                fprintf(stderr, "!!!! device access error (read C)\n");
                if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                    printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
                if(stat == CUBLAS_STATUS_INVALID_VALUE)
                    printf("   CUBLAS_STATUS_INVALID_VALUE\n");
                if(stat == CUBLAS_STATUS_MAPPING_ERROR)
                    printf("   CUBLAS_STATUS_MAPPING_ERROR\n");
                exit(EXIT_FAILURE);
            }
            
            // TEST
    
            sprintf(fname, "gputest%d.txt", device);
            if((fptest = fopen(fname, "w"))==NULL)
            {
                printf("ERROR: cannot create file \"%s\"\n", fname);
                exit(0);
            }
            printf("Writing test file \"%s\"\n", fname);
            fflush(stdout);
            for(ii=0;ii<gpumatmultconf[index].M;ii++)
                fprintf(fptest, "%ld %f\n", ii, gpumatmultconf[index].dmRef_part[device][ii]);
            fclose(fptest);
            
            
            
 
 
         if(gpumatmultconf[index].sem==1)
            sem_post(gpumatmultconf[index].semptr5[device]);

        *ptrstat = 6;


 
        }
        else
        {
            *ptrstat = 4; // compute

            if(gpumatmultconf[index].sem==1)
                sem_post(gpumatmultconf[index].semptr2[device]);

            stat = cublasSgemv(gpumatmultconf[index].handle[device], CUBLAS_OP_N, gpumatmultconf[index].M, gpumatmultconf[index].Nsize[device], &cublasSgemv_alpha, gpumatmultconf[index].d_cMat[device], gpumatmultconf[index].M, gpumatmultconf[index].d_wfsVec[device], 1, &cublasSgemv_beta, gpumatmultconf[index].d_dmVec[device], 1);

            if (stat != CUBLAS_STATUS_SUCCESS)
            {
                printf("cublasSgemv returned error code %d, line(%d)\n", stat, __LINE__);
                if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                    printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
                if(stat == CUBLAS_STATUS_INVALID_VALUE)
                    printf("   CUBLAS_STATUS_INVALID_VALUE\n");
                if(stat == CUBLAS_STATUS_ARCH_MISMATCH)
                    printf("   CUBLAS_STATUS_ARCH_MISMATCH\n");
                if(stat == CUBLAS_STATUS_EXECUTION_FAILED)
                    printf("   CUBLAS_STATUS_EXECUTION_FAILED\n");
                exit(EXIT_FAILURE);
            }


            if(gpumatmultconf[index].sem==1)
                sem_post(gpumatmultconf[index].semptr3[device]);

            *ptrstat = 5; // transfer result

            if(gpumatmultconf[index].sem==1)
            {
                sem_wait(gpumatmultconf[index].semptr4[device]);
                if(FORCESEMINIT==1)
                    {
                        sem_getvalue(gpumatmultconf[index].semptr4[device], &semval);
                        for(cnt=0; cnt<semval; cnt++)
                            sem_trywait(gpumatmultconf[index].semptr4[device]);
                    }
            }


            // result is on gpumatmultconf[index].d_dmVec[device]

            stat = cublasGetVector(gpumatmultconf[index].M, sizeof(float), gpumatmultconf[index].d_dmVec[device], 1, gpumatmultconf[index].dmVec_part[device], 1);
            if (stat != CUBLAS_STATUS_SUCCESS)
            {
                fprintf(stderr, "!!!! device access error (read C)\n");
                if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                    printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
                if(stat == CUBLAS_STATUS_INVALID_VALUE)
                    printf("   CUBLAS_STATUS_INVALID_VALUE\n");
                if(stat == CUBLAS_STATUS_MAPPING_ERROR)
                    printf("   CUBLAS_STATUS_MAPPING_ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
        if(gpumatmultconf[index].sem==1)
            sem_post(gpumatmultconf[index].semptr5[device]);

        *ptrstat = 6;

        // START MODE VALUES COMPUTATION HERE
        
        
        
        iter++;
    }


    pthread_exit(0);
}



//
// Computes control matrix
// Conventions:
//   n: number of actuators (= NB_MODES)
//   m: number of sensors  (= # of pixels)
// assumes m < n

int GPU_SVD_computeControlMatrix(int device, char *ID_Rmatrix_name, char *ID_Cmatrix_name, double SVDeps, char *ID_VTmatrix_name)
{
    cusolverDnHandle_t  cudenseH = NULL;
    cublasHandle_t cublasH = NULL;
    cublasStatus_t cublas_status = CUBLAS_STATUS_SUCCESS;
    cusolverStatus_t cusolver_status = CUSOLVER_STATUS_SUCCESS;
    struct cudaDeviceProp deviceProp;
    int k;

    long ID_Rmatrix, ID_Cmatrix, ID_VTmatrix;
    int atype;
    int m;
    int n;
    long *arraysizetmp;
    int lda, ldu, ldvt;
    

    float *d_A = NULL; // linear memory of GPU
    float *h_A = NULL;
    float *d_S = NULL; // linear memory of GPU
    float *d_U = NULL; // linear memory of GPU
    float *h_U1 = NULL;    
    float *d_VT = NULL; // linear memory of GPU
    float *d_M = NULL; // linear memory of GPU
    float *d_U1 = NULL; // linear memory of GPU
    float *d_Work = NULL; // linear memory of GPU
    cudaError_t cudaStat = cudaSuccess;
    int *devInfo = NULL; // info in gpu (device copy)
    int Lwork;
    float *rwork;

    float *Sarray;
    float *Aarray;
    long i;
    FILE *fp;
    char fname[200];

    int info_gpu;

    double time1sec, time2sec;
    struct timespec tnow;


    long ID_leftSV; // left singular vectors
    float val;
    long ii, jj;
    float alpha = 1.0;
    float beta = 0.0;
    long ID;
    
    float *h_M;
    long cnt0;
 

    cudaGetDeviceCount(&deviceCount);
    printf("%d devices found\n", deviceCount);
    fflush(stdout);
    printf("\n");
    for (k = 0; k < deviceCount; ++k) {
        cudaGetDeviceProperties(&deviceProp, k);
        printf("Device %d [ %20s ]  has compute capability %d.%d.\n",
               k, deviceProp.name, deviceProp.major, deviceProp.minor);
        printf("  Total amount of global memory:                 %.0f MBytes (%llu bytes)\n", (float)deviceProp.totalGlobalMem/1048576.0f, (unsigned long long) deviceProp.totalGlobalMem);
        printf("  (%2d) Multiprocessors\n", deviceProp.multiProcessorCount);
        printf("  GPU Clock rate:                                %.0f MHz (%0.2f GHz)\n", deviceProp.clockRate * 1e-3f, deviceProp.clockRate * 1e-6f);
        printf("\n");
    }


    if(device<deviceCount)
    {
        cudaSetDevice(device);
    }
    else
    {
        printf("Invalid Device : %d / %d\n", device, deviceCount);
        exit(0);
    }

cudaDeviceReset();

    printf("step 1a: create cudense handle ...");
    fflush(stdout);
    cusolver_status = cusolverDnCreate(&cudenseH);
    if (cusolver_status != CUSOLVER_STATUS_SUCCESS) {
        printf ("CUSOLVER initialization failed\n");
        return EXIT_FAILURE;
    }
    printf(" done\n");
    fflush(stdout);


    printf("step 1b: create cublas handle ...");
    fflush(stdout);
    cublas_status = cublasCreate(&cublasH);
    if (cublas_status != CUBLAS_STATUS_SUCCESS) {
        printf ("CUBLAS initialization failed\n");
        return EXIT_FAILURE;
    }
    printf(" done\n");
    fflush(stdout);





    clock_gettime(CLOCK_REALTIME, &tnow);
    time1sec = 1.0*((long) tnow.tv_sec) + 1.0e-9*tnow.tv_nsec;




    arraysizetmp = (long*) malloc(sizeof(long)*3);
    ID_Rmatrix = image_ID(ID_Rmatrix_name);

    atype = data.image[ID_Rmatrix].md[0].atype;
    if(atype!=FLOAT)
    {
        printf("wrong type\n");
        exit(0);
    }

    if(data.image[ID_Rmatrix].md[0].naxis==3)
    {
        m = data.image[ID_Rmatrix].md[0].size[0]*data.image[ID_Rmatrix].md[0].size[1];
        n = data.image[ID_Rmatrix].md[0].size[2];
        printf("3D image -> %d %d\n", m, n);
        fflush(stdout);
    }
    else
    {
        m = data.image[ID_Rmatrix].md[0].size[0];
        n = data.image[ID_Rmatrix].md[0].size[1];
        printf("2D image -> %d %d\n", m, n);
        fflush(stdout);
    }

    if(m<=n)
    {
        printf("ERROR: m must be larger than n\n");
        exit(0);
    }









    cudaStat = cudaMalloc ((void**)&d_A  , sizeof(float) * n * m);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_A returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    h_A = (float*) malloc(sizeof(float)*m*n);
    
    cudaStat = cudaMemcpy(d_A, data.image[ID_Rmatrix].array.F, sizeof(float)*m*n, cudaMemcpyHostToDevice);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy d_A returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }





    cudaStat = cudaMalloc ((void**)&d_S  , sizeof(float) * n);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_S returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    cudaStat = cudaMalloc ((void**)&d_U  , sizeof(float) * m * m);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_U returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    cudaStat = cudaMalloc ((void**)&d_VT  , sizeof(float) * n * n);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_VT returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    
    cudaStat = cudaMalloc ((void**)&devInfo, sizeof(int));
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc devInfo returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    lda = m;
    ldu = m;
    ldvt = n;
    cusolver_status = cusolverDnSgesvd_bufferSize(cudenseH, m, n, &Lwork );
    if (cusolver_status != CUSOLVER_STATUS_SUCCESS) {
        printf ("CUSOLVER DnSgesvd_bufferSize failed\n");
        return EXIT_FAILURE;
    }

    cudaStat = cudaMalloc((void**)&d_Work, sizeof(float)*Lwork);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_Work returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    
    rwork = (float*) malloc(5*sizeof(float)*n);
    
 
    printf("START GPU COMPUTATION (%d x %d)  buffer size = %d ...", m, n, Lwork);
    fflush(stdout);
    cusolverDnSgesvd (cudenseH, 'A', 'A', m, n, d_A, lda, d_S, d_U, ldu, d_VT, ldvt, d_Work, Lwork, NULL, devInfo);
    cudaStat = cudaDeviceSynchronize();
    printf(" DONE\n");
    fflush(stdout);

    cudaStat = cudaMemcpy(&info_gpu, devInfo, sizeof(int), cudaMemcpyDeviceToHost);
    printf("after gesvd: info_gpu = %d\n", info_gpu);

 
    ID_VTmatrix = create_2Dimage_ID(ID_VTmatrix_name, n, n);
    cudaStat = cudaMemcpy(data.image[ID_VTmatrix].array.F, d_VT, sizeof(float)*n*n, cudaMemcpyDeviceToHost);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
   
   save_fits(ID_VTmatrix_name, "!matVT0.fits");
  
  
     Sarray = (float*) malloc(sizeof(float)*n);
    //    Aarray = (float*) malloc(sizeof(float)*m*n);
    cudaStat = cudaMemcpy(Sarray, d_S, sizeof(float)*n, cudaMemcpyDeviceToHost);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    sprintf(fname, "eigenv.dat");
    if((fp=fopen(fname, "w"))==NULL)
    {
        printf("ERROR: cannot create file \"%s\"\n", fname);
        exit(0);
    }
    for(i=0; i<n; i++)
        fprintf(fp,"%ld %g\n", i, Sarray[i]);
    fclose(fp);



    ID = create_2Dimage_ID("matU", m, m);
    cudaMemcpy(data.image[ID].array.F, d_U, sizeof(float)*m*m, cudaMemcpyDeviceToHost);
    save_fits("matU", "!matU.fits");
  
    h_U1 = (float*) malloc(sizeof(float)*m*n);
    cudaStat = cudaMalloc((void**)&d_U1, sizeof(float)*m*n);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_U1 returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    for(ii=0;ii<m;ii++)
        for(jj=0;jj<n;jj++)
            h_U1[jj*m+ii] = data.image[ID].array.F[jj*m+ii];
    cudaMemcpy(d_U1, h_U1, sizeof(float)*m*n, cudaMemcpyHostToDevice);
    free(h_U1);
    
    ID = create_2Dimage_ID("matU1", m, n);
    cudaMemcpy(data.image[ID].array.F, d_U1, sizeof(float)*m*n, cudaMemcpyDeviceToHost);
    save_fits("matU1", "!matU1.fits");


   
   
    printf("SVDeps = %f\n", SVDeps);
    cnt0 = 0;
    // multiply lines of VT by 1/eigenval
    for(ii=0;ii<n;ii++)
    {
        if( Sarray[ii] > Sarray[0]*SVDeps )
            {
                val = 1.0/(Sarray[ii]);
                cnt0++;
            }
        else
            val = 0.0;
        
        for(jj=0;jj<n;jj++)
             data.image[ID_VTmatrix].array.F[jj*n+ii] *= val;
    }
    printf("%ld eigenvalues kept\n", cnt0);
    
    // copy VT back to GPU
   cudaStat = cudaMemcpy(d_VT, data.image[ID_VTmatrix].array.F, sizeof(float)*n*n, cudaMemcpyHostToDevice);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    
    
    cudaStat = cudaMalloc((void**)&d_M, sizeof(float)*n*m);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_M returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    
 
    save_fits(ID_VTmatrix_name, "!matVT.fits");
 
    cudaStat = cublasSgemm(cublasH, CUBLAS_OP_T, CUBLAS_OP_T, n, m, n, &alpha, d_VT, n, d_U, m, &beta, d_M, n);
     if (cudaStat != cudaSuccess)
    {
        printf("cublasSgemm returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

     



    if(data.image[ID_Rmatrix].md[0].naxis==3)
    {
        arraysizetmp[0] = data.image[ID_Rmatrix].md[0].size[0];
        arraysizetmp[1] = data.image[ID_Rmatrix].md[0].size[1];
        arraysizetmp[2] = n;
    }
    else
    {
        arraysizetmp[0] = m;
        arraysizetmp[1] = n;
    }

    
    ID_Cmatrix = create_image_ID(ID_Cmatrix_name, data.image[ID_Rmatrix].md[0].naxis, arraysizetmp, FLOAT, 0, 0);
    
    
 //   cudaStat = cudaMemcpy(data.image[ID_Cmatrix].array.F, d_M, sizeof(float)*m*n, cudaMemcpyDeviceToHost);
    
    h_M = (float*) malloc(sizeof(float)*m*n);
    cudaStat = cudaMemcpy(h_M, d_M, sizeof(float)*m*n, cudaMemcpyDeviceToHost);
    for(ii=0;ii<m;ii++)
        for(jj=0;jj<n;jj++)
            data.image[ID_Cmatrix].array.F[jj*m+ii] = h_M[ii*n+jj];
    
    //cudaStat = cudaMemcpy(data.image[ID_Cmatrix].array.F, d_VT, sizeof(float)*n*n, cudaMemcpyDeviceToHost);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    


    cudaFree(d_A);
    cudaFree(d_S);
    cudaFree(d_U);
    cudaFree(d_VT);
    cudaFree(d_Work);
    cudaFree(devInfo);
    cudaFree(d_M);
    cudaFree(d_U1);

    clock_gettime(CLOCK_REALTIME, &tnow);
    time2sec = 1.0*((long) tnow.tv_sec) + 1.0e-9*tnow.tv_nsec;

    printf("time = %8.3f s\n", 1.0*(time2sec-time1sec));




    if (cublasH ) cublasDestroy(cublasH);
    if (cudenseH) cusolverDnDestroy(cudenseH);

    cudaDeviceReset();

    free(arraysizetmp);
    free(Sarray);
    free(rwork);
    free(h_A);
    free(h_M);
    
    return(0);
}





// extract mode coefficients
int GPUextractModesLoop(char *DMact_stream, char *DMmodes, char *DMmodes_gain, char *DMmodes_val, int GPUindex)
{
    long ID_DMact;
    long ID_DMmodes;
    cublasHandle_t cublasH = NULL;
    cublasStatus_t cublas_status = CUBLAS_STATUS_SUCCESS;
    cudaError_t cudaStat = cudaSuccess;
    struct cudaDeviceProp deviceProp;
    int m, n;
    int k;

    float *d_DMmodes = NULL; // linear memory of GPU
    float *d_DMact = NULL;
    float *d_modeval = NULL;

    float alpha = 1.0;
    float beta = 0.0;


    ID_DMact = image_ID(DMact_stream);
    m = data.image[ID_DMact].md[0].size[0]*data.image[ID_DMact].md[0].size[1];

    ID_DMmodes = image_ID(DMmodes);
    n = data.image[ID_DMmodes].md[0].size[2];



    cudaGetDeviceCount(&deviceCount);
    printf("%d devices found\n", deviceCount);
    fflush(stdout);
    printf("\n");
    for (k = 0; k < deviceCount; ++k) {
        cudaGetDeviceProperties(&deviceProp, k);
        printf("Device %d [ %20s ]  has compute capability %d.%d.\n",
               k, deviceProp.name, deviceProp.major, deviceProp.minor);
        printf("  Total amount of global memory:                 %.0f MBytes (%llu bytes)\n", (float)deviceProp.totalGlobalMem/1048576.0f, (unsigned long long) deviceProp.totalGlobalMem);
        printf("  (%2d) Multiprocessors\n", deviceProp.multiProcessorCount);
        printf("  GPU Clock rate:                                %.0f MHz (%0.2f GHz)\n", deviceProp.clockRate * 1e-3f, deviceProp.clockRate * 1e-6f);
        printf("\n");
    }


    if(GPUindex<deviceCount)
        cudaSetDevice(GPUindex);
    else
    {
        printf("Invalid Device : %d / %d\n", GPUindex, deviceCount);
        exit(0);
    }


    printf("Create cublas handle ...");
    fflush(stdout);
    cublas_status = cublasCreate(&cublasH);
    if (cublas_status != CUBLAS_STATUS_SUCCESS) {
        printf ("CUBLAS initialization failed\n");
        return EXIT_FAILURE;
    }
    printf(" done\n");
    fflush(stdout);


    // load DMmodes to GPU
    cudaStat = cudaMalloc((void**)&d_DMmodes, sizeof(float)*m*n);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_DMmodes returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }
    cudaStat = cudaMemcpy(d_DMmodes, data.image[ID_DMmodes].array.F, sizeof(float)*m*n, cudaMemcpyHostToDevice);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }


    // create d_DMact
    cudaStat = cudaMalloc((void**)&d_DMact, sizeof(float)*m);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_DMact returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }

    // create d_modeval
    cudaStat = cudaMalloc((void**)&d_modeval, sizeof(float)*n);
    if (cudaStat != cudaSuccess)
    {
        printf("cudaMalloc d_modeval returned error code %d, line(%d)\n", cudaStat, __LINE__);
        exit(EXIT_FAILURE);
    }




    if(1==1)
    {
        // load DMact to GPU
        cudaStat = cudaMemcpy(d_DMact, data.image[ID_DMact].array.F, sizeof(float)*m, cudaMemcpyHostToDevice);
        if (cudaStat != cudaSuccess)
        {
            printf("cudaMemcpy returned error code %d, line(%d)\n", cudaStat, __LINE__);
            exit(EXIT_FAILURE);
        }

        // compute
        cublas_status = cublasSgemv(cublasH, CUBLAS_OP_T, m, n, &alpha, d_DMmodes, m, d_DMact, 1, &beta, d_modeval, 1);
        if (cudaStat != CUBLAS_STATUS_SUCCESS)
        {
            printf("cublasSgemv returned error code %d, line(%d)\n", stat, __LINE__);
            if(stat == CUBLAS_STATUS_NOT_INITIALIZED)
                printf("   CUBLAS_STATUS_NOT_INITIALIZED\n");
            if(stat == CUBLAS_STATUS_INVALID_VALUE)
                printf("   CUBLAS_STATUS_INVALID_VALUE\n");
            if(stat == CUBLAS_STATUS_ARCH_MISMATCH)
                printf("   CUBLAS_STATUS_ARCH_MISMATCH\n");
            if(stat == CUBLAS_STATUS_EXECUTION_FAILED)
                printf("   CUBLAS_STATUS_EXECUTION_FAILED\n");
            exit(EXIT_FAILURE);
        }
    }


    cudaFree(d_DMmodes);
    cudaFree(d_DMact);
    cudaFree(d_modeval);

    if (cublasH ) cublasDestroy(cublasH);



    return(0);
}




 


int GPUcomp_test(long NBact, long NBmodes, long WFSsize, long GPUcnt)
{
    long ID_contrM;
    long ID_WFS;
    long ID_cmd_modes;
    long *cmsize;
    long *wfssize;
    long *cmdmodessize;
    int status;
    int GPUstatus[100];
    long iter;
    long NBiter = 50000;
    double time1sec, time2sec;
    struct timespec tnow;
    int *GPUdevices;
    int k;
    double SVDeps = 0.1;

    long n, m;
    long *arraysizetmp;
    long ID, ID_R, ID_C;
    long ii, jj;
    float val;

    if(1==1)
    {
    //printf("Testing SVD on CPU\n");
      // linopt_compute_reconstructionMatrix("Rmat", "Cmat", SVDeps, "VTmat");
    
       printf("Testing SVD on GPU\n");
       GPU_SVD_computeControlMatrix(1, "Rmat", "Cmat", SVDeps, "VTmat");
       
       // CHECK RESULT
        arraysizetmp = (long*) malloc(sizeof(long)*3);
        ID_R = image_ID("Rmat");
        ID_C = image_ID("Cmat");

        if(data.image[ID_R].md[0].naxis==3)
        {
            m = data.image[ID_R].md[0].size[0]*data.image[ID_R].md[0].size[1];
            n = data.image[ID_R].md[0].size[2];
            printf("3D image -> %ld %ld\n", m, n);
            fflush(stdout);
        }
        else
        {
            m = data.image[ID_R].md[0].size[0];
            n = data.image[ID_R].md[0].size[1];
            printf("2D image -> %ld %ld\n", m, n);
            fflush(stdout);
        }
        
        printf("CHECKING RESULT ... ");
        fflush(stdout);
        
        ID = create_2Dimage_ID("SVDcheck", n, n);
        for(ii=0;ii<n;ii++)
            for(jj=0;jj<n;jj++)
                {
                    val = 0.0;
                    for(k=0;k<m;k++)
                        val += data.image[ID_C].array.F[ii*m+k] * data.image[ID_R].array.F[jj*m+k];
                    data.image[ID].array.F[jj*n+ii] = val;
                }
        save_fits("SVDcheck", "!SVDcheck.fits");
        printf("DONE\n");
        fflush(stdout);
    }
    else
    {
        printf("Testing GPU matrix multiplication speed, %ld GPUs\n", GPUcnt);


        GPUdevices = (int*) malloc(sizeof(int)*GPUcnt);
        for(k=0; k<GPUcnt; k++)
            GPUdevices[k] = k+8;

        //    GPUstatus = (int*) malloc(sizeof(int)*100);

        cmsize = (long*) malloc(sizeof(long)*3);
        cmsize[0] = WFSsize;
        cmsize[1] = WFSsize;
        cmsize[2] = NBmodes;
        ID_contrM = create_image_ID("cudatestcm", 3, cmsize, FLOAT, 1, 0);

        wfssize = (long*) malloc(sizeof(long)*2);
        wfssize[0] = WFSsize;
        wfssize[1] = WFSsize;
        ID_WFS = create_image_ID("cudatestwfs", 2, wfssize, FLOAT, 1, 0);

        cmdmodessize = (long*) malloc(sizeof(long)*2);
        cmdmodessize[0] = NBmodes;
        cmdmodessize[1] = 1;
        ID_cmd_modes = create_image_ID("cudatestcmd", 2, cmdmodessize, FLOAT, 1, 0);

        GPU_loop_MultMat_setup(0, data.image[ID_contrM].name, data.image[ID_WFS].name, data.image[ID_cmd_modes].name, GPUcnt, GPUdevices, 0, 1, 1, 0);

        clock_gettime(CLOCK_REALTIME, &tnow);
        time1sec = 1.0*((long) tnow.tv_sec) + 1.0e-9*tnow.tv_nsec;

        for(iter=0; iter<NBiter; iter++)
        {
            status = 0;
            GPU_loop_MultMat_execute(0, &status, &GPUstatus[0], 1.0, 0.0);
        }
        clock_gettime(CLOCK_REALTIME, &tnow);
        time2sec = 1.0*((long) tnow.tv_sec) + 1.0e-9*tnow.tv_nsec;

        printf("Frequ = %12.3f Hz\n", 1.0*NBiter/(time2sec-time1sec));

        printf("done\n");
        fflush(stdout);

        delete_image_ID("cudatestcm");
        delete_image_ID("cudatestwfs");
        delete_image_ID("cudatestcmd");

        free(cmsize);
        free(wfssize);
        free(cmdmodessize);
        free(GPUdevices);
    }

    return(0);
}



#endif










