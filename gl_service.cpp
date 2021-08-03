#include "gl_service.h"

#include "NX_Queue.h"
#include "NX_Semaphore.h"

#include <math.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <linux/videodev2.h>

//	opengl library
#include <nx_gpu_surf.h>
#include <nx_gl_tools.h>

//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//							Define Macros									//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
//	Debugging Macrhos

#define	DEBUG_API		(0)

#ifdef ANDROID
#define LOG_TAG "NX_GL_TOOLS"	//	log options
#include <utils/Log.h>
#define NX_DBG			ALOGD
#define NX_ERR			ALOGE
#else
#define NX_DBG			printf
#define NX_ERR			printf
#endif
//	Define image alignement related Macros
#define APP_PLATFORM_Y_ALIGN_SIZE  16
#define APP_PLATFORM_X_ALIGN_SIZE  32

#ifndef ALIGN
#define  ALIGN(X,N) ( ((X) + (N - 1)) & (~((N) - 1)) )
#endif

//	Maximum in/out buffer for instance
#define MAX_BUFFER_NUM		31

//	Maximum Number of Commands
#define MAX_CMD_SIZE	(4)

//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//			Define Types for GL Service & Instance Management				//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////

//	Service Related Structure
typedef struct tag_GL_SERVICE_INFO {
	pthread_t 		hThread;
	pthread_mutex_t	hServiceLock;
	int 			isThreadRun;
	NX_QUEUE_HANDLE	hCmdQueue;
	NX_SEM_HANDLE	hSemCmd;
	NX_SEM_HANDLE	hSemResult;
} GL_SERVICE_INFO;

typedef struct tag_GL_SERVICE_CMD {
	void			*handle;
	int				id;
	int				cmd;
	void			*data;
	// Output
	int				retCode;
} GL_SERVICE_CMD;

typedef struct tag_NX_GT_SERVICE {
	void*	(*GLOpen)	( GL_OPEN_PARAM   *param );
	int		(*GLRun)	( GL_RUN_PARAM    *param );
	void	(*GLClose)	( GL_CLOSE_PARAM  *param );
	int		(*GLMotion)	( GL_MOTION_PARAM *param );
} NX_GT_SERVICE;


//	Instance Handle
typedef struct
{
	HGSURFCTRL 			ghAppGSurfCtrl;
	HGSURFSOURCE 		ghAppGSurfSource[MAX_BUFFER_NUM];
	HGSURFTARGET 		ghAppGSurfTarget[MAX_BUFFER_NUM];
	HGSURFMOTIONCTRL	ghAppGSurfMotion;
	int					srcImageFormat;
	int					dstImageFormat;
	int 				srcDmaFd[MAX_BUFFER_NUM];
	int 				srcDmaBufNum;
	int 				dstDmaFd[MAX_BUFFER_NUM];
	int 				dstDmaBufNum;
	unsigned int		srcWidth;
	unsigned int 		srcHeight;
	unsigned int 		dstWidth;
	unsigned int 		dstHeight;
	int					dstOutBufNum;
	int					mode;
	int					motionFds[2];
	float				coeff;
} NX_GL_HANDLE;


//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//				Declare Instance Management Variables						//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static int gst_IsInitService = 0;
static int gst_NumInstance = 0;
static pthread_mutex_t gst_SvcCtrlLock = PTHREAD_MUTEX_INITIALIZER;
static GL_SERVICE_INFO gst_SvcInfo;
static NX_GT_SERVICE gst_ServiceList[MAX_SERVICE_NUM];


//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//						Local Utility Functions								//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static int FindSrcDmaFdIndex(NX_GL_HANDLE *hGl, int srcDmaFd)
{
	int i=0;

	for( i = 0 ; i < hGl->srcDmaBufNum ; i ++ )
	{
		if( hGl->srcDmaFd[i] == srcDmaFd )
		{
			//	already added memory
			return i;
		}
	}
	return -1;
}

static int FindSrcDmaFdIndexWithCreate(NX_GL_HANDLE *hGl, int *pDmaFds )
{
	int i=0;
	for( i = 0 ; i < hGl->srcDmaBufNum ; i ++ )
	{
		if( hGl->srcDmaFd[i] == pDmaFds[0] )
		{
			return i;
		}
	}

	//create GSurf source handle
	if(hGl->dstImageFormat == V4L2_PIX_FMT_YUV420)		//	Single Plane
	{
		hGl->ghAppGSurfSource[hGl->srcDmaBufNum] = 
			nxGSurfaceCreateDeinterlaceSource(hGl->ghAppGSurfCtrl, hGl->srcWidth, hGl->srcHeight, pDmaFds[0]);
	}
	else if(hGl->dstImageFormat == V4L2_PIX_FMT_YUV420M)	//	Multiple Plane
	{
		hGl->ghAppGSurfSource[hGl->srcDmaBufNum] = 
			nxGSurfaceCreateDeinterlaceSourceWithFDs(hGl->ghAppGSurfCtrl, hGl->srcWidth, hGl->srcHeight, pDmaFds);
	}
	else
	{
		return -1;
	}

	hGl->srcDmaFd[hGl->srcDmaBufNum] = pDmaFds[0];
	i = hGl->srcDmaBufNum;
	hGl->srcDmaBufNum++;
	return i;
}

static unsigned int vmem_get_size(unsigned int width, unsigned int height,
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE image_format, unsigned int width_align, unsigned int height_align)
{
	unsigned int bytes_size = 0;

	width = (width_align)? NX_ALIGN(width, width_align) : width;
	if (NX_GSURF_VMEM_IMAGE_FORMAT_RGBA == image_format)
	{
		bytes_size = width * 4 * height;
	}
	else if (NX_GSURF_VMEM_IMAGE_FORMAT_YUV422 == image_format)
	{
		bytes_size = width * height * 2;
	}
	else if (NX_GSURF_VMEM_IMAGE_FORMAT_NV12 == image_format ||
	    NX_GSURF_VMEM_IMAGE_FORMAT_NV21 == image_format)
	{
		bytes_size = (width * height * 3) / 2;
	}
	else if (NX_GSURF_VMEM_IMAGE_FORMAT_YUV420 == image_format ||
		NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE == image_format)
	{
		unsigned int y_height, u_height;
		unsigned int y_align_blank_height = 0;
		unsigned int u_align_blank_height = 0;
		bytes_size = width * (height + height/2);
		if (height_align)
		{
			y_height = height;
			if (y_height % height_align)
			{
				y_align_blank_height = (height_align - (y_height % height_align));
			}
			u_height = height/2;
			if (u_height % height_align)
			{
				u_align_blank_height = (height_align - (u_height % height_align)) / 2;
			}
			//re-calculate size
			bytes_size = width * (height + height/2 + y_align_blank_height + u_align_blank_height);
		}
	}
	else if (NX_GSURF_VMEM_IMAGE_FORMAT_Y == image_format)
	{
		unsigned int y_height;
		bytes_size = width * height;
		y_height = height;
		if (height_align && y_height % height_align)
		{
			unsigned int y_align_blank_height;
			y_align_blank_height = (height_align - (y_height % height_align));
			//re-calculate size
			bytes_size = width * (height + y_align_blank_height);
		}
	}
	else
	{
		NX_ERR("mem_get_size() unvalid format(%d)!\n", image_format);
		return 0;
	}
	return bytes_size;
}
//
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//								Deinterlace									//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static void *GLServiceDeinterlaceOpen(GL_OPEN_PARAM  *param)
{
	int32_t bRet;

	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_MOTION_INTERLACE;
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_MOTION_INTERLACE;

	NX_GL_HANDLE *hDeint = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));

	memset(hDeint, 0x00, sizeof(NX_GL_HANDLE) );
	for(int i = 0; i < MAX_BUFFER_NUM; i++)
	{
		hDeint->srcDmaFd[i] = -1;
		hDeint->dstDmaFd[i] = -1;
	}
	hDeint->srcImageFormat = param->srcImageFormat;
	hDeint->dstImageFormat = param->srcImageFormat;
	hDeint->dstOutBufNum   = param->dstOutBufNum;
	hDeint->srcWidth       = param->srcWidth;
	hDeint->srcHeight      = param->srcHeight;
	hDeint->dstWidth       = param->dstWidth;
	hDeint->dstHeight      = param->dstHeight;
	hDeint->mode           = param->mode;
	hDeint->motionFds[0]   = param->motionFds[0];
	hDeint->motionFds[1]   = param->motionFds[1];
	hDeint->coeff          = param->coeff;

	if( (param->srcImageFormat != V4L2_PIX_FMT_YUV420) && (param->srcImageFormat != V4L2_PIX_FMT_YUV420M) )
	{
		NX_ERR("Not Support imageFormat !!!\n");
		return NULL;
	}

	//	create GSurf handle
	hDeint->ghAppGSurfCtrl = nxGSurfaceCreate(hDeint->dstOutBufNum, NX_FALSE,
						0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(hDeint->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	if( hDeint->mode == DEINT_MODE_ADAPTIVE )
	{
		glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_MOTION_INTERLACE;
		glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_MOTION_INTERLACE;
	}
	else
	{
		glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;
		glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;
	}

	//	init GSurf EGL
	bRet = nxGSurfaceInitEGL(hDeint->ghAppGSurfCtrl, NULL,
	      glSrcFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      glDstFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);
	if(bRet == false)
	{
		return NULL;
	}

	//	create target surface
	for (int i = 0 ; i < hDeint->dstOutBufNum ; i++)
	{
		//	create GSurf target handle
		if(hDeint->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			hDeint->ghAppGSurfTarget[i] = 
				nxGSurfaceCreateDeinterlaceTarget(hDeint->ghAppGSurfCtrl, hDeint->dstWidth, hDeint->dstHeight, param->pDstDmaFd[i][0]);
		}
		else if(hDeint->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			hDeint->ghAppGSurfTarget[i] = 
				nxGSurfaceCreateDeinterlaceTargetWithFDs(hDeint->ghAppGSurfCtrl, hDeint->dstWidth, hDeint->dstHeight, param->pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		hDeint->dstDmaFd[i] = param->pDstDmaFd[i][0];
		hDeint->dstDmaBufNum++;
	}

	//	init GSurf GL
	bRet = nxGSurfaceInitGL(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfTarget[0]);

	if(bRet == false)
	{
		return NULL;
	}

	if( hDeint->mode == DEINT_MODE_ADAPTIVE )
	{
		hDeint->ghAppGSurfMotion = 
			nxGSurfaceCreateMotionData(hDeint->ghAppGSurfCtrl, hDeint->srcWidth, hDeint->srcHeight, hDeint->motionFds[0], hDeint->motionFds[1]);

		//	Ready Deinterlace
		nxGSurfaceReadyDeinterlace(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfMotion);
	}

	return (void *)hDeint;
}

static int GLServiceDeinterlaceMakeMotion ( GL_MOTION_PARAM *param )
{
	NX_GL_HANDLE *hDeint = (NX_GL_HANDLE *)param->pHandle;

	if( (NULL == param) || (NULL == param->pHandle) )
	{
		NX_ERR("Handle is NULL !\n");
		return -1; 
	}
	int idxCurr = FindSrcDmaFdIndexWithCreate( hDeint, param->pSrcDmaFds );
	int idxNext = FindSrcDmaFdIndexWithCreate( hDeint, param->pSrcNDmaFds );
	HGSURFSOURCE hSource = hDeint->ghAppGSurfSource[idxCurr];
	HGSURFSOURCE hSourceN = hDeint->ghAppGSurfSource[idxNext];
	bool ret = nxGSurfaceRenderMotionData(hDeint->ghAppGSurfCtrl, hSource, hSourceN, hDeint->ghAppGSurfMotion );
	return ret ? 0 : -1;
}

static int   GLServiceDeinterlaceRun  ( GL_RUN_PARAM  *param )
{
	NX_GL_HANDLE *hDeint = (NX_GL_HANDLE *)param->pHandle;
	int bRet, i;
	int idxCurr, idxNext;

	HGSURFSOURCE hSource = NULL;
	HGSURFSOURCE hSourceN = NULL;
	HGSURFTARGET hTarget = NULL;

	if( !hDeint )
	{
		NX_ERR("pHandle is NULL !\n");
		return -1; 
	}

	// find target
	for(i = 0; i < hDeint->dstDmaBufNum; i++)
	{
		if(hDeint->dstDmaFd[i] == param->pDstDmaFd[0])
		{
			hTarget = hDeint->ghAppGSurfTarget[i];
			break;
		}
	}

	//	Find Current Buffers' Surface
	idxCurr = FindSrcDmaFdIndexWithCreate( hDeint, param->pSrcDmaFd );
	hSource = hDeint->ghAppGSurfSource[idxCurr];

	//	Find Next DMA Buffer's index
	if( param->pSrcNDmaFd )
	{
		//	Odd Even Odd
		idxNext = FindSrcDmaFdIndexWithCreate( hDeint, param->pSrcNDmaFd );
		hSourceN = hDeint->ghAppGSurfSource[idxNext];
	}

	if( hDeint->mode == DEINT_MODE_ADAPTIVE )
	{
		bRet = nxGSurfaceRenderMotionDeinterlace(
			hDeint->ghAppGSurfCtrl,
			hSource, hSourceN, hDeint->ghAppGSurfMotion, hTarget,
			hDeint->coeff, hDeint->coeff);
	}
	else
	{
		bRet = nxGSurfaceRenderDeinterlace(hDeint->ghAppGSurfCtrl, hSource, hTarget); 
	}
	if(bRet == false)
	{
		return -1;
	}		
	nxGSurfaceUpdate(hDeint->ghAppGSurfCtrl);
	return 0;	
}

static void GLServiceDeinterlaceClose ( GL_CLOSE_PARAM *param )
{
	int i;
	NX_GL_HANDLE *hDeint = (NX_GL_HANDLE *)param->pHandle;

	if( (NULL == param) || (NULL == param->pHandle) )
	{
		NX_ERR("Handle is NULL !\n");
		return; 
	}

	hDeint = (NX_GL_HANDLE *)param->pHandle;

	if( hDeint->mode == DEINT_MODE_ADAPTIVE )
	{
		nxGSurfaceReleaseDeinterlace(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfMotion);
	}

	//destroy GSurf source handle
	for (i = 0 ; i < hDeint->srcDmaBufNum ; i++)
	{

		nxGSurfaceDestroySource(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfSource[i]);
		hDeint->ghAppGSurfSource[i] = NULL;
	}

	//deinit GSurf GL
	nxGSurfaceDeinitGL(hDeint->ghAppGSurfCtrl);

	//destroy target	
	for (i = 0 ; i < hDeint->dstOutBufNum ; i++)
	{
		//destroy GSurf target handle
		nxGSurfaceDestroyTarget(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfTarget[i]);
		hDeint->ghAppGSurfTarget[i] = NULL;
	}

	if( hDeint->mode == DEINT_MODE_ADAPTIVE )
	{
		nxGSurfaceDestroyMotionData(hDeint->ghAppGSurfCtrl, hDeint->ghAppGSurfMotion);
	}

	//deinit GSurf EGL
	nxGSurfaceDeinitEGL(hDeint->ghAppGSurfCtrl);

	//destroy GSurf handle
	nxGSurfaceDestroy(hDeint->ghAppGSurfCtrl);
	hDeint->ghAppGSurfCtrl = NULL;

	if(hDeint)
	{
		free(hDeint);
	}
}

//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//								Rotate										//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static void* GLServiceRotateOpen ( GL_OPEN_PARAM  *param )
{
	NX_GL_HANDLE *hRotate = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));
	memset(hRotate, 0x00, sizeof(NX_GL_HANDLE) );

	for(int i = 0; i < MAX_BUFFER_NUM; i++)
	{
		hRotate->srcDmaFd[i] = -1;
		hRotate->dstDmaFd[i] = -1;
	}

	if( (param->srcImageFormat != V4L2_PIX_FMT_YUV420) &&
	    (param->srcImageFormat != V4L2_PIX_FMT_YUV420M) )
	{
		NX_ERR("Not Support imageFormat !!!\n");
		return NULL;
	}

	hRotate->srcImageFormat = param->srcImageFormat;
	hRotate->dstImageFormat = param->srcImageFormat;
	hRotate->dstOutBufNum   = param->dstOutBufNum;
	hRotate->srcWidth       = param->srcWidth;
	hRotate->srcHeight      = param->srcHeight;
	hRotate->dstWidth       = param->dstWidth;
	hRotate->dstHeight      = param->dstHeight;


	//create GSurf handle
	hRotate->ghAppGSurfCtrl = nxGSurfaceCreate(param->dstOutBufNum, NX_FALSE, 0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(hRotate->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	//init GSurf EGL
	nxGSurfaceInitEGL(hRotate->ghAppGSurfCtrl, NULL,
	      NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);

	//create target
	for (int i = 0 ; i < param->dstOutBufNum ; i++)
	{
		//create GSurf target handle
		if(hRotate->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			hRotate->ghAppGSurfTarget[i] = nxGSurfaceCreateTarget(hRotate->ghAppGSurfCtrl, hRotate->dstWidth, hRotate->dstHeight, param->pDstDmaFd[i][0]);
		}
		else if(hRotate->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			hRotate->ghAppGSurfTarget[i] = nxGSurfaceCreateTargetWithFDs(hRotate->ghAppGSurfCtrl, hRotate->dstWidth, hRotate->dstHeight, param->pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		hRotate->dstDmaFd[i] = param->pDstDmaFd[i][0];
		hRotate->dstDmaBufNum++;
	}

	//init GSurf GL
	nxGSurfaceInitGL(hRotate->ghAppGSurfCtrl, hRotate->ghAppGSurfTarget[0]);

	return (void *)hRotate;
}

static int   GLServiceRotateRun  ( GL_RUN_PARAM  *param )
{
	int srcDmaFd, index;
	NX_GL_HANDLE *hRotate;
	HGSURFSOURCE hsource = NULL;
	HGSURFTARGET htarget = NULL;

	if( (NULL==param) || (NULL==param->pHandle) )
	{
		NX_ERR("pHandle is NULL !\n");
		return -1;
	}
	hRotate = (NX_GL_HANDLE *)param->pHandle;

	srcDmaFd = param->pSrcDmaFd[0];

	// find target
	for(int i = 0; i < hRotate->dstDmaBufNum; i++)
	{
		if(hRotate->dstDmaFd[i] == param->pDstDmaFd[0])
		{
			htarget = hRotate->ghAppGSurfTarget[i];
			break;
		}
	}

	index = FindSrcDmaFdIndex(hRotate, srcDmaFd );
	if( 0 > index )
	{
		//create GSurf source handle
		if(hRotate->dstImageFormat == V4L2_PIX_FMT_YUV420)
		{
			hRotate->ghAppGSurfSource[hRotate->srcDmaBufNum] = nxGSurfaceCreateSource(hRotate->ghAppGSurfCtrl, hRotate->srcWidth, hRotate->srcHeight, param->pSrcDmaFd[0]);
		}
		else if(hRotate->dstImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			hRotate->ghAppGSurfSource[hRotate->srcDmaBufNum] = nxGSurfaceCreateSourceWithFDs(hRotate->ghAppGSurfCtrl, hRotate->srcWidth, hRotate->srcHeight, param->pSrcDmaFd);
		}
		else
		{
			return -1;
		}
		hRotate->srcDmaFd[hRotate->srcDmaBufNum] = srcDmaFd;
		index = hRotate->srcDmaBufNum;
		hRotate->srcDmaBufNum++;
	}

	hsource = hRotate->ghAppGSurfSource[index];
	nxGSurfaceRender(hRotate->ghAppGSurfCtrl, hsource, htarget, (NX_GSURF_ROTATE_MODE)param->option);
	nxGSurfaceUpdate(hRotate->ghAppGSurfCtrl);
	return 0;
}


static void  GLServiceRotateClose( GL_CLOSE_PARAM *param )
{
	NX_GL_HANDLE *hRotate;
	int i;
	if( (NULL==param) || (NULL==param->pHandle) )
	{
		NX_ERR("pHandle is NULL !\n");
		return;
	}

	hRotate = (NX_GL_HANDLE *)param->pHandle;

	//destroy GSurf source handle
	for (i=0 ; i < hRotate->srcDmaBufNum ; i++)
	{
		nxGSurfaceDestroySource(hRotate->ghAppGSurfCtrl, hRotate->ghAppGSurfSource[i]);
		hRotate->ghAppGSurfSource[i] = NULL;
	}

	//deinit GSurf GL
	nxGSurfaceDeinitGL(hRotate->ghAppGSurfCtrl);

	//destroy target	
	for (i=0 ; i < hRotate->dstOutBufNum ; i++)
	{
		//destroy GSurf target handle
		nxGSurfaceDestroyTarget(hRotate->ghAppGSurfCtrl, hRotate->ghAppGSurfTarget[i]);
		hRotate->ghAppGSurfTarget[i] = NULL;
	}

	//deinit GSurf EGL
	nxGSurfaceDeinitEGL(hRotate->ghAppGSurfCtrl);

	//destroy GSurf handle
	nxGSurfaceDestroy(hRotate->ghAppGSurfCtrl);
	hRotate->ghAppGSurfCtrl = NULL;
	if(hRotate)
	{
		free(hRotate);
	}
}
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//							Copy Buffer										//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static void* GLServiceCopyBufferOpen ( GL_OPEN_PARAM  *param )
{
	int ret = 0, i;
	unsigned int renderWidth, renderHeight, imgLength;;

	NX_GL_HANDLE *hMemCpy = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));
	memset(hMemCpy, 0x00, sizeof(NX_GL_HANDLE) );

	if( (param->srcImageFormat != V4L2_PIX_FMT_YUV420) && 
		(param->srcImageFormat != V4L2_PIX_FMT_YUV420M) )
	{
		NX_ERR("Not Support imageFormat !!!\n");
		free( hMemCpy );
		return NULL;
	}

	for(i = 0; i < MAX_BUFFER_NUM; i++)
	{
		hMemCpy->srcDmaFd[i] = -1;
		hMemCpy->dstDmaFd[i] = -1;
	}

	hMemCpy->srcImageFormat = param->srcImageFormat;
	hMemCpy->dstImageFormat = param->srcImageFormat;
	hMemCpy->dstOutBufNum   = param->dstOutBufNum;
	hMemCpy->srcWidth       = param->srcWidth;
	hMemCpy->srcHeight      = param->srcHeight;
	hMemCpy->dstWidth       = param->dstWidth;
	hMemCpy->dstHeight      = param->dstHeight;


	//	create GSurf handle
	hMemCpy->ghAppGSurfCtrl = nxGSurfaceCreate(param->dstOutBufNum, NX_FALSE, 0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(hMemCpy->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	//	init GSurf EGL
	ret = nxGSurfaceInitEGL(hMemCpy->ghAppGSurfCtrl, NULL,
	      NX_GSURF_VMEM_IMAGE_FORMAT_RGBA, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      NX_GSURF_VMEM_IMAGE_FORMAT_RGBA, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);


	//	create target
	imgLength = vmem_get_size(hMemCpy->dstWidth, hMemCpy->dstHeight, NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, 32, 16);
	renderWidth = hMemCpy->dstWidth / 4;
	renderHeight = imgLength / hMemCpy->dstWidth;
	for (i = 0 ; i < param->dstOutBufNum ; i++)
	{
		//create GSurf target handle
		if(hMemCpy->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			hMemCpy->ghAppGSurfTarget[i] = nxGSurfaceCreateTarget(hMemCpy->ghAppGSurfCtrl, renderWidth, renderHeight, param->pDstDmaFd[i][0]);
		}
		else if(hMemCpy->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			hMemCpy->ghAppGSurfTarget[i] = nxGSurfaceCreateTargetWithFDs(hMemCpy->ghAppGSurfCtrl, renderWidth, renderHeight, param->pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		hMemCpy->dstDmaFd[i] = param->pDstDmaFd[i][0];
		hMemCpy->dstDmaBufNum++;
	}

	//	init GSurf GL
	ret = nxGSurfaceInitGL(hMemCpy->ghAppGSurfCtrl, hMemCpy->ghAppGSurfTarget[0]);
	return (void *)hMemCpy;
}


static int   GLServiceCopyBufferRun  ( GL_RUN_PARAM *param )
{
	int i, index;
	int srcDmaFd;
	int rotateMode = 0;
	unsigned int imgLength, renderWidth, renderHeight;
	NX_GL_HANDLE *hMemCpy;
	HGSURFSOURCE hsource = NULL;
	HGSURFTARGET htarget = NULL;

	if( (NULL==param) || (NULL==param->pHandle) )
	{
		NX_ERR("pHandle is NULL !\n");
		return -1; 
	}

	hMemCpy = (NX_GL_HANDLE *)param->pHandle;
	srcDmaFd = param->pSrcDmaFd[0];

	//	find target
	for(i = 0; i < hMemCpy->dstDmaBufNum; i++)
	{
		if(hMemCpy->dstDmaFd[i] == param->pDstDmaFd[0])
		{
			htarget = hMemCpy->ghAppGSurfTarget[i];
			break;
		}
	}

	index = FindSrcDmaFdIndex(hMemCpy, srcDmaFd );
	if( 0 > index )
	{
		//	create GSurf source handle
		imgLength = vmem_get_size(hMemCpy->srcWidth, hMemCpy->srcHeight, NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, 32, 16);
		renderWidth = hMemCpy->srcWidth / 4;
		renderHeight = imgLength / hMemCpy->srcWidth;

		if(hMemCpy->dstImageFormat == V4L2_PIX_FMT_YUV420)
		{
			hMemCpy->ghAppGSurfSource[hMemCpy->srcDmaBufNum] = nxGSurfaceCreateSource(hMemCpy->ghAppGSurfCtrl, renderWidth, renderHeight, param->pSrcDmaFd[0]);
		}
		else if(hMemCpy->dstImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			hMemCpy->ghAppGSurfSource[hMemCpy->srcDmaBufNum] = nxGSurfaceCreateSourceWithFDs(hMemCpy->ghAppGSurfCtrl, renderWidth, renderHeight, param->pSrcDmaFd);
		}
		else
		{
			return -1;
		}
		hMemCpy->srcDmaFd[hMemCpy->srcDmaBufNum] = srcDmaFd;
		index = hMemCpy->srcDmaBufNum;
		hMemCpy->srcDmaBufNum++;
	}

	hsource = hMemCpy->ghAppGSurfSource[index];
	nxGSurfaceRender(hMemCpy->ghAppGSurfCtrl, hsource, htarget, (NX_GSURF_ROTATE_MODE)0);
	nxGSurfaceUpdate(hMemCpy->ghAppGSurfCtrl);
	return 0;
}

static void  GLServiceCopyBufferClose( GL_CLOSE_PARAM *param )
{
	int i;
	NX_GL_HANDLE *hMemCpy;
	if( NULL==param || NULL==param->pHandle )
	{
		NX_ERR("pHandle is NULL !\n");
		return; 
	}

	hMemCpy = (NX_GL_HANDLE *)param->pHandle;

	//	destroy GSurf source handle
	for (i = 0 ; i < hMemCpy->srcDmaBufNum ; i++)
	{
		nxGSurfaceDestroySource(hMemCpy->ghAppGSurfCtrl, hMemCpy->ghAppGSurfSource[i]);
		hMemCpy->ghAppGSurfSource[i] = NULL;
	}

	//	deinit GSurf GL
	nxGSurfaceDeinitGL(hMemCpy->ghAppGSurfCtrl);

	//	destroy target	
	for (i = 0 ; i < hMemCpy->dstOutBufNum ; i++)
	{
		//	destroy GSurf target handle
		nxGSurfaceDestroyTarget(hMemCpy->ghAppGSurfCtrl, hMemCpy->ghAppGSurfTarget[i]);
		hMemCpy->ghAppGSurfTarget[i] = NULL;
	}

	//	deinit GSurf EGL
	nxGSurfaceDeinitEGL(hMemCpy->ghAppGSurfCtrl);

	//	destroy GSurf handle
	nxGSurfaceDestroy(hMemCpy->ghAppGSurfCtrl);
	hMemCpy->ghAppGSurfCtrl = NULL;
	if(hMemCpy)
	{
		free(hMemCpy);
	}
}
//
//////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////
//																			//
//																			//
//							Service Management								//
//																			//
//																			//
//////////////////////////////////////////////////////////////////////////////
static void RegisterServices()
{
	gst_ServiceList[SERVICE_ID_DEINTERLACE].GLOpen   = GLServiceDeinterlaceOpen;
	gst_ServiceList[SERVICE_ID_DEINTERLACE].GLRun    = GLServiceDeinterlaceRun;
	gst_ServiceList[SERVICE_ID_DEINTERLACE].GLClose  = GLServiceDeinterlaceClose;
	gst_ServiceList[SERVICE_ID_DEINTERLACE].GLMotion = GLServiceDeinterlaceMakeMotion;
	gst_ServiceList[SERVICE_ID_COPY       ].GLOpen   = GLServiceRotateOpen;
	gst_ServiceList[SERVICE_ID_COPY       ].GLRun    = GLServiceRotateRun;
	gst_ServiceList[SERVICE_ID_COPY       ].GLClose  = GLServiceRotateClose;
	gst_ServiceList[SERVICE_ID_ROTATE     ].GLOpen   = GLServiceCopyBufferOpen;
	gst_ServiceList[SERVICE_ID_ROTATE     ].GLRun    = GLServiceCopyBufferRun;
	gst_ServiceList[SERVICE_ID_ROTATE     ].GLClose  = GLServiceCopyBufferClose;
}


void *GLServiceThread( void *arg )
{
	int32_t retCode = 0;
	GL_SERVICE_CMD *pCmd;
	(void) arg;

#if DEBUG_API
	NX_DBG("GLServiceThread Started!!\n");
#endif

	//	Register GL Services
	RegisterServices();

	while( gst_SvcInfo.isThreadRun )
	{
		pCmd = NULL;

		if( !NX_QueueGetCount(gst_SvcInfo.hCmdQueue ) )
			NX_SemaporePend( gst_SvcInfo.hSemCmd );
		
		NX_QueuePop( gst_SvcInfo.hCmdQueue, (void**)&pCmd );
		
		// Command Parser..
		if( !pCmd ) continue;
		
		switch( pCmd->cmd )
		{
		case SERVICE_CMD_OPEN :
			//	return handle for each instance
			pCmd->handle = gst_ServiceList[pCmd->id].GLOpen( (GL_OPEN_PARAM*)pCmd->data );
			if( pCmd->handle == NULL )
				pCmd->retCode = -1;
			else
				pCmd->retCode = 0;
			break;
		case SERVICE_CMD_RUN :
			retCode = gst_ServiceList[pCmd->id].GLRun( (GL_RUN_PARAM*)pCmd->data );
			pCmd->retCode = retCode;
			break;
		case SERVICE_CMD_CLOSE :
			gst_ServiceList[pCmd->id].GLClose( (GL_CLOSE_PARAM*)pCmd->data );
			pCmd->retCode = 0;
			break;
		case SERVICE_CMD_MOTION :
			retCode = gst_ServiceList[pCmd->id].GLMotion( (GL_MOTION_PARAM*)pCmd->data );
			pCmd->retCode = retCode;
			break;
		default :
			break;
		}
		NX_SemaporePost( gst_SvcInfo.hSemResult );
	}
#if DEBUG_API
	NX_DBG("GLServiceThread End!!\n");
#endif
	return (void*)0xDEADDEAD;
}



static void GTServiceInit()
{
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	pthread_mutex_lock( &gst_SvcCtrlLock );
	if( 0 == gst_NumInstance )
	{
		memset( &gst_SvcInfo, 0, sizeof(gst_SvcInfo) );
		gst_SvcInfo.hThread		= 0;
		gst_SvcInfo.isThreadRun	= 0;
		gst_SvcInfo.hCmdQueue	= NX_QueueInit( MAX_CMD_SIZE );
		gst_SvcInfo.hSemCmd		= NX_SemaporeInit( MAX_CMD_SIZE, 0 );
		gst_SvcInfo.hSemResult	= NX_SemaporeInit( MAX_CMD_SIZE, 0 );
		pthread_mutex_init( &gst_SvcInfo.hServiceLock, NULL);

		gst_SvcInfo.isThreadRun	= 1;
		if( 0 > pthread_create( &gst_SvcInfo.hThread, NULL, &GLServiceThread, NULL ) )
		{
			NX_ERR( "%s() : Cannot create service thread!!\n", __FUNCTION__ );
			gst_SvcInfo.isThreadRun	= 0;
		}
		else
		{
			gst_NumInstance ++;
		}
	}
	pthread_mutex_unlock( &gst_SvcCtrlLock );
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
}


static void GTServiceDeinit()
{
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	pthread_mutex_lock( &gst_SvcCtrlLock );
	if( 1 == gst_NumInstance )
	{
		if( gst_SvcInfo.isThreadRun )
		{
			gst_SvcInfo.isThreadRun = false;
			NX_SemaporePost( gst_SvcInfo.hSemCmd );	// send dummy
			pthread_join( gst_SvcInfo.hThread, NULL );
		}
		NX_QueueDeinit( gst_SvcInfo.hCmdQueue );
		NX_SemaporeDeinit( gst_SvcInfo.hSemCmd );
		NX_SemaporeDeinit( gst_SvcInfo.hSemResult );
		pthread_mutex_destroy( &gst_SvcInfo.hServiceLock );
		gst_NumInstance = 0;
	}
	pthread_mutex_unlock( &gst_SvcCtrlLock );
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
}

//	gst_NumInstance
void *GLServiceOpen( int id, GL_OPEN_PARAM *param )
{
	GL_SERVICE_CMD cmd;
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	GTServiceInit();

	cmd.id      = id;
	cmd.cmd     = SERVICE_CMD_OPEN;
	cmd.data    = param;

	//	Push queue
	NX_QueuePush( gst_SvcInfo.hCmdQueue, &cmd );
	NX_SemaporePost( gst_SvcInfo.hSemCmd );

	//	Wait Result
	NX_SemaporePend( gst_SvcInfo.hSemResult );
	cmd.retCode = 0;
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
	return cmd.handle;
}

int GLServiceRun( int id, GL_RUN_PARAM *param )
{
	GL_SERVICE_CMD cmd;
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	cmd.id      = id;
	cmd.cmd     = SERVICE_CMD_RUN;
	cmd.data    = param;

	//	Push queue
	NX_QueuePush( gst_SvcInfo.hCmdQueue, &cmd );
	NX_SemaporePost( gst_SvcInfo.hSemCmd );

	//	Wait Result
	NX_SemaporePend( gst_SvcInfo.hSemResult );
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
	return cmd.retCode;
}

void GLServiceClose( int id, GL_CLOSE_PARAM *param )
{
	GL_SERVICE_CMD cmd;
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	cmd.id      = id;
	cmd.cmd     = SERVICE_CMD_CLOSE;
	cmd.data    = param;

	//	Push queue
	NX_QueuePush( gst_SvcInfo.hCmdQueue, &cmd );
	NX_SemaporePost( gst_SvcInfo.hSemCmd );

	//	Wait Result
	NX_SemaporePend( gst_SvcInfo.hSemResult );
	GTServiceDeinit();
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
	return;
}

int GLServiceMotion( int id, GL_MOTION_PARAM *param )
{
	GL_SERVICE_CMD cmd;
#if DEBUG_API
	NX_DBG("[%s : %d] In\n", __FUNCTION__, __LINE__);
#endif
	cmd.id      = id;
	cmd.cmd     = SERVICE_CMD_MOTION;
	cmd.data    = param;

	//	Push queue
	NX_QueuePush( gst_SvcInfo.hCmdQueue, &cmd );
	NX_SemaporePost( gst_SvcInfo.hSemCmd );

	//	Wait Result
	NX_SemaporePend( gst_SvcInfo.hSemResult );
#if DEBUG_API
	NX_DBG("[%s : %d] Out\n", __FUNCTION__, __LINE__);
#endif
	return cmd.retCode;
}
