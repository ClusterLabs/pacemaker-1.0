/* $Id: response.h,v 1.3 2004/03/12 02:59:38 deng.pan Exp $ */
#ifndef _CKPT_RESPONSE_H
#define _CKPT_RESPONSE_H

#include <saf/ais.h>
#include <checkpointd/clientrequest.h>
#include "request.h"


typedef struct _SaCkptResponseT {
	SaCkptRequestT*	ckptReq;
	SaCkptClientResponseT*	resp;
} SaCkptResponseT;	

SaCkptResponseT* SaCkptResponseCreate(SaCkptRequestT*);
int SaCkptResponseSend(SaCkptResponseT**);


#endif
