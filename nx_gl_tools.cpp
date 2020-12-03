//------------------------------------------------------------------------------
//
//    Copyright (C) 2019 Nexell Co., All Rights Reserved
//    Nexell Co. Proprietary & Confidential
//
//    NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//    AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//    BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//    FOR A PARTICULAR PURPOSE.
//
//    Description    :
//    Author         : hyunjh(hyunjh@nexell.co.kr)
//    History        : 2019-07-24 1st release
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//    Includes
//
//------------------------------------------------------------------------------
#include <stdarg.h>     // for va_start, va_list
#include <math.h>
#include <assert.h>
#include <nx_gpu_surf.h>
#include <linux/videodev2.h>
#include "nx_gl_tools.h"

#define APP_PLATFORM_Y_ALIGN_SIZE  16
#define APP_PLATFORM_X_ALIGN_SIZE  32

#ifndef ALIGN
#define  ALIGN(X,N) ( ((X) + (N - 1)) & (~((N) - 1)) )
#endif

//------------------------------------------------------------------------------
//
//    Defines
//
//------------------------------------------------------------------------------
/* Source is memory */
// #define APP_PLATFORM_SRC_FORMAT 	NX_GSURF_VMEM_IMAGE_FORMAT_RGBA //NX_GSURF_VMEM_IMAGE_FORMAT_YUV420 NX_GSURF_VMEM_IMAGE_FORMAT_YUV420M NX_GSURF_VMEM_IMAGE_FORMAT_RGBA NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE
// #define APP_PLATFORM_DST_FORMAT 	NX_GSURF_VMEM_IMAGE_FORMAT_YUV420 //NX_GSURF_VMEM_IMAGE_FORMAT_NV21 //NX_GSURF_VMEM_IMAGE_FORMAT_NV12 //NX_GSURF_VMEM_IMAGE_FORMAT_YUV420 //NX_GSURF_VMEM_IMAGE_FORMAT_YUV422

//------------------------------------------------------------------------------
//
//    Ratate Functions
//    memory to memory
//------------------------------------------------------------------------------
#define MAX_BUFFER_NUM		31
typedef struct
{
	HGSURFCTRL 		ghAppGSurfCtrl;
	HGSURFSOURCE 	ghAppGSurfSource[MAX_BUFFER_NUM];
	HGSURFTARGET 	ghAppGSurfTarget[MAX_BUFFER_NUM];
	int32_t			srcImageFormat;
	int32_t			dstImageFormat;
	int32_t 		srcDmaFd[MAX_BUFFER_NUM];
	int32_t 		srcDmaBufNum;
	int32_t 		dstDmaFd[MAX_BUFFER_NUM];
	int32_t 		dstDmaBufNum;
	uint32_t 		srcWidth;
	uint32_t 		srcHeight;
	uint32_t 		dstWidth;
	uint32_t 		dstHeight;
	int32_t			dstOutBufNum;
} NX_GL_HANDLE;

static int32_t FindSrcDmaFdIndex(NX_GL_HANDLE *pGlHANDLE, int32_t srcDmaFd)
{
	int32_t i=0;

	for( i = 0 ; i < pGlHANDLE->srcDmaBufNum ; i ++ )
	{
		if( pGlHANDLE->srcDmaFd[i] == srcDmaFd )
		{
			//	already added memory
			return i;
		}
	}
	return -1;
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
			//NxDbgMsg("mem_get_size(%dx%d, 0x%x), add y_align_blank_height(%d), u_align_blank_height(%d)\n", width, height, width * (height + height/2), y_align_blank_height, u_align_blank_height);
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
			//NxDbgMsg("mem_get_size(%dx%d, 0x%x), add y_align_blank_height(%d)\n", width, height, width * height, y_align_blank_height);
		}
	}
	else
	{
		printf("mem_get_size() unvalid format(%d)!\n", image_format);
		return 0;
	}
	//NxDbgMsg("mem_get_size(0x%x, %dx%d)\n", bytes_size, width, height);
	return bytes_size;
}

void *NX_GlRotateInit(uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth, uint32_t dstHeight, int32_t (*pDstDmaFd)[3], int32_t srcImageFormat, int32_t dstOutBufNum)
{
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;

	NX_GL_HANDLE *pRotate_HANDLE = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));

	memset(pRotate_HANDLE, 0x00, sizeof(NX_GL_HANDLE) );

	for(int i = 0; i < MAX_BUFFER_NUM; i++)
	{
		pRotate_HANDLE->srcDmaFd[i] = -1;
		pRotate_HANDLE->dstDmaFd[i] = -1;
	}

	if( (srcImageFormat == V4L2_PIX_FMT_YUV420) || srcImageFormat == V4L2_PIX_FMT_YUV420M)
	{
		pRotate_HANDLE->srcImageFormat = srcImageFormat;
		pRotate_HANDLE->dstImageFormat = srcImageFormat;

		glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;
		glDstFormat = glSrcFormat;

	}
	else
	{
		NX_ERR("Not Support imageFormat !!!\n");
		return NULL;
	}

	pRotate_HANDLE->dstOutBufNum = dstOutBufNum;

	pRotate_HANDLE->srcWidth  = srcWidth;
	pRotate_HANDLE->srcHeight = srcHeight;
	pRotate_HANDLE->dstWidth  = dstWidth;
	pRotate_HANDLE->dstHeight = dstHeight;


	//create GSurf handle
	pRotate_HANDLE->ghAppGSurfCtrl = nxGSurfaceCreate(dstOutBufNum, NX_FALSE,
						0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(pRotate_HANDLE->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	//init GSurf EGL
	nxGSurfaceInitEGL(pRotate_HANDLE->ghAppGSurfCtrl, NULL,
	      glSrcFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      glDstFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);

	//create target
	unsigned int gRenderWidth = pRotate_HANDLE->dstWidth;
	unsigned int gRenderHeight = pRotate_HANDLE->dstHeight;

	for (int i = 0 ; i < dstOutBufNum ; i++)
	{
		//create GSurf target handle
		if(pRotate_HANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pRotate_HANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateTarget(pRotate_HANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i][0]);
		}
		else if(pRotate_HANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pRotate_HANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateTargetWithFDs(pRotate_HANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		pRotate_HANDLE->dstDmaFd[i] = pDstDmaFd[i][0];
		pRotate_HANDLE->dstDmaBufNum++;
	}

	//init GSurf GL
	nxGSurfaceInitGL(pRotate_HANDLE->ghAppGSurfCtrl, pRotate_HANDLE->ghAppGSurfTarget[0]);

	return (void *)pRotate_HANDLE;
}

int32_t NX_GlRotateRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd, int32_t rotateMode)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return -1; 
	}
	NX_GL_HANDLE *pRotateHANDLE = (NX_GL_HANDLE *)pHandle;

	int32_t srcDmaFd = pSrcDmaFd[0];

	HGSURFSOURCE hsource = NULL;
	HGSURFTARGET htarget = NULL;

	// find target
	for(int i = 0; i < pRotateHANDLE->dstDmaBufNum; i++)
	{
		if(pRotateHANDLE->dstDmaFd[i] == pDstDmaFd[0])
		{
			htarget = pRotateHANDLE->ghAppGSurfTarget[i];
			break;
		}
	}

	int32_t index = FindSrcDmaFdIndex(pRotateHANDLE, srcDmaFd );
	if( 0 > index )
	{
		//create GSurf source handle
		int32_t srcWidth = pRotateHANDLE->srcWidth;
		int32_t srcHeight = pRotateHANDLE->srcHeight;
		if(pRotateHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pRotateHANDLE->ghAppGSurfSource[pRotateHANDLE->srcDmaBufNum] = nxGSurfaceCreateSource(pRotateHANDLE->ghAppGSurfCtrl, srcWidth, srcHeight, pSrcDmaFd[0]);
		}
		else if(pRotateHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pRotateHANDLE->ghAppGSurfSource[pRotateHANDLE->srcDmaBufNum] = nxGSurfaceCreateSourceWithFDs(pRotateHANDLE->ghAppGSurfCtrl, srcWidth, srcHeight, pSrcDmaFd);
		}
		else
		{
			return -1;
		}

		pRotateHANDLE->srcDmaFd[pRotateHANDLE->srcDmaBufNum] = srcDmaFd;
		index = pRotateHANDLE->srcDmaBufNum;
		pRotateHANDLE->srcDmaBufNum++;
	}

	hsource = pRotateHANDLE->ghAppGSurfSource[index];

	uint32_t gRenderWidth = pRotateHANDLE->dstWidth;
	uint32_t gRenderHeight = pRotateHANDLE->dstHeight;
	nxGSurfaceRender(pRotateHANDLE->ghAppGSurfCtrl, hsource, htarget, (NX_GSURF_ROTATE_MODE)rotateMode);

	nxGSurfaceUpdate(pRotateHANDLE->ghAppGSurfCtrl);

	return 0;
}

void NX_GlRotateDeInit(void *pHandle)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return; 
	}

	NX_GL_HANDLE *pRotateHANDLE = (NX_GL_HANDLE *)pHandle;

	//destroy GSurf source handle
	for (int i = 0 ; i < pRotateHANDLE->srcDmaBufNum ; i++)
	{

		nxGSurfaceDestroySource(pRotateHANDLE->ghAppGSurfCtrl, pRotateHANDLE->ghAppGSurfSource[i]);
		pRotateHANDLE->ghAppGSurfSource[i] = NULL;
	}

	//deinit GSurf GL
	nxGSurfaceDeinitGL(pRotateHANDLE->ghAppGSurfCtrl);

	//destroy target	
	for (int i = 0 ; i < pRotateHANDLE->dstOutBufNum ; i++)
	{
		//destroy GSurf target handle
		nxGSurfaceDestroyTarget(pRotateHANDLE->ghAppGSurfCtrl, pRotateHANDLE->ghAppGSurfTarget[i]);
		pRotateHANDLE->ghAppGSurfTarget[i] = NULL;
	}

	//deinit GSurf EGL
	nxGSurfaceDeinitEGL(pRotateHANDLE->ghAppGSurfCtrl);

	//destroy GSurf handle
	nxGSurfaceDestroy(pRotateHANDLE->ghAppGSurfCtrl);
	pRotateHANDLE->ghAppGSurfCtrl = NULL;

	if(pRotateHANDLE)
	{
		free(pRotateHANDLE);
	}
}


//------------------------------------------------------------------------------
//
//    Deinterlace Functions
//    memory to memory
//------------------------------------------------------------------------------
void *NX_GlDeinterlaceInit(uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth, uint32_t dstHeight, int32_t (*pDstDmaFd)[3], int32_t srcImageFormat, int32_t dstOutBufNum)
{
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;

	int32_t bRet = false;

	NX_GL_HANDLE *pDeinterlace_HANDLE = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));

	memset(pDeinterlace_HANDLE, 0x00, sizeof(NX_GL_HANDLE) );

	for(int i = 0; i < MAX_BUFFER_NUM; i++)
	{
		pDeinterlace_HANDLE->srcDmaFd[i] = -1;
		pDeinterlace_HANDLE->dstDmaFd[i] = -1;
	}

	if( (srcImageFormat == V4L2_PIX_FMT_YUV420) || srcImageFormat == V4L2_PIX_FMT_YUV420M)
	{
		pDeinterlace_HANDLE->srcImageFormat = srcImageFormat;
		pDeinterlace_HANDLE->dstImageFormat = srcImageFormat;

		glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;
		glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420_INTERLACE;

	}
	else
	{
		NX_ERR("Not Support imageFormat !!!\n");
		return NULL;
	}

	pDeinterlace_HANDLE->dstOutBufNum = dstOutBufNum;

	pDeinterlace_HANDLE->srcWidth  = srcWidth;
	pDeinterlace_HANDLE->srcHeight = srcHeight;
	pDeinterlace_HANDLE->dstWidth  = dstWidth;
	pDeinterlace_HANDLE->dstHeight = dstHeight;


	//create GSurf handle
	pDeinterlace_HANDLE->ghAppGSurfCtrl = nxGSurfaceCreate(dstOutBufNum, NX_FALSE,
						0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(pDeinterlace_HANDLE->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	//init GSurf EGL
	bRet = nxGSurfaceInitEGL(pDeinterlace_HANDLE->ghAppGSurfCtrl, NULL,
	      glSrcFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      glDstFormat, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);
	if(bRet == false)
	{
		return NULL;
	}

	//create target
	unsigned int gRenderWidth = pDeinterlace_HANDLE->dstWidth;
	unsigned int gRenderHeight = pDeinterlace_HANDLE->dstHeight;

	for (int i = 0 ; i < dstOutBufNum ; i++)
	{
		//create GSurf target handle
		if(pDeinterlace_HANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pDeinterlace_HANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateDeinterlaceTarget(pDeinterlace_HANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i][0]);
		}
		else if(pDeinterlace_HANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pDeinterlace_HANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateDeinterlaceTargetWithFDs(pDeinterlace_HANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		pDeinterlace_HANDLE->dstDmaFd[i] = pDstDmaFd[i][0];
		pDeinterlace_HANDLE->dstDmaBufNum++;
	}

	//init GSurf GL
	bRet = nxGSurfaceInitGL(pDeinterlace_HANDLE->ghAppGSurfCtrl, pDeinterlace_HANDLE->ghAppGSurfTarget[0]);

	if(bRet == false)
	{
		return NULL;
	}	

	return (void *)pDeinterlace_HANDLE;
}

int32_t NX_GlDeinterlaceRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return -1; 
	}
	NX_GL_HANDLE *pDeinterlaceHANDLE = (NX_GL_HANDLE *)pHandle;
	int32_t bRet = false;

	int32_t srcDmaFd = pSrcDmaFd[0];

	HGSURFSOURCE hsource = NULL;
	HGSURFTARGET htarget = NULL;

	// find target
	for(int i = 0; i < pDeinterlaceHANDLE->dstDmaBufNum; i++)
	{
		if(pDeinterlaceHANDLE->dstDmaFd[i] == pDstDmaFd[0])
		{
			htarget = pDeinterlaceHANDLE->ghAppGSurfTarget[i];
			break;
		}
	}

	int32_t index = FindSrcDmaFdIndex(pDeinterlaceHANDLE, srcDmaFd );
	// printf("@@@@@@@ index(%d),htarget(%p)\n",index,htarget);
	if( 0 > index )
	{
		//create GSurf source handle
		int32_t srcWidth = pDeinterlaceHANDLE->srcWidth;
		int32_t srcHeight = pDeinterlaceHANDLE->srcHeight;
		if(pDeinterlaceHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pDeinterlaceHANDLE->ghAppGSurfSource[pDeinterlaceHANDLE->srcDmaBufNum] = nxGSurfaceCreateDeinterlaceSource(pDeinterlaceHANDLE->ghAppGSurfCtrl, srcWidth, srcHeight, pSrcDmaFd[0]);
		}
		else if(pDeinterlaceHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pDeinterlaceHANDLE->ghAppGSurfSource[pDeinterlaceHANDLE->srcDmaBufNum] = nxGSurfaceCreateDeinterlaceSourceWithFDs(pDeinterlaceHANDLE->ghAppGSurfCtrl, srcWidth, srcHeight, pSrcDmaFd);
		}
		else
		{
			return -1;
		}

		pDeinterlaceHANDLE->srcDmaFd[pDeinterlaceHANDLE->srcDmaBufNum] = srcDmaFd;
		index = pDeinterlaceHANDLE->srcDmaBufNum;
		pDeinterlaceHANDLE->srcDmaBufNum++;
	}

	hsource = pDeinterlaceHANDLE->ghAppGSurfSource[index];

	uint32_t gRenderWidth = pDeinterlaceHANDLE->dstWidth;
	uint32_t gRenderHeight = pDeinterlaceHANDLE->dstHeight;

	bRet = nxGSurfaceRenderDeinterlace(pDeinterlaceHANDLE->ghAppGSurfCtrl, hsource, htarget);

	if(bRet == false)
	{
		return -1;
	}		

	nxGSurfaceUpdate(pDeinterlaceHANDLE->ghAppGSurfCtrl);

	return 0;
}

void NX_GlDeinterlaceDeInit(void *pHandle)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return; 
	}

	NX_GL_HANDLE *pDeinterlaceHANDLE = (NX_GL_HANDLE *)pHandle;

	//destroy GSurf source handle
	for (int i = 0 ; i < pDeinterlaceHANDLE->srcDmaBufNum ; i++)
	{

		nxGSurfaceDestroySource(pDeinterlaceHANDLE->ghAppGSurfCtrl, pDeinterlaceHANDLE->ghAppGSurfSource[i]);
		pDeinterlaceHANDLE->ghAppGSurfSource[i] = NULL;
	}

	//deinit GSurf GL
	nxGSurfaceDeinitGL(pDeinterlaceHANDLE->ghAppGSurfCtrl);

	//destroy target	
	for (int i = 0 ; i < pDeinterlaceHANDLE->dstOutBufNum ; i++)
	{
		//destroy GSurf target handle
		nxGSurfaceDestroyTarget(pDeinterlaceHANDLE->ghAppGSurfCtrl, pDeinterlaceHANDLE->ghAppGSurfTarget[i]);
		pDeinterlaceHANDLE->ghAppGSurfTarget[i] = NULL;
	}

	//deinit GSurf EGL
	nxGSurfaceDeinitEGL(pDeinterlaceHANDLE->ghAppGSurfCtrl);

	//destroy GSurf handle
	nxGSurfaceDestroy(pDeinterlaceHANDLE->ghAppGSurfCtrl);
	pDeinterlaceHANDLE->ghAppGSurfCtrl = NULL;

	if(pDeinterlaceHANDLE)
	{
		free(pDeinterlaceHANDLE);
	}
}

#ifdef PIE
//------------------------------------------------------------------------------
//
//    MemCopy Functions
//    memory to memory
//------------------------------------------------------------------------------
void *NX_GlMemCopyInit(uint32_t Width, uint32_t Height, int32_t (*pDstDmaFd)[3], int32_t srcImageFormat, int32_t dstOutBufNum)
{
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;
	NX_GSURF_VMEM_IMAGE_FORMAT_MODE glDstFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;

	int32_t ret = 0;

	NX_GL_HANDLE *pMemCopyHANDLE = (NX_GL_HANDLE *)malloc(sizeof(NX_GL_HANDLE));

	memset(pMemCopyHANDLE, 0x00, sizeof(NX_GL_HANDLE) );

	for(int i = 0; i < MAX_BUFFER_NUM; i++)
	{
		pMemCopyHANDLE->srcDmaFd[i] = -1;
		pMemCopyHANDLE->dstDmaFd[i] = -1;
	}

	if( (srcImageFormat == V4L2_PIX_FMT_YUV420) || srcImageFormat == V4L2_PIX_FMT_YUV420M)
	{
		pMemCopyHANDLE->srcImageFormat = srcImageFormat;
		pMemCopyHANDLE->dstImageFormat = srcImageFormat;

		glSrcFormat = NX_GSURF_VMEM_IMAGE_FORMAT_YUV420;
		glDstFormat = glSrcFormat;
	}
	else
	{
		NX_ERR("Not Support imageFormat !!!\n");
		return NULL;
	}

	pMemCopyHANDLE->dstOutBufNum = dstOutBufNum;

	pMemCopyHANDLE->srcWidth  = Width;
	pMemCopyHANDLE->srcHeight = Height;
	pMemCopyHANDLE->dstWidth  = Width;
	pMemCopyHANDLE->dstHeight = Height;


	//create GSurf handle
	pMemCopyHANDLE->ghAppGSurfCtrl = nxGSurfaceCreate(dstOutBufNum, NX_FALSE,
						0, 0, 0, 0, 0, NX_FALSE);

	nxGSurfaceSetDoneCallback(pMemCopyHANDLE->ghAppGSurfCtrl, NULL); /* not used  at pixmap */

	//init GSurf EGL
	ret = nxGSurfaceInitEGL(pMemCopyHANDLE->ghAppGSurfCtrl, NULL,
	      NX_GSURF_VMEM_IMAGE_FORMAT_RGBA, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE,
	      NX_GSURF_VMEM_IMAGE_FORMAT_RGBA, APP_PLATFORM_X_ALIGN_SIZE, APP_PLATFORM_Y_ALIGN_SIZE);


	//create target
	unsigned int input_image_length = vmem_get_size(pMemCopyHANDLE->dstWidth, pMemCopyHANDLE->dstHeight, 
	NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, 32, 16);

	unsigned int gRenderWidth = pMemCopyHANDLE->dstWidth / 4;
	unsigned int gRenderHeight = input_image_length / pMemCopyHANDLE->dstWidth;


	for (int i = 0 ; i < dstOutBufNum ; i++)
	{
		//create GSurf target handle
		if(pMemCopyHANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pMemCopyHANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateTarget(pMemCopyHANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i][0]);
		}
		else if(pMemCopyHANDLE->srcImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pMemCopyHANDLE->ghAppGSurfTarget[i] = nxGSurfaceCreateTargetWithFDs(pMemCopyHANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pDstDmaFd[i]);
		}
		else
		{
			return NULL;
		}

		pMemCopyHANDLE->dstDmaFd[i] = pDstDmaFd[i][0];
		pMemCopyHANDLE->dstDmaBufNum++;
	}

	//init GSurf GL
	ret = nxGSurfaceInitGL(pMemCopyHANDLE->ghAppGSurfCtrl, pMemCopyHANDLE->ghAppGSurfTarget[0]);

	return (void *)pMemCopyHANDLE;
}

int32_t NX_GlMemCopyRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return -1; 
	}
	NX_GL_HANDLE *pMemCopyHANDLE = (NX_GL_HANDLE *)pHandle;

	int32_t srcDmaFd = pSrcDmaFd[0];
	int32_t rotateMode = 0;

	HGSURFSOURCE hsource = NULL;
	HGSURFTARGET htarget = NULL;

	// find target
	for(int i = 0; i < pMemCopyHANDLE->dstDmaBufNum; i++)
	{
		if(pMemCopyHANDLE->dstDmaFd[i] == pDstDmaFd[0])
		{
			htarget = pMemCopyHANDLE->ghAppGSurfTarget[i];
			break;
		}
	}

	int32_t index = FindSrcDmaFdIndex(pMemCopyHANDLE, srcDmaFd );
	if( 0 > index )
	{
		//create GSurf source handle
		unsigned int input_image_length = vmem_get_size(pMemCopyHANDLE->srcWidth, pMemCopyHANDLE->srcHeight, 
			NX_GSURF_VMEM_IMAGE_FORMAT_YUV420, 32, 16);

		unsigned int gRenderWidth = pMemCopyHANDLE->srcWidth / 4;
		unsigned int gRenderHeight = input_image_length / pMemCopyHANDLE->srcWidth;

		if(pMemCopyHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420)
		{
			pMemCopyHANDLE->ghAppGSurfSource[pMemCopyHANDLE->srcDmaBufNum] = nxGSurfaceCreateSource(pMemCopyHANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pSrcDmaFd[0]);
		}
		else if(pMemCopyHANDLE->dstImageFormat == V4L2_PIX_FMT_YUV420M)
		{
			pMemCopyHANDLE->ghAppGSurfSource[pMemCopyHANDLE->srcDmaBufNum] = nxGSurfaceCreateSourceWithFDs(pMemCopyHANDLE->ghAppGSurfCtrl, gRenderWidth, gRenderHeight, pSrcDmaFd);
		}
		else
		{
			return -1;
		}

		pMemCopyHANDLE->srcDmaFd[pMemCopyHANDLE->srcDmaBufNum] = srcDmaFd;
		index = pMemCopyHANDLE->srcDmaBufNum;
		pMemCopyHANDLE->srcDmaBufNum++;
	}

	hsource = pMemCopyHANDLE->ghAppGSurfSource[index];

	uint32_t gRenderWidth = pMemCopyHANDLE->dstWidth;
	uint32_t gRenderHeight = pMemCopyHANDLE->dstHeight;
	nxGSurfaceRender(pMemCopyHANDLE->ghAppGSurfCtrl, hsource, htarget, (NX_GSURF_ROTATE_MODE)rotateMode);

	nxGSurfaceUpdate(pMemCopyHANDLE->ghAppGSurfCtrl);

	return 0;
}

void NX_GlMemCopyDeInit(void *pHandle)
{
	if(!pHandle)
	{
		NX_ERR("pHandle is NULL !\n");
		return; 
	}

	NX_GL_HANDLE *pMemCopyHANDLE = (NX_GL_HANDLE *)pHandle;

	//destroy GSurf source handle
	for (int i = 0 ; i < pMemCopyHANDLE->srcDmaBufNum ; i++)
	{

		nxGSurfaceDestroySource(pMemCopyHANDLE->ghAppGSurfCtrl, pMemCopyHANDLE->ghAppGSurfSource[i]);
		pMemCopyHANDLE->ghAppGSurfSource[i] = NULL;
	}

	//deinit GSurf GL
	nxGSurfaceDeinitGL(pMemCopyHANDLE->ghAppGSurfCtrl);

	//destroy target	
	for (int i = 0 ; i < pMemCopyHANDLE->dstOutBufNum ; i++)
	{
		//destroy GSurf target handle
		nxGSurfaceDestroyTarget(pMemCopyHANDLE->ghAppGSurfCtrl, pMemCopyHANDLE->ghAppGSurfTarget[i]);
		pMemCopyHANDLE->ghAppGSurfTarget[i] = NULL;
	}

	//deinit GSurf EGL
	nxGSurfaceDeinitEGL(pMemCopyHANDLE->ghAppGSurfCtrl);

	//destroy GSurf handle
	nxGSurfaceDestroy(pMemCopyHANDLE->ghAppGSurfCtrl);
	pMemCopyHANDLE->ghAppGSurfCtrl = NULL;

	if(pMemCopyHANDLE)
	{
		free(pMemCopyHANDLE);
	}
}
#endif

