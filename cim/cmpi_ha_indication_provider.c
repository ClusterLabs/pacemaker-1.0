/*
 * cmpi_ha_indication_provider.c: LinuxHA_Indication provider
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cmpi_utils.h"
#include "linuxha_info.h"
#include "cmpi_ha_indication.h"

#define PROVIDER_ID          "cim-ind"
static char ClassName []   = "LinuxHA_Indication";
static CMPIBroker * Broker = NULL;
static int ind_enabled     = 0;


/*----------- indication interfaces ----------*/
static CMPIStatus 
Indication_IndicationCleanup(CMPIIndicationMI * mi, CMPIContext * ctx);
static CMPIStatus 
Indication_AuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner);
static CMPIStatus 
Indication_MustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath * classPath);
static CMPIStatus 
Indication_ActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation);
static CMPIStatus 
Indication_DeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation);

/**************************************************
 * Indication Interface
 *************************************************/
static CMPIStatus 
Indication_IndicationCleanup(CMPIIndicationMI * mi, 
                CMPIContext * ctx)
{
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
Indication_AuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner)
{
        CMPIBoolean authorized = 1;
        char * filter_str = NULL;
        CMPIStatus rc;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();

        filter_str = CMGetCharPtr( CMGetSelExpString(filter, &rc) );

        cl_log(LOG_INFO, "%s: eventype = %s, filter = %s", 
               __FUNCTION__,type, filter_str);

        CMReturnData(rslt, (CMPIValue *)&authorized, CMPI_boolean);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
Indication_MustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath * classPath)
{
        CMPIStatus rc = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
        CMPIBoolean poll = 0;
        char * filter_str = NULL;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();

        filter_str = CMGetCharPtr( CMGetSelExpString(filter, &rc) );

        cl_log(LOG_INFO, "%s: eventype = %s, filter = %s", 
               __FUNCTION__, indType, filter_str);
        
        cl_log(LOG_INFO, "%s: does not suppot poll", __FUNCTION__);

        DEBUG_LEAVE();

        CMReturnData(rslt, (CMPIValue *)&poll, CMPI_boolean);
        CMReturnDone(rslt);

        return rc;
}

static CMPIStatus 
Indication_ActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation)
{

        CMPIBoolean activated = 1;
        CMPIStatus rc;

        init_logger(PROVIDER_ID);
        
        DEBUG_ENTER();
        
        if (  haind_activate(ClassName, Broker, ctx, rslt, filter, type, 
                             classPath, firstActivation, &rc) == HA_OK ) {

                DEBUG_LEAVE();
                CMReturnData(rslt, (CMPIValue *)&activated, CMPI_boolean);
                CMReturnDone(rslt);
 
                CMReturn(CMPI_RC_OK);
        } else {
                DEBUG_LEAVE();
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}

static CMPIStatus 
Indication_DeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation)
{
        CMPIBoolean deactivated = 1;
        CMPIStatus rc;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();

        if ( haind_deactivate(ClassName, Broker, ctx, rslt, filter, type, 
                              classPath, lastActivation, &rc) == HA_OK ) {
        
                DEBUG_LEAVE();
                CMReturnData(rslt, (CMPIValue *)&deactivated, CMPI_boolean);
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_OK);
        } else {
                DEBUG_LEAVE();
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}



static void 
Indication_EnableIndications(CMPIIndicationMI * mi )
{

        /* Enable indication generation */
        ind_enabled = 1;

}


static void 
Indication_DisableIndications(CMPIIndicationMI * mi )
{
        /* Disable indication generation */
        ind_enabled = 0;
}

/*****************************************************
 * Indication
 ****************************************************/
DeclareIndicationMI(Indication_, LinuxHA_IndicationProvider, Broker);


