#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <hb_api.h>
#include "cmpi_utils.h"
#include "linuxha_info.h"
#include "cmpi_node.h"


#define PROVIDER_ID "cim-provider-node"

static CMPIBroker * Broker;
static char ClassName[] = "LinuxHA_ClusterNode";


/***** 	prototype declare  ***/

CMPIStatus 
LinuxHA_ClusterNodeProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_ClusterNodeProviderEnumInstanceNames(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref);
CMPIStatus 
LinuxHA_ClusterNodeProviderEnumInstances(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char ** properties);

CMPIStatus 
LinuxHA_ClusterNodeProviderGetInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath *cop,
		char ** properties);

CMPIStatus 
LinuxHA_ClusterNodeProviderCreateInstance(CMPIInstanceMI* mi,
		CMPIContext *ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop,
		CMPIInstance* ci);


CMPIStatus 
LinuxHA_ClusterNodeProviderSetInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop,
		CMPIInstance* ci,
		char ** properties);

CMPIStatus 
LinuxHA_ClusterNodeProviderDeleteInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop);
CMPIStatus 
LinuxHA_ClusterNodeProviderExecQuery(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char* lang,
		char* query);

CMPIStatus 
LinuxHA_ClusterNodeProviderInvokeMethod(CMPIMethodMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char* methodName,
		CMPIArgs* in,
		CMPIArgs* out);



CMPIStatus 
LinuxHA_ClusterNodeProviderMethodCleanup(CMPIMethodMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_ClusterNodeProviderIndicationCleanup(
	CMPIInstanceMI * mi, 
	CMPIContext * ctx);

CMPIStatus 
LinuxHA_ClusterNodeProviderAuthorizeFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	const char * owner);

CMPIStatus 
LinuxHA_ClusterNodeProviderMustPoll(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * indType,
	CMPIObjectPath* classPath);

CMPIStatus 
LinuxHA_ClusterNodeProviderActivateFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	CMPIBoolean firstActivation);

CMPIStatus 
LinuxHA_ClusterNodeProviderDeActivateFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	CMPIBoolean lastActivation);

CMPIInstanceMI * 
LinuxHA_ClusterNodeProvider_Create_InstanceMI(CMPIBroker * brkr, 
                        CMPIContext * ctx);

CMPIAssociationMI *
LinuxHA_ClusterNodeProvider_Create_AssociationMI(CMPIBroker* brkr,CMPIContext *ctx);


/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_ClusterNodeProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{

        cleanup_node();
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterNodeProviderEnumInstanceNames(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret;
	
        init_logger( PROVIDER_ID );
	ret = enumerate_clusternode_instances(ClassName, Broker, 
                                mi, ctx, rslt, ref, 0, &rc);

        if ( ret == HA_OK ) {
	        CMReturn(CMPI_RC_OK);	
        } else {

                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }
}


CMPIStatus 
LinuxHA_ClusterNodeProviderEnumInstances(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret;
	
        init_logger( PROVIDER_ID );
	ret = enumerate_clusternode_instances(ClassName, Broker, 
                                mi, ctx, rslt, ref, 1, &rc);

        if ( ret == HA_OK ) {
	        CMReturn(CMPI_RC_OK);	
        } else {

                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }

}


CMPIStatus 
LinuxHA_ClusterNodeProviderGetInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath *cop,
		char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger( PROVIDER_ID );

        DEBUG_ENTER();

	ret = get_clusternode_instance(ClassName, Broker, ctx, rslt, cop, 
                                properties, &rc);

        if ( ret != HA_OK ){
                cl_log(LOG_WARNING, "%s: NULL instance", __FUNCTION__);

                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }

        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
LinuxHA_ClusterNodeProviderCreateInstance(CMPIInstanceMI* mi,
		CMPIContext *ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop,
		CMPIInstance* ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


CMPIStatus 
LinuxHA_ClusterNodeProviderSetInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop,
		CMPIInstance* ci,
		char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


CMPIStatus 
LinuxHA_ClusterNodeProviderDeleteInstance(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
LinuxHA_ClusterNodeProviderExecQuery(CMPIInstanceMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char* lang,
		char* query)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


/**************************************************
 * Method Provider 
 *************************************************/
CMPIStatus 
LinuxHA_ClusterNodeProviderInvokeMethod(CMPIMethodMI* mi,
		CMPIContext* ctx,
		CMPIResult* rslt,
		CMPIObjectPath* ref,
		char* methodName,
		CMPIArgs* in,
		CMPIArgs* out)
{

	CMPIString* class_name = NULL;
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMPIData arg_data;
	CMPIValue valrc;
	
	class_name = CMGetClassName(ref, &rc);
	arg_data = CMGetArg(in, "DirPathName", &rc);
	
	if(strcasecmp(CMGetCharPtr(class_name), ClassName) == 0 &&
		strcasecmp("Send_message_to_all", methodName) == 0 ){
		char* strArg = CMGetCharPtr(arg_data.value.string);
		char cmd[] = "wall ";

		strcat(cmd, strArg);
		valrc.uint32 = system(cmd);
	}

	CMReturnData(rslt, &valrc, CMPI_uint32);
	CMReturnDone(rslt);

	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterNodeProviderMethodCleanup(CMPIMethodMI* mi, CMPIContext* ctx)
{
	CMReturn(CMPI_RC_OK);
}


/**************************************************
 * Indication Interface Implementaion
 *************************************************/


CMPIStatus 
LinuxHA_ClusterNodeProviderIndicationCleanup(
	CMPIInstanceMI * mi, 
	CMPIContext * ctx)
{
	CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterNodeProviderAuthorizeFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	const char * owner)
{

	CMPIValue valrc;
	valrc.boolean = 1;

	CMReturnData(rslt, &valrc, CMPI_boolean);
	CMReturnDone(rslt);


	/*** debug ***/
        DEBUG_ENTER();
        DEBUG_LEAVE();	
	CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterNodeProviderMustPoll(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * indType,
	CMPIObjectPath* classPath)
{
	
	CMPIValue valrc;
	valrc.boolean = 1;

	CMReturnData(rslt, &valrc, CMPI_boolean);
	CMReturnDone(rslt);
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterNodeProviderActivateFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	CMPIBoolean firstActivation)
{


        DEBUG_ENTER();
        DEBUG_LEAVE();	
	CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterNodeProviderDeActivateFilter(
	CMPIIndicationMI * mi,
	CMPIContext * ctx,
	CMPIResult * rslt,
	CMPISelectExp * filter,
	const char * type,
	CMPIObjectPath * classPath,
	CMPIBoolean lastActivation)
{


	CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install interface
 ****************************************************/

static char inst_provider_name[] = "instanceLinuxHA_ClusterNodeProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        LinuxHA_ClusterNodeProviderCleanup,
        LinuxHA_ClusterNodeProviderEnumInstanceNames,
        LinuxHA_ClusterNodeProviderEnumInstances,
        LinuxHA_ClusterNodeProviderGetInstance,
        LinuxHA_ClusterNodeProviderCreateInstance,
        LinuxHA_ClusterNodeProviderSetInstance, 
        LinuxHA_ClusterNodeProviderDeleteInstance,
        LinuxHA_ClusterNodeProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterNodeProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        CMNoHook;
        return &mi;
}


/*----------------------------------------------------------------*/

