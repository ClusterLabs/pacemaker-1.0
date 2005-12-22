/*
 * inst_attribute_provider.c: HA_InstanceAttributes provider
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
#include <hb_api.h> 
#include "cmpi_utils.h"
#include "cluster_info.h"


/*
 resource has instance attributes
 operation has instance attributes
*/

#define PROVIDER_ID                 "cim-inst-attr"
static CMPIBroker * G_broker        = NULL;
static char G_classname []          = "HA_InstanceAttributes"; 

DeclareInstanceFunctions(InstanceAttributes);


/* sys_name is the resource id */
static CMPIInstance *
make_attrs_instance(CMPIObjectPath * op, char * id, char * sys_name, 
                    char * sys_cr_name, CMPIStatus * rc)
{

        CMPIInstance* ci = NULL;
        struct ci_table * attrs, * attr, * nvpair;
        struct ci_table * instattrs_tbl;
        int i;
        CMPIArray * array;
        char caption[256]; 

        ci = CMNewInstance(G_broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
                return NULL;
        }

        /* get inst atrributes table */
        if ((instattrs_tbl = ci_get_resource_instattrs_table(sys_name)) == NULL) {
                cl_log(LOG_ERR, "cmpi attrs: couldn't get instattrs_tbl");
                return NULL;
        }
        
        /* actually only one attributes in the table */
        if ( (attrs = instattrs_tbl->get_data_at(instattrs_tbl, 
                                                 0).value.table) == NULL ) {
                cl_log(LOG_ERR, "cmpi attrs: attrs is NULL ");
                return NULL;
        }
        /* only 1 attr in attrs, without key */
        attr = attrs->get_data_at(attrs, 0).value.table;

        if ( ( array =  CMNewArray(G_broker, attr->get_data_size(attr), 
                                   CMPI_chars, rc)) == NULL ) {
                cl_log(LOG_ERR, "cmpi attrs: coudn't make CMPIArray");
                instattrs_tbl->free(instattrs_tbl);
                return NULL;
        }

        cl_log(LOG_INFO, "attrs: ready to iterate, size = %d", 
               attr->get_data_size(attr));
        for ( i = 0; i < attr->get_data_size(attr); i++ ) {
                char * id, * name, * value, * tmp;
                int len;
                nvpair = attr->get_data_at(attr, i).value.table;
                if ( nvpair == NULL ) { continue; }

                id = nvpair->get_data(nvpair, "id").value.string;
                name = nvpair->get_data(nvpair, "name").value.string;
                value = nvpair->get_data(nvpair, "value").value.string;

                cl_log(LOG_INFO, "cmpi attrs: id, name, value = %s, %s, %s",
                       id, name, value);
                len = strlen("id") + strlen("name") + strlen("value")
                      + strlen(id) + strlen(name) + strlen(value) + 8;
                if ((tmp = (char *) CIM_MALLOC(len)) == NULL ) {
                        continue;
                }
                sprintf(tmp, "id=%s,name=%s,value=%s", id, name, value);
                CMSetArrayElementAt(array, i, &tmp, CMPI_chars);
        }
        
        sprintf(caption, "Attributes.%s", id);
        CMSetProperty(ci, "NVPairs", &array, CMPI_charsA);
        CMSetProperty(ci, "Id", id, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", G_classname, CMPI_chars);
        CMSetProperty(ci, "SystemName", sys_name, CMPI_chars);
        CMSetProperty(ci, "SystemCreationClassName", sys_cr_name, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

        instattrs_tbl->free(instattrs_tbl);

        return ci;
}


static int
get_inst_attrs(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
               char ** properties, CMPIStatus * rc)
{
        CMPIObjectPath * op;
        CMPIString * cmpi_id, * cmpi_sys_name, * cmpi_sys_cr_name;
        char * id, * sys_name, * sys_cr_name;
        CMPIInstance * ci;

        /* currently Id has no meaning */

         if ((cmpi_id = CMGetKey(cop, "Id", rc).value.string) == NULL ) {
                cl_log(LOG_WARNING, "key Id is NULL");
                return HA_FAIL;
        }
        id = CMGetCharPtr(cmpi_id);


        if ((cmpi_sys_name = CMGetKey(cop, "SystemName", rc).value.string) 
            == NULL ) {
                cl_log(LOG_WARNING, "key SystemName is NULL");
                return HA_FAIL;
        }
        sys_name = CMGetCharPtr(cmpi_sys_name);

        if ((cmpi_sys_cr_name = 
             CMGetKey(cop, "SystemCreationClassName", rc).value.string)==NULL){
                cl_log(LOG_WARNING, "key SystemCreationClassName is NULL");
                return HA_FAIL;
        }


        sys_cr_name = CMGetCharPtr(cmpi_sys_cr_name);
        op = CMNewObjectPath(G_broker, CMGetCharPtr(CMGetNameSpace(cop, rc)), 
                             G_classname, rc);

        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "inst_attr: can not create object path.");
                return HA_FAIL;
        }


        ci = make_attrs_instance(op, id, sys_name, sys_cr_name, rc);

        if ( CMIsNullObject(ci) ) {
                return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        return HA_OK;
}


static CMPIStatus 
InstanceAttributesCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
InstanceAttributesEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                                    CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        /* not support enumerate operations */

        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
InstanceAttributesEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * ref, 
                                char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        /* not support enumerate operations */
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
InstanceAttributesGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              char ** properties)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        if ( get_inst_attrs(ctx, rslt, cop, properties, &rc) == HA_FAIL ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
InstanceAttributesCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * cop, 
                                 CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
InstanceAttributesSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
InstanceAttributesDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
InstanceAttributesExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * ref,
                            char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}
                
DeclareInstanceMI(InstanceAttributes, 
                  HA_InstanceAttributesProvider, G_broker);
