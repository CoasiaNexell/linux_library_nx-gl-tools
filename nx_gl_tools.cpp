//------------------------------------------------------------------------------
//
//    Copyright (C) 2021 Nexell Co., All Rights Reserved
//    Nexell Co. Proprietary & Confidential
//
//    NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//    AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//    BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//    FOR A PARTICULAR PURPOSE.
//
//    Description    : This function is an external interface code to use GL Tool.
//    Author         : SeongO Park (ray@coasia.com)
//    History        : First Release
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//    Includes
//
//------------------------------------------------------------------------------

#include "gl_service.h"
#include "nx_gl_tools.h"

#ifdef ANDROID
#define LOG_TAG "NX_GL_TOOLS"	//	log options
#include <utils/Log.h>
#define NX_DBG			ALOGD
#define NX_ERR			ALOGE
#else
#define NX_DBG			printf
#define NX_ERR			printf
#endif


//////////////////////////////////////////////////////////////////////////////
//
//					Rotate Service
//
void *NX_GlRotateInit( unsigned int srcWidth, unsigned int srcHeight, 
					   unsigned int dstWidth, unsigned int dstHeight,
					   int (*pDstDmaFd)[3], int srcImageFormat, int dstOutBufNum )
{
	GL_OPEN_PARAM param;
	param.srcWidth       = srcWidth;
	param.srcHeight      = srcHeight;
	param.dstWidth       = dstWidth;
	param.dstHeight      = dstHeight;
	param.pDstDmaFd      = pDstDmaFd;
	param.srcImageFormat = srcImageFormat;
	param.dstOutBufNum   = dstOutBufNum;
	return GLServiceOpen( SERVICE_ID_ROTATE, &param );
}

int NX_GlRotateRun(void *pHandle, int* pSrcDmaFd, int *pDstDmaFd, int rotateMode)
{
	GL_RUN_PARAM param;
	param.pHandle   = pHandle;
	param.pSrcDmaFd = pSrcDmaFd;
	param.pDstDmaFd = pDstDmaFd;
	param.option    = rotateMode;
	return GLServiceRun( SERVICE_ID_ROTATE, &param );
}

void NX_GlRotateDeInit(void *pHandle)
{
	GL_CLOSE_PARAM param;
	param.pHandle = pHandle;
	GLServiceClose( SERVICE_ID_ROTATE, &param );
}

//////////////////////////////////////////////////////////////////////////////
//
//					Deinterlace Service
//
void *NX_GlDeinterlaceInit( unsigned int srcWidth, unsigned int srcHeight,
							unsigned int dstWidth, unsigned int dstHeight,
							int (*pDstDmaFd)[3], int srcImageFormat, int dstOutBufNum,
							int mode, int motionFds[2], float coeff)
{
	GL_OPEN_PARAM param;
	param.srcWidth       = srcWidth;
	param.srcHeight      = srcHeight;
	param.dstWidth       = dstWidth;
	param.dstHeight      = dstHeight;
	param.pDstDmaFd      = pDstDmaFd;
	param.srcImageFormat = srcImageFormat;
	param.dstOutBufNum   = dstOutBufNum;
	param.mode           = mode;
	param.motionFds[0]   = motionFds[0];
	param.motionFds[1]   = motionFds[1];
	param.coeff          = coeff;
	return GLServiceOpen( SERVICE_ID_DEINTERLACE, &param );
}

int NX_GlDeinterlaceRun(void *pHandle, int *pSrcDmaFd, int *pSrcNDmaFd, int *pDstDmaFd)
{
	GL_RUN_PARAM param;
	param.pHandle   = pHandle;
	param.pSrcDmaFd = pSrcDmaFd;
	param.pSrcNDmaFd = pSrcNDmaFd;
	param.pDstDmaFd = pDstDmaFd;
	param.option    = 0;
	return GLServiceRun( SERVICE_ID_DEINTERLACE, &param );
}

void NX_GlDeinterlaceDeInit(void *pHandle)
{
	GL_CLOSE_PARAM param;
	param.pHandle = pHandle;
	GLServiceClose( SERVICE_ID_DEINTERLACE, &param );
}

int NX_GlDeinterlaceMotion(void *pHandle, int *pSrcDmaFds, int *pSrcNDmaFds)
{
	GL_MOTION_PARAM param;
	param.pHandle   = pHandle;
	param.pSrcDmaFds = pSrcDmaFds;
	param.pSrcNDmaFds = pSrcNDmaFds;
	return GLServiceMotion( SERVICE_ID_DEINTERLACE, &param );
}

//////////////////////////////////////////////////////////////////////////////
//
//					Memory Copy Service
//
void *NX_GlMemCopyInit( unsigned int srcWidth, unsigned int srcHeight,
						int (*pDstDmaFd)[3], int srcImageFormat, int dstOutBufNum)
{
	GL_OPEN_PARAM param;
	param.srcWidth       = srcWidth;
	param.srcHeight      = srcHeight;
	param.dstWidth       = srcWidth;
	param.dstHeight      = srcHeight;
	param.pDstDmaFd      = pDstDmaFd;
	param.srcImageFormat = srcImageFormat;
	param.dstOutBufNum   = dstOutBufNum;
	return GLServiceOpen( SERVICE_ID_COPY, &param );
}

int NX_GlMemCopyRun(void *pHandle, int *pSrcDmaFd, int *pDstDmaFd)
{
	GL_RUN_PARAM param;
	param.pHandle   = pHandle;
	param.pSrcDmaFd = pSrcDmaFd;
	param.pDstDmaFd = pDstDmaFd;
	param.option    = 0;
	return GLServiceRun( SERVICE_ID_COPY, &param );
}

void NX_GlMemCopyDeInit(void *pHandle)
{
	GL_CLOSE_PARAM param;
	param.pHandle = pHandle;
	GLServiceClose( SERVICE_ID_COPY, &param );
}
