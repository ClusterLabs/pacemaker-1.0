/*
 * CIM Provider
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

#define PROVIDER_ID "cim-ind"

static CMPIBroker * Broker = NULL;



/*----------- indication interfaces ----------*/
CMPIStatus LinuxHA_IndicationProviderIndicationCleanup(CMPIIndicationMI * mi, 
                CMPIContext * ctx);

CMPIStatus LinuxHA_IndicationProviderAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner);

CMPIStatus LinuxHA_IndicationProviderMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath * classPath);

CMPIStatus LinuxHA_IndicationProviderActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation);

CMPIStatus LinuxHA_IndicationProviderDeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation);




CMPIIndicationMI *
LinuxHA_IndicationProvider_Create_IndicationMI(CMPIBroker * brkr, CMPIContext * ctx);



/**************************************************
 * Indication Interface Implementaion
 *************************************************/
CMPIStatus 
LinuxHA_IndicationProviderIndicationCleanup(CMPIIndicationMI * mi, 
                CMPIContext * ctx)
{
        init_logger(PROVIDER_ID);
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_IndicationProviderAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner)
{

        CMPIValue valrc;

        init_logger(PROVIDER_ID);
        /*** debug ***/
        DEBUG_ENTER();

        valrc.boolean = 1;

        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_IndicationProviderMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath* classPath)
{
        
        CMPIValue valrc;
        valrc.boolean = 1;

        init_logger(PROVIDER_ID);
        /*** debug ***/
        DEBUG_ENTER();
        

        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);
        
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_IndicationProviderActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation)
{
        CMPIStatus rc;
        int ret = 0;

        init_logger(PROVIDER_ID);
        
        DEBUG_ENTER();
        
        ret = ha_indication_initialize(Broker, ctx, filter, &rc);

        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_IndicationProviderDeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation)
{
        int ret = 0;
        CMPIStatus rc;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = ha_indication_finalize(Broker, ctx, filter, &rc);        
        
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}



/*--------------------------------------------*/

static char ind_provider_name[] = "indLinuxHA_IndicationProvider";
static CMPIIndicationMIFT indMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        ind_provider_name,
        LinuxHA_IndicationProviderIndicationCleanup,
        LinuxHA_IndicationProviderAuthorizeFilter,
        LinuxHA_IndicationProviderMustPoll,
        LinuxHA_IndicationProviderActivateFilter,
        LinuxHA_IndicationProviderDeActivateFilter
};

CMPIIndicationMI *
LinuxHA_IndicationProvider_Create_IndicationMI(CMPIBroker * brkr, 
                                               CMPIContext * ctx)
{
        static CMPIIndicationMI mi = {
                NULL,
                &indMIFT
        };

        Broker = brkr;
        CMNoHook;
        return &mi;
}

