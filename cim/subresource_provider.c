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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
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
static CMPIBroker * G_broker     = NULL;
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

static CMPIArray * 	enum_func_for_right(CMPIBroker * broker, 
				char * classname, CMPIContext * ctx,
                    		char * namespace, char * target_name, 
				char * target_role, 
				CMPIObjectPath * source_op, CMPIStatus * rc);
static CMPIArray * 	group_enum_func(CMPIBroker * broker, char * classname,
				CMPIContext * ctx, char * namespace, 
				char * target_name, char * target_role,
                		CMPIObjectPath * source_op, CMPIStatus * rc);
static int 		group_contain(CMPIBroker * broker, char * classname, 
				CMPIContext * ctx, CMPIObjectPath * group_op, 
				CMPIObjectPath * res_op, CMPIStatus * rc);

DeclareInstanceFunctions(SubResource);
DeclareAssociationFunctions(SubResource);


static CMPIArray *
enum_func_for_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role, 
                    CMPIObjectPath * source_op, CMPIStatus * rc)
{
        CMPIArray * array;
	CIMArray * sublist;
	const char * key[] = {	"Id", "CreationClassName", 
				"SystemName", "SystemCreationClassName"};
	char *id, *crname, *sysname, * syscrname;
        int i;

        /* we need to make a enumeration that contains sub resource
           according to source_op */
        /* get keys */
	id = CMGetKeyString(source_op, key[0], rc);
	crname = CMGetKeyString(source_op, key[1], rc);
	sysname = CMGetKeyString(source_op, key[2], rc);
	syscrname = CMGetKeyString(source_op, key[3], rc);

        if ( strcmp(crname, PrimitiveClassName) == 0 ) {
                return CMNewArray(broker, 0, CMPI_ref, rc);
        }
        if ((sublist = cim_get_array(GET_SUB_RSC, id, NULL))== NULL) {
                return NULL;
        }
        
        /* create a array to hold the object path */
        array = CMNewArray(broker, cim_array_len(sublist), CMPI_ref, rc);
        
        /* for each sub primitive resource */
        for ( i = 0; i < cim_array_len(sublist); i ++ ) {
                uint32_t rsctype;
                CMPIObjectPath * target_op;
                char * 	subid;
                
                subid = cim_array_index_v(sublist,i).v.str;
                rsctype = cim_get_resource_type(subid);
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
        
	cim_array_free(sublist);
        return array;
}

static CMPIArray *
group_enum_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                char * namespace, char * target_name, char * target_role,
                CMPIObjectPath * source_op, CMPIStatus * rc)
{
        if ( target_name == NULL ) {
                cl_log(LOG_ERR, "target name can not be NULL");
                return NULL;
        }

        if ( strcmp(target_role, Left) == 0 ) {
                CMPIArray * left_array;
                left_array = cmpi_instance_names(broker,namespace, 
					target_name, ctx, rc);
                if ( CMIsNullObject(left_array) ) {
                        cl_log(LOG_ERR, "group array is NULL ");
                        return NULL;
                }
                return left_array;
        } else if ( strcmp(target_role, Right) == 0 ) {
                return enum_func_for_right(broker, classname,
				ctx, namespace, target_name, target_role, 
                                source_op, rc);
        }
        
        /* else */
        return NULL;
}

static int 
group_contain(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * group_op, CMPIObjectPath * res_op,  
              CMPIStatus * rc)
{
        char *rscid, *groupid;
        int i;
	CIMArray * sublist;

	DEBUG_ENTER();
        rscid = CMGetKeyString(res_op, "Id", rc);
        groupid = CMGetKeyString(group_op, "Id", rc);

	if ( rscid == NULL || groupid == NULL ) {
		return 0;
	}
        /* get the sub resource of this group */
        if ((sublist = cim_get_array(GET_SUB_RSC, groupid, NULL)) == NULL ){
                return 0;
        }
        
        /* looking for the rsc */
        for ( i = 0; i < cim_array_len(sublist); i++ ) {
                char * subid;
                subid = cim_array_index_v(sublist, i).v.str;
                if ( strcmp(subid, rscid ) == 0 ) {
                        /* found */
                        return 1;
                }
        }
	cim_array_free(sublist);
	DEBUG_LEAVE();
	return 0;
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

        assoc_enum_insts(G_broker, ClassName, ctx, rslt, cop, 
				Left, Right, LeftClassName, RightClassName, 
                                group_contain, group_enum_func, FALSE, &rc);
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
        assoc_enum_insts(G_broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                group_contain, group_enum_func, TRUE, &rc);
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

        assoc_get_inst(G_broker, ClassName, ctx, rslt, cop, Left, Right, &rc);
      	return rc; 
}

static CMPIStatus 
SubResourceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
SubResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
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
        CMSetStatusWithChars(G_broker, &rc, 
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
        if (assoc_enum_associators(G_broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, group_contain, group_enum_func, 1, 
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
        if (assoc_enum_associators(G_broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, group_contain, group_enum_func, 0, 
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
        if ( assoc_enum_references(G_broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, group_contain, 
                                group_enum_func, 1, &rc) != HA_OK ) {
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
        if ( assoc_enum_references(G_broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, group_contain, 
                                group_enum_func, 0, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}                

/***************************************
 * installed MIs
 **************************************/

DeclareInstanceMI(SubResource, HA_SubResourceProvider, G_broker);
DeclareAssociationMI(SubResource, HA_SubResourceProvider, G_broker);
