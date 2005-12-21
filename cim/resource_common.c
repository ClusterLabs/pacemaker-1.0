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
#include "cluster_info.h"
#include "resource_common.h"

static char sys_name              [] = "LinuxHACluster";
static char sys_cr_classname      [] = "HA_Cluster";
static char G_primitive_classname [] = "HA_PrimitiveResource";
static char G_group_classname     [] = "HA_ResourceGroup";
static char G_clone_classname     [] = "HA_ResourceClone";
static char G_master_classname    [] = "HA_MasterSlaveResource";

static void
set_primitive_properties(CMPIInstance * ci, const struct ci_table * info)
{
        char * hosting_node = NULL;
        char * id, * type, * class, * provider;

        id = info->get_data(info, "id").value.string;
        type = info->get_data(info, "type").value.string;
        class = info->get_data(info, "class").value.string;
        provider = info->get_data(info, "provider").value.string;

        /* setting properties */
        CMSetProperty(ci, "Type", type, CMPI_chars);
        CMSetProperty(ci, "ResourceClass", class, CMPI_chars);
        CMSetProperty(ci, "Provider", provider, CMPI_chars);

        
        /* get hosting node */
        if ( (hosting_node = ci_get_res_running_node(id)) != NULL ){
                cl_log(LOG_INFO, "Hosting node is %s", hosting_node);
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        } else {
               /* OpenWBEM will segment fault in HostedResource provider 
                  if "HostingNode" not set */
                hosting_node = CIM_STRDUP ("Unknown");
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        }

        CIM_FREE(hosting_node);
}

static void 
set_group_properties(CMPIInstance * ci, const struct ci_table * info)
{
        /* nothing to do */
}

static void 
set_clone_properties(CMPIInstance * ci, const struct ci_table * clone)
{
        char * notify, * ordered, * interleave;

        notify = clone->get_data(clone, "notify").value.string;
        ordered = clone->get_data(clone, "ordered").value.string;
        interleave = clone->get_data(clone, "interleave").value.string;
}

static void 
set_master_properties(CMPIInstance * ci, const struct ci_table * info)
{
        char * max_node_masters, * max_masters;
        
        max_node_masters =info->get_data(info, "max_node_masters").value.string;
        max_masters = info->get_data(info, "max_masters").value.string;

        CMSetProperty(ci, "MaxNodeMasters", max_node_masters, CMPI_chars);
        CMSetProperty(ci, "MaxMasters", max_masters, CMPI_chars);

}

static CMPIInstance *
make_res_inst(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
              struct ci_table * info, uint32_t type, CMPIStatus * rc) 
{
        CMPIInstance * ci = NULL;
        char * id , * status;
        char caption [256];

        if ( info == NULL ) return NULL;
        ci = CMNewInstance(broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }

        id = info->get_data(info, "id").value.string;

        sprintf(caption, "Resource.%s", id);
        /* set other key properties inherited from super classes */
        CMSetProperty(ci, "SystemCreationClassName", sys_cr_classname, CMPI_chars);
        CMSetProperty(ci, "SystemName", sys_name, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);      
        CMSetProperty(ci, "Id", id, CMPI_chars);
        /* set Caption */
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

        if (( status = ci_get_resource_status(id)) ) {
                CMSetProperty(ci, "ResourceStatus", status, CMPI_chars);
                CIM_FREE(status);
        }

        switch(type){
        case TID_RES_PRIMITIVE:
             set_primitive_properties(ci, info);
             break;
        case TID_RES_GROUP:
             set_group_properties(ci, info);
             break;
        case TID_RES_CLONE:
             set_clone_properties(ci, info);
             break;
        case TID_RES_MASTER:
             set_master_properties(ci, info);
             break;
        }
out:
        return ci;
}

static CMPIInstance *
make_res_inst_by_id(CMPIBroker * broker, CMPIObjectPath * ref, 
                    char * rsc_name, uint32_t type, CMPIStatus * rc) 
{
        CMPIInstance * ci = NULL;
        struct ci_table * info;
        char * namespace, * classname;
        CMPIObjectPath * op;

        if ( rsc_name == NULL ) return NULL;
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));

        cl_log(LOG_INFO, "%s: make instance for %s", __FUNCTION__, rsc_name);
        switch(type) {
        case TID_RES_PRIMITIVE:
                classname = G_primitive_classname;
                info = ci_get_primitive_resource(rsc_name);
                break;
        case TID_RES_GROUP:
                classname = G_group_classname;
                info = ci_get_resource_group(rsc_name);
                break;
        case TID_RES_CLONE:
                classname = G_clone_classname;
                info = ci_get_resource_clone(rsc_name);
                break;
        case TID_RES_MASTER:
                classname = G_master_classname;
                info = ci_get_master_resource(rsc_name);
                break;
        default:
             return NULL;
        }

        if ( info == NULL ) return NULL;

        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ) {
                return NULL;
        }

        ci = make_res_inst(broker, classname, op, info, type, rc);
        info->free(info);
        CMRelease(op);
        return ci;
}


static CMPIObjectPath *
make_res_op_by_id(CMPIBroker * broker, CMPIObjectPath * ref, 
                  char * rsc_name, uint32_t type, CMPIStatus * rc)
{
        char * namespace, * classname;
        CMPIObjectPath * op;

        if ( rsc_name == NULL ) return NULL;
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));

        switch(type) {
        case TID_RES_PRIMITIVE:
                classname = G_primitive_classname;
                break;
        case TID_RES_GROUP:
                classname = G_group_classname;
                break;
        case TID_RES_CLONE:
                classname = G_clone_classname;
                break;
        case TID_RES_MASTER:
                classname = G_master_classname;
                break;
        default:
             return NULL;
        }
        
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ) {
                return NULL;
        }

        CMAddKey(op, "Id", rsc_name, CMPI_chars);
        CMAddKey(op, "SystemName", sys_name, CMPI_chars);
        CMAddKey(op, "SystemCreationClassName", sys_cr_classname, CMPI_chars);
        CMAddKey(op, "CreationClassName", classname, CMPI_chars);

        return op;
}


int
get_inst_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * ref,
                  char ** properties, uint32_t type, CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIData data_name;
        char * rsc_name = NULL;
        int ret = 0;
        uint32_t this_type;

        data_name = CMGetKey(ref, "Id", rc);
        if ( data_name.value.string == NULL ) {
                cl_log(LOG_WARNING, "key %s is NULL", "Id");
                ret = HA_FAIL;
                goto out;
        }

        rsc_name = CMGetCharPtr(data_name.value.string);

        cl_log(LOG_INFO, "rsc_name = %s", rsc_name);
        this_type = type == 0 ? ci_get_resource_type(rsc_name) : type;

        ci = make_res_inst_by_id(broker, ref, rsc_name, this_type, rc);
        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                cl_log(LOG_WARNING, 
                        "%s: can not create instance.", __FUNCTION__);
                goto out;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;
out:
        return ret;
}


/* should return primitives in group for primitive provider? no */
int 
enum_inst_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                   CMPIResult * rslt, CMPIObjectPath * ref, int need_inst,
                   uint32_t type, CMPIStatus * rc)
{
        uint32_t this_type;
        int i;
        struct ci_table * table;

        if ( ( table = ci_get_resource_name_table ()) == NULL ) {
                return HA_FAIL;
        }

        for ( i = 0; i < CITableSize(table); i++) {
                char * rsc_id;
                rsc_id = CITableGetAt(table, i).value.string;
                if ( rsc_id == NULL ) continue;
                /* if type == 0, enum all resources */
                this_type =  ci_get_resource_type(rsc_id);
                if ( type != 0 && type != this_type) {
                        continue;
                }
                if ( need_inst ) {
                        CMPIInstance * ci = NULL;
                        ci = make_res_inst_by_id(broker, ref, rsc_id, 
                                                 this_type, rc); 
                        if ( CMIsNullObject(ci) ){
                                CITableFree(table);
                                return HA_FAIL;
                        }
                        
                        CMReturnInstance(rslt, ci);
                } else {
                        CMPIObjectPath * op;
                        op = make_res_op_by_id(broker, ref, rsc_id, 
                                               this_type, rc);
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }

        }

        CITableFree(table);
        CMReturnDone(rslt);
        return HA_OK;
}

int
resource_cleanup(CMPIBroker * broker, char * classname, CMPIInstanceMI * mi, 
                 CMPIContext * ctx, uint32_t type, CMPIStatus * rc)
{
        return HA_OK;
}
