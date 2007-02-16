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
SaCkptResponseT* 
createLocalReplical(SaCkptRequestT* ckptReq);

#endif
