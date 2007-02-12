/* $Id: utils.c,v 1.8 2004/11/18 01:56:59 yixiong Exp $ */
/* 
 * utils.c
 *
 * Copyright (C) 2003 Deng Pan <deng.pan@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include <saf/ais.h>
#include "utils.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif


/* 
 * compare two version number.
 * return value
 *  0	ver1 = ver2
 * <0	ver1 < ver2
 * >0	ver1 > ver2
 */
int
SaCkptVersionCompare(SaVersionT ver1, SaVersionT ver2)
{
	if (ver1.releaseCode < ver2.releaseCode) {
		return -1;
	} else if (ver1.releaseCode > ver2.releaseCode) {
		return 1;
	} else if (ver1.major < ver2.major) {
		return -1;
	} else if (ver1.major > ver2.major) {
		return 1;
	} else if ((ver1.minor == 0xff) || (ver2.minor == 0xff)) {
		/* do not care about the minor number */
		return 0;
	} else if (ver1.minor < ver2.minor) {
		return -1;
	} else if (ver1.minor > ver2.minor) {
		return 1;
	} else {
		return 0;
	}
}

/* convert version to string */
void 
SaCkptPackVersion(char* strVer, SaVersionT* ver)
{
	sprintf(strVer, "%c-%c-%c", ver->releaseCode, 
		ver->major, ver->minor);

	return ;
}

/* convert string to version */
void
SaCkptUnpackVersion(const char* strVer, SaVersionT *ver)
{
	if (strVer == NULL) {
		ver->releaseCode = 0;
		ver->major = 0;
		ver->minor = 0;
	} else {
		sscanf(strVer, "%c-%c-%c", &ver->releaseCode,
			&ver->major, &ver->minor);
	}

	return ;
}


char* 
SaCkptErr2String(SaErrorT retVal)
{
	char *strErr = NULL;
	char *strTemp = NULL;

	strTemp = (char*)calloc(1, 256);
	if (strTemp == NULL) {
		return NULL;
	}

	switch (retVal) {
	case SA_OK:
		strcpy(strTemp, "SA_OK");
		break;
	case SA_ERR_LIBRARY:
		strcpy(strTemp, "SA_ERR_LIBRARY");
		break;
	case SA_ERR_VERSION:
		strcpy(strTemp, "SA_ERR_VERSION");
		break;
	case SA_ERR_INIT:
		strcpy(strTemp, "SA_ERR_INIT");
		break;
	case SA_ERR_TIMEOUT:
		strcpy(strTemp, "SA_ERR_TIMEOUT");
		break;
	case SA_ERR_TRY_AGAIN:
		strcpy(strTemp, "SA_ERR_TIMEOUT");
		break;
	case SA_ERR_INVALID_PARAM:
		strcpy(strTemp, "SA_ERR_INVALID_PARAM");
		break;
	case SA_ERR_NO_MEMORY:
		strcpy(strTemp, "SA_ERR_NO_MEMORY");
		break;
	case SA_ERR_BAD_HANDLE:
		strcpy(strTemp, "SA_ERR_BAD_HANDLE");
		break;
	case SA_ERR_BUSY:
		strcpy(strTemp, "SA_ERR_BUSY");
		break;
	case SA_ERR_ACCESS:
		strcpy(strTemp, "SA_ERR_ACCESS");
		break;
	case SA_ERR_NOT_EXIST:
		strcpy(strTemp, "SA_ERR_NOT_EXIST");
		break;
	case SA_ERR_NAME_TOO_LONG:
		strcpy(strTemp, "SA_ERR_NAME_TOO_LONG");
		break;
	case SA_ERR_EXIST:
		strcpy(strTemp, "SA_ERR_EXIST");
		break;
	case SA_ERR_NO_SPACE:
		strcpy(strTemp, "SA_ERR_NO_SPACE");
		break;
	case SA_ERR_INTERRUPT:
		strcpy(strTemp, "SA_ERR_INTERRUPT");
		break;
	case SA_ERR_SYSTEM:
		strcpy(strTemp, "SA_ERR_SYSTEM");
		break;
	case SA_ERR_NAME_NOT_FOUND:
		strcpy(strTemp, "SA_ERR_NAME_NOT_FOUND");
		break;
	case SA_ERR_NO_RESOURCES:
		strcpy(strTemp, "SA_ERR_NO_RESOURCES");
		break;
	case SA_ERR_NOT_SUPPORTED:
		strcpy(strTemp, "SA_ERR_NOT_SUPPORTED");
		break;
	case SA_ERR_BAD_OPERATION:
		strcpy(strTemp, "SA_ERR_BAD_OPERATION");
		break;
	case SA_ERR_FAILED_OPERATION:
		strcpy(strTemp, "SA_ERR_FAILED_OPERATION");
		break;
	case SA_ERR_MESSAGE_ERROR:
		strcpy(strTemp, "SA_ERR_MESSAGE_ERROR");
		break;
	case SA_ERR_NO_MESSAGE:
		strcpy(strTemp, "SA_ERR_NO_MESSAGE");
		break;
	case SA_ERR_QUEUE_FULL:
		strcpy(strTemp, "SA_ERR_QUEUE_FULL");
		break;
	case SA_ERR_QUEUE_NOT_AVAILABLE:
		strcpy(strTemp, "SA_ERR_QUEUE_NOT_AVAILABLE");
		break;
	case SA_ERR_BAD_CHECKPOINT:
		strcpy(strTemp, "SA_ERR_BAD_CHECKPOINT");
		break;
	case SA_ERR_BAD_FLAGS:
		strcpy(strTemp, "SA_ERR_BAD_FLAGS");
		break;
	}

	strErr = calloc(strlen(strTemp)+1, 1);
	if (strErr == NULL) {
		return NULL;
	}
	memcpy(strErr, strTemp, strlen(strTemp)+1);

	free(strTemp);

	return strErr;
}

/* alloc memory and clear it to zero */
void* SaCkptMalloc(int size) 
{
	void* p = NULL;
	p = malloc(size);
	if (p != NULL) {
		memset(p, 0, size);
	}

	return p;
}

/* free memory and set pointer to NULL */
void 
SaCkptFree(void** p)
{
	if (p != NULL) {
		if (*p != NULL) {
			free(*p);
		}
		*p = NULL;
	}

	return;
}


