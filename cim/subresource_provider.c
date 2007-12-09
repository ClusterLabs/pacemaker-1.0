/*
 * sub_resource_provider.c: HA_SubResource provider
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

#include <hb_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <hb_api.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID  = "cim-sub-res";
static CMPIBroker * Broker     = NULL;
static char ClassName       	[] = "HA_SubResource"; 
static char Left            	[] = "Antecedent";
static char Right           	[] = "Dependent"; 
static char LeftClassName	[] = "HA_ClusterResource"; 
static char RightClassName	[] = "HA_ClusterResource";

static char PrimitiveClassName	[] = "HA_PrimitiveResource";
static char GroupClassName	[] = "HA_ResourceGroup";
static char CloneClassName	[] = "HA_ResourceClone";
static char MasterClassName	[] = "HA_MasterSlaveResource";
static char UnknownClassName	[] = "Unknown";

static CMPIArray * 	make_subrsc_array(CMPIBroker * broker, 
				char * classname, CMPIContext * ctx,
                    		char * namespace, char * target_name, 
				char * target_role, 
				CMPIObjectPath * source_op, CMPIStatus * rc);
static CMPIArray * 	make_resource_array(CMPIBroker * broker, char * classname,
				CMPIContext * ctx, char * namespace, 
				char * target_name, char * target_role,
                		CMPIObjectPath * source_op, CMPIStatus * rc);
static int 		subresource(CMPIBroker * broker, char * classname, 
				CMPIContext * ctx, CMPIObjectPath * group_op, 
				CMPIObjectPath * res_op, CMPIStatus * rc);

DeclareInstanceFunctions(SubResource);
DeclareAssociationFunctions(SubResource);


/* make an array with all sub resources of source_op */
static CMPIArray *
make_subrsc_array(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role, 
                    CMPIObjectPath * source_op, CMPIStatus * rc)
{
        CMPIArray * array;
	struct ha_msg *sublist;
	const char * key[] = {	"Id", "CreationClassName", "SystemName", 
				"SystemCreationClassName"};

	char *id, *crname, *sysname, * syscrname;
        int i, len;


        /* get keys */
	id        = CMGetKeyString(source_op, key[0], rc);
	crname    = CMGetKeyString(source_op, key[1], rc);
	sysname   = CMGetKeyString(source_op, key[2], rc);
	syscrname = CMGetKeyString(source_op, key[3], rc);

        if ( strcmp(crname, PrimitiveClassName) == 0 ) {
                return CMNewArray(broker, 0, CMPI_ref, rc);
        }

	if ((sublist = cim_get_subrsc_list(id)) == NULL ) {
		cl_log(LOG_ERR, "%s: can't find subresource of %s",
			__FUNCTION__, id);
                return NULL;
        }
       
        /* create a array to hold the object path */
	len = cim_list_length(sublist);
        array = CMNewArray(broker, len, CMPI_ref, rc);
        
        /* for each sub primitive resource */
        for ( i = 0; i < len; i++ ) {
                uint32_t rsctype;
                CMPIObjectPath * target_op;
                char * 	subid;
                
                subid = cim_list_index(sublist, i);
                rsctype = cim_get_rsctype(subid);

                if (rsctype == TID_RES_PRIMITIVE ) {
                        crname = PrimitiveClassName;
                } else if ( rsctype == TID_RES_GROUP ) {
                        crname = GroupClassName;
                } else if ( rsctype == TID_RES_GROUP ) {
                        crname = CloneClassName;
                } else if ( rsctype == TID_RES_MASTER ) {
                        crname = MasterClassName;
                } else {
                        crname = UnknownClassName;
                }
               
                /* create object path */
                target_op = CMNewObjectPath(broker, namespace, crname, rc);
                if ( CMIsNullObject(target_op) ) {
                        continue;
                }
                /* set keys */
                CMAddKey(target_op, "Id", subid, CMPI_chars);
                CMAddKey(target_op, "SystemName", sysname, CMPI_chars);
                CMAddKey(target_op, "SystemCreationClassName", syscrname, CMPI_chars);
                CMAddKey(target_op, "CreationClassName", crname, CMPI_chars);
                
                /* add to array */
                CMSetArrayElementAt(array, i, &target_op, CMPI_ref);
        }
        
	ha_msg_del(sublist);
        return array;
}

static CMPIArray *
make_resource_array(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                char * namespace, char * target_name, char * target_role,
                CMPIObjectPath * source_op, CMPIStatus * rc)
{
        if ( target_name == NULL ) {
                cl_log(LOG_ERR, "%s: target name can't be NULL", __FUNCTION__);
                return NULL;
        }

        if ( strcmp(target_role, Left) == 0 ) {	
		/* target is resource group */
		return cmpi_instance_names(broker,namespace, 
				target_name, ctx, rc);
        } else if ( strcmp(target_role, Right) == 0 ) {	
		/* target is primitive */
                return make_subrsc_array(broker, classname, ctx, namespace, 
				target_name, target_role, source_op, rc);
        }
        
        /* else */
        return NULL;
}

static int 
subresource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * group_op, CMPIObjectPath * res_op,  
              CMPIStatus * rc)
{
        char *rscid, *groupid;
        int i, len;
	struct ha_msg * sublist;

        rscid   = CMGetKeyString(res_op, "Id", rc);
        groupid = CMGetKeyString(group_op, "Id", rc);

	if ( rscid == NULL || groupid == NULL ) {
		return FALSE;
	}

        /* get the this group's sub resources */
	if ((sublist = cim_get_subrsc_list(groupid)) == NULL ) {
                return 0;
        }

        /* looking for the rsc */
	len = cim_list_length(sublist); 
        for ( i = 0; i < len; i++ ) {
                char * subid = cim_list_index(sublist, i);
                if ( strncmp(rscid, subid?subid:"", MAXLEN ) == 0 ) {
                        /* found */
                        return TRUE;
                }
        }
	ha_msg_del(sublist);
	return FALSE;
}

/**********************************************
 * Instance 
 **********************************************/
static CMPIStatus 
SubResourceCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
SubResourceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}

        assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName, 
			subresource, make_resource_array, FALSE, &rc);
	return rc;
}

static CMPIStatus 
SubResourceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                subresource, make_resource_array, TRUE, &rc);
	return rc;
}

static CMPIStatus 
SubResourceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}

        assoc_get_inst(Broker, ClassName, ctx, rslt, cop, Left, Right, &rc);
      	return rc; 
}

static CMPIStatus 
SubResourceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
SubResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}

static CMPIStatus 
SubResourceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        return rc;
}

static CMPIStatus 
SubResourceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                      CMPIResult * rslt, CMPIObjectPath *ref,
                      char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/****************************************************
 * Association
 ****************************************************/
static CMPIStatus 
SubResourceAssociationCleanup(CMPIAssociationMI * mi, CMPIContext * ctx)
{

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
SubResourceAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                        CMPIResult * rslt,  CMPIObjectPath * cop, 
                        const char * assoc_class, const char * result_class,
                        const char * role, const char * result_role, 
                        char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, subresource, make_resource_array, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
SubResourceAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * assoc_class, const char * result_class,
                            const char * role, const char * result_role)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, subresource, make_resource_array, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }      

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
SubResourceReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                      CMPIResult * rslt, CMPIObjectPath * cop, 
                      const char * result_class, const char * role, 
                      char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER(); 
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, subresource, 
                                make_resource_array, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
SubResourceReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
	if (cim_get_hb_status() != HB_RUNNING ) {
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, subresource, 
                                make_resource_array, 0, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}                

/***************************************
 * installed MIs
 **************************************/

DeclareInstanceMI(SubResource, HA_SubResourceProvider, Broker);
DeclareAssociationMI(SubResource, HA_SubResourceProvider, Broker);
