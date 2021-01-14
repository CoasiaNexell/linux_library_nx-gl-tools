#ifndef _GL_SERVICE_H_
#define _GL_SERVICE_H_

//
//  deinterlace / copy / rotate
//
enum {
	SERVICE_ID_DEINTERLACE	= 0,
	SERVICE_ID_COPY			= 1,
	SERVICE_ID_ROTATE		= 2,
	MAX_SERVICE_NUM,
};

enum {
	SERVICE_CMD_OPEN		= 0,
	SERVICE_CMD_RUN			= 1,
	SERVICE_CMD_CLOSE		= 2,
};

typedef struct tag_GL_OPEN_PARAM
{
	unsigned int srcWidth;
	unsigned int srcHeight;
	unsigned int dstWidth;
	unsigned int dstHeight;
	int (*pDstDmaFd)[3];
	int srcImageFormat;
	int dstOutBufNum;
} GL_OPEN_PARAM;

typedef struct tag_GL_RUN_PARAM
{
	void *pHandle;
	int *pSrcDmaFd;
	int *pDstDmaFd;
	int option;
} GL_RUN_PARAM;

typedef struct tag_GL_CLOSE_PARAM
{
	void *pHandle;
} GL_CLOSE_PARAM;


void *GLServiceOpen( int id, GL_OPEN_PARAM *param );
int GLServiceRun( int id, GL_RUN_PARAM *param );
void GLServiceClose( int id, GL_CLOSE_PARAM *param );

#endif // _GL_SERVICE_H_
