#ifndef _SA_CKPT_UTILS_H
#define _SA_CKPT_UTILS_H

#include <saf/ais.h>

int SaCkptVersionCompare(SaVersionT, SaVersionT);
void SaCkptPackVersion(char*, SaVersionT*);
void SaCkptUnpackVersion(const char*, SaVersionT*);

char* SaCkptErr2String(SaErrorT);

void* SaCkptMalloc(int);
void SaCkptFree(void**);


#define SACKPTASSERT(x) \
{	\
	if (!(x)) {	\
		cl_log(LOG_ERR,	\
			"Assertion failed on line %d in file \"%s\"",	\
			__LINE__, __FILE__);	\
		exit(1);	\
	}	\
}

#endif

