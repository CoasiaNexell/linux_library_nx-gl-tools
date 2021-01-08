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
void *NX_GlRotateInit(uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth, uint32_t dstHeight, int32_t (*pDstDmaFd)[3], int32_t srcImageFormat, int32_t dstOutBufNum);

/**
* NX_GlRotateRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			source height
* @param[in]		*pDstDmaFd			destination width
* @param[in]		rotateMode			rotateMode
* @return 0 on success, negative error on failure
*/
int32_t NX_GlRotateRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd, int32_t rotateMode);

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
* @param[in]		(*pDstDmaFd)[3]			destination dmaFd
* @param[in]		srcImageFormat			source imageFormat
* @param[in]		dstOutBufNum			destination buffer number
* @return if not null on success(handle), null on failure
*/
void *NX_GlDeinterlaceInit(uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth, uint32_t dstHeight, int32_t (*pDstDmaFd)[3], int32_t srcImageFormat, int32_t dstOutBufNum);

/**
* NX_GlDeinterlaceRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			source height
* @param[in]		*pDstDmaFd			destination width
* @return 0 on success, negative error on failure
*/
int32_t NX_GlDeinterlaceRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd);

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
* @param[in]		Width				width
* @param[in]		Height				height
* @param[in]		(*pDstDmaFd)[3]			destination dmaFd
* @param[in]		ImageFormat			source imageFormat
* @param[in]		dstOutBufNum			destination buffer number
* @return if not null on success(handle), null on failure
*/
void *NX_GlMemCopyInit(uint32_t Width, uint32_t height, int32_t (*pDstDmaFd)[3], int32_t ImageFormat, int32_t dstOutBufNum);

/**
* NX_GlMemCopyRun.
*
* @param[in]		*pHandle			handle
* @param[in]		*pSrcDmaFd			source height
* @param[in]		*pDstDmaFd			destination width
* @param[in]		rotateMode			rotateMode
* @return 0 on success, negative error on failure
*/
int32_t NX_GlMemCopyRun(void *pHandle, int32_t* pSrcDmaFd, int32_t *pDstDmaFd);

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
