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

#ifndef __NX_GlTOOL_H__
#define __NX_GlTOOL_H__

//#include <stdint.h>

//------------------------------------------------------------------------------
//
//    Defines
//
//------------------------------------------------------------------------------
#ifdef ANDROID
#include <utils/Log.h>
#define NX_DBG			ALOGD
#define NX_ERR			ALOGE
#else
#define NX_DBG			printf
#define NX_ERR			printf
#endif

//------------------------------------------------------------------------------
//
//    Ratate Functions
//    memory to memory
//------------------------------------------------------------------------------
enum
{
	NX_ROTATE_O,
	NX_ROTATE_90R,
	NX_ROTATE_180,
	NX_ROTATE_270R,
	NX_ROTATE_MIRROR,
	NX_ROTATE_VFLIP,
	NX_ROTATE_MAX
};

#define DEINT_MODE_MIXING	0
#define	DEINT_MODE_ADAPTIVE	1

#ifdef __cplusplus
extern "C" {
#endif

/**
* NX_GlRotateInit.
*
* @param[in]		srcWidth			source width
* @param[in]		srcHeight			source height
* @param[in]		dstWidth			destination width
* @param[in]		dstHeight			destination height
* @param[in]		(*pDstDmaFd)[3]			destination dmaFd
* @param[in]		srcImageFormat			source imageFormat
* @param[in]		dstOutBufNum			destination buffer number
* @return if not null on success(handle), null on failure
*/
void *NX_GlRotateInit(unsigned int srcWidth, unsigned int srcHeight, 
					  unsigned int dstWidth, unsigned int dstHeight,
					  int (*pDstDmaFd)[3], int srcImageFormat, int dstOutBufNum);

/**
* NX_GlRotateRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			source height
* @param[in]		*pDstDmaFd			destination width
* @param[in]		rotateMode			rotateMode
* @return 0 on success, negative error on failure
*/
int NX_GlRotateRun(void *pHandle, int *pSrcDmaFd, int *pDstDmaFd, int rotateMode);

/**
* NX_GlRotateDeInit.
*
* @param[in]		*pHandle			handle
*/
void NX_GlRotateDeInit(void *pHandle);

//------------------------------------------------------------------------------
//
//    Deinterlace Functions
//    memory to memory
//------------------------------------------------------------------------------
/**
* NX_GlDeinterlaceInit.
*
* @param[in]		srcWidth			source width
* @param[in]		srcHeight			source height
* @param[in]		dstWidth			destination width
* @param[in]		dstHeight			destination height
* @param[in]		(*pDstDmaFd)[3]		destination dmaFd
* @param[in]		srcImageFormat		source imageFormat
* @param[in]		dstOutBufNum		destination buffer number
* @param[in]		mode				deinterlace mode (def:0)
* @param[in]		motionFds			motion buffer's fds
* @param[in]		coeff				co-efficient ( 1.5 ~ 3.0, def 2.5 )
* @return if not null on success(handle), null on failure
*/
void *NX_GlDeinterlaceInit(unsigned int srcWidth, unsigned int srcHeight,
						   unsigned int dstWidth, unsigned int dstHeight,
						   int (*pDstDmaFd)[3], int srcImageFormat, int dstOutBufNum,
						   int mode, int motionFds[2], float coeff);

/**
* NX_GlDeinterlaceMotion.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFds			current frame's DMA fds
* @param[in]		*pSrcNDmaFds		next frame's DMA fds
* @return 0 on success, negative error on failure
*/
int NX_GlDeinterlaceMotion(void *pHandle, int *pSrcDmaFds, int *pSrcNDmaFds);

/**
* NX_GlDeinterlaceRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			current frame's DMA fd
* @param[in]		*pSrcNDmaFd			next frame's DMA fd
* @param[in]		*pDstDmaFd			destination frame's DMA fd
* @return 0 on success, negative error on failure
*/
int NX_GlDeinterlaceRun(void *pHandle, int *pSrcDmaFd, int *pSrcNDmaFd, int *pDstDmaFd);

/**
* NX_GlDeinterlaceDeInit.
*
* @param[in]		*pHandle			handle
*/
void NX_GlDeinterlaceDeInit(void *pHandle);


//------------------------------------------------------------------------------
//
//    MemCopy Functions
//    memory to memory
//------------------------------------------------------------------------------
/**
* NX_GlMemCopyInit.
*
* @param[in]		width				width
* @param[in]		height				height
* @param[in]		(*pDstDmaFd)[3]			destination dmaFd
* @param[in]		imageFormat			source imageFormat
* @param[in]		dstOutBufNum			destination buffer number
* @return if not null on success(handle), null on failure
*/
void *NX_GlMemCopyInit(unsigned int width, unsigned int height, int (*pDstDmaFd)[3], int imageFormat, int dstOutBufNum);

/**
* NX_GlMemCopyRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			source height
* @param[in]		*pDstDmaFd			destination width
* @param[in]		rotateMode			rotateMode
* @return 0 on success, negative error on failure
*/
int NX_GlMemCopyRun(void *pHandle, int *pSrcDmaFd, int *pDstDmaFd);

/**
* NX_GlMemCopyDeInit.
*
* @param[in]		*pHandle			handle
*/
void NX_GlMemCopyDeInit(void *pHandle);
#endif

#ifdef __cplusplus
}
#endif
