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
#include "cmpi_utils.h"

static char 	SystemName         [] = "LinuxHACluster";
static char 	SystemClassName    [] = "HA_Cluster";
static char 	PrimitiveClassName [] = "HA_PrimitiveResource";
static char 	GroupClassName     [] = "HA_ResourceGroup";
static char 	CloneClassName     [] = "HA_ResourceClone";
static char 	MasterClassName    [] = "HA_MasterSlaveResource";

static void     primitive_set_properties(CMPIBroker * broker, 
			CMPIInstance * ci, CIMTable *, CMPIStatus *);
static void     group_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
			CIMTable *, CMPIStatus *);
static void     clone_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
			CIMTable *, CMPIStatus *);
static void     master_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
			CIMTable *, CMPIStatus *);
static void     instance_set_attributes(CMPIBroker * broker, CMPIInstance * ci,
			const char * rscid, CMPIStatus *);

static int      primitive_get_properties(CIMTable *, CMPIInstance *, CMPIStatus *);
static int      clone_get_properties(CIMTable *, CMPIInstance *, CMPIStatus *);
static int      master_get_properties(CIMTable *, CMPIInstance *, CMPIStatus *);
static int      group_get_properties(CIMTable *, CMPIInstance *, CMPIStatus *);

static CIMArray *	instance_get_attributes(CMPIObjectPath *, 
					CMPIInstance *, CMPIStatus *);
static CMPIInstance *   make_instance(CMPIBroker * broker, char * classname, 
					CMPIObjectPath * op, CIMTable * info, 
					uint32_t type, CMPIStatus * rc);
static CMPIInstance *   make_instance_byid(CMPIBroker * broker, 
					CMPIObjectPath * ref, char * rscid, 
					uint32_t type, CMPIStatus * rc);
static CMPIObjectPath * make_objectpath_byid(CMPIBroker * broker, 
					CMPIObjectPath * ref, char * rscid, 
					uint32_t type, CMPIStatus * rc);

static void     
instance_set_attributes(CMPIBroker * broker, CMPIInstance * ci,
			const char * rscid, CMPIStatus * rc)
{
	CIMArray * attrs = NULL;
	CMPIArray * array = NULL;
        int len = 0, i = 0;
	if ((attrs = cim_get_array(GET_RSC_ATTRIBUTES, rscid, NULL)) == NULL) {
                cl_log(LOG_ERR, "Resource attribute: can't get attributes.");
                return;
        }

        len = cim_array_len(attrs);
        if ( ( array = CMNewArray(broker, len, CMPI_chars, rc)) == NULL ) {
                cl_log(LOG_ERR, "Resource attribute: can't make CMPIArray.");
                cim_array_free(attrs);
                return;
        }

        for ( i = 0; i < cim_array_len(attrs); i++ ) {
                char buf[MAXLEN];
                char * id, * name, *value, *p;
                CIMTable *attribute;

                attribute = cim_array_index_v(attrs,i).v.table;
                if ( attribute == NULL ) {
                        continue;
                }

                id    = cim_table_lookup_v(attribute, "id").v.str;
                name  = cim_table_lookup_v(attribute, "name").v.str;
                value = cim_table_lookup_v(attribute, "value").v.str;
                snprintf(buf, MAXLEN, "%s=%s", name, value);

                if ( (p = cim_strdup(buf)) ){
                        CMSetArrayElementAt(array, i, &p, CMPI_chars);
                }
        }
	CMSetProperty(ci, "InstanceAttributes", &array, CMPI_charsA);

}

static CIMArray *
instance_get_attributes(CMPIObjectPath* cop, CMPIInstance* ci, CMPIStatus* rc)
{
        char *id, *rscid, *rsc_crname;
        CMPIArray * array;
        int i, len;
        CIMArray * attrs;

        DEBUG_ENTER();
        id = CMGetKeyString(cop, "Id", rc);
        rscid = CMGetKeyString(cop, "SystemName", rc);
        rsc_crname = CMGetKeyString(cop, "SystemCreationClassName", rc);

        array = CMGetProperty(ci, "InstanceAttributes", rc).value.array;
        if ( array == NULL ) {
		cl_log(LOG_ERR, "instance_get_attrs: attributes missing.");
		return NULL;
        }
        len = CMGetArrayCount(array, rc);
        if ( rc->rc != CMPI_RC_OK) {
                cl_log(LOG_ERR, "instance_get_attrs: can't get array length.");
                return NULL;
        }

        if ((attrs = cim_array_new()) == NULL ) {
		cl_log(LOG_ERR, "instance_get_attrs: failed to alloc array.");
                return NULL;
        }

        for(i = 0; i < len; i++) {
                CMPIData data;
                CIMTable * table;
                char **s, *v, tmp[MAXLEN] = "id-";
                int len;

                data = CMGetArrayElementAt(array, i, rc);
                if ( rc->rc != CMPI_RC_OK ) {
                        continue;
                }
                v = CMGetCharPtr(data.value.string);

                if((table = cim_table_new()) == NULL ) {
                        continue;
                }
		
		/* parse attributes, get key and value */
                s = split_string(v, &len, " =");
                if ( len == 2 ) {
                        cim_table_strdup_replace(table, "name", s[0]);
                        cim_table_strdup_replace(table, "value", s[1]);
                }

		strncat(tmp, s[0], MAXLEN);
		cim_table_strdup_replace(table, "id", tmp);
                free_2d_array(s, len, cim_free);

                dump_cim_table(table, 0);
                cim_array_append(attrs, makeTableData(table));
        }

        DEBUG_LEAVE();
	return attrs;
}

static void
primitive_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
		CIMTable * info, CMPIStatus * rc)
{
	const mapping_t map [] = { MAPPING_HA_PrimitiveResource};
	int len = MAPDIM(map);
        char *hosting_node = NULL, *id;

	cmpi_set_properties(broker, ci, info, map, len, rc);
	id = cim_table_lookup_v(info, "id").v.str;

        /* get hosting node */
        if ( (hosting_node = cim_get(GET_RSC_HOST, id, NULL)) != NULL ){
                cl_log(LOG_INFO, "Hosting node is %s", hosting_node);
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        } else {
               /* OpenWBEM will segment fault in HostedResource provider 
                  if "HostingNode" not set */
                hosting_node = cim_strdup ("Unknown");
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        }
        cim_free(hosting_node);
}

static void 
group_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
		CIMTable * info, CMPIStatus * rc)
{
        /* nothing to do */
}

static void 
clone_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
		CIMTable * clone, CMPIStatus * rc)
{
	const mapping_t map [] = { MAPPING_HA_ResourceClone };
	int len = MAPDIM(map);
	cmpi_set_properties(broker, ci, clone, map, len, rc);
}

static void 
master_set_properties(CMPIBroker * broker, CMPIInstance * ci, 
		CIMTable * master, CMPIStatus * rc)
{
	const mapping_t map [] = { MAPPING_HA_MasterSlaveResource};
	int len = MAPDIM(map);
	cmpi_set_properties(broker, ci, master, map, len, rc);
}

static CMPIInstance *
make_instance(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
              CIMTable * info, uint32_t type, CMPIStatus * rc) 
{
        CMPIInstance * ci = NULL;
        char * id;
	char * status;
        char caption [MAXLEN];

        ci = CMNewInstance(broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: Can't create instance.", __FUNCTION__);
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }

	id = cim_table_lookup_v(info, "id").v.str;

        snprintf(caption, MAXLEN, "Resource.%s", id);
        /* set other key properties inherited from super classes */
        CMSetProperty(ci, "SystemCreationClassName", 
			      SystemClassName, CMPI_chars);
        CMSetProperty(ci, "SystemName", SystemName, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);      
        CMSetProperty(ci, "Id", id, CMPI_chars);
	
        /* set Caption */
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
        if (( status = cim_get_str(GET_RSC_STATUS, id, NULL)) ) {
                CMSetProperty(ci, "ResourceStatus", status, CMPI_chars);
                cim_free(status);
        }

	instance_set_attributes(broker, ci, id, rc);
        switch(type){
        case TID_RES_PRIMITIVE:
             primitive_set_properties(broker, ci, info, rc);
             break;
        case TID_RES_GROUP:
             group_set_properties(broker, ci, info, rc);
             break;
        case TID_RES_CLONE:
             clone_set_properties(broker, ci, info, rc);
             break;
        case TID_RES_MASTER:
             master_set_properties(broker, ci, info, rc);
             break;
        }
out:
        return ci;
}

static CMPIInstance *
make_instance_byid(CMPIBroker * broker, CMPIObjectPath * ref, 
                    char * rscid, uint32_t type, CMPIStatus * rc) 
{
        CMPIInstance * ci = NULL;
        char * namespace, * classname;
        CMPIObjectPath * op;
	CIMTable * info;

        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
        switch(type) {
        case TID_RES_PRIMITIVE:
                classname = PrimitiveClassName;
                info = cim_get(GET_PRIMITIVE, rscid, NULL);
                break;
        case TID_RES_GROUP:
                classname = GroupClassName;
                if ((info = cim_table_new())) {
			cim_table_strdup_replace(info, "id", rscid);
		}
                break;
        case TID_RES_CLONE:
                classname = CloneClassName;
                info = cim_get(GET_CLONE, rscid, NULL);
                break;
        case TID_RES_MASTER:
                classname = MasterClassName;
                info = cim_get(GET_MASTER, rscid, NULL);
                break;
        default:
             return NULL;
        }

        if ( info == NULL ) { 
		cl_log(LOG_ERR, "%s: failed to get resource", __FUNCTION__);
		return NULL;
	}

        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ) {
		cl_log(LOG_ERR, "%s: can't create objectpath", __FUNCTION__);
                return NULL;
        }

        ci = make_instance(broker, classname, op, info, type, rc);
        cim_table_free(info);
	CMRelease(op);
        return ci;
}


static CMPIObjectPath *
make_objectpath_byid(CMPIBroker * broker, CMPIObjectPath * ref, 
                  char * rscid, uint32_t type, CMPIStatus * rc)
{
        char * 		 namespace;
	char * 		 classname;
        CMPIObjectPath * op;

	DEBUG_ENTER();
        if ( rscid == NULL )  { return NULL; }
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));

        switch(type) {
        case TID_RES_PRIMITIVE:
                classname = PrimitiveClassName;
                break;
        case TID_RES_GROUP:
                classname = GroupClassName;
                break;
        case TID_RES_CLONE:
                classname = CloneClassName;
                break;
        case TID_RES_MASTER:
                classname = MasterClassName;
                break;
        default:
             return NULL;
        }
        
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ) {
                return NULL;
        }

        CMAddKey(op, "Id", rscid, CMPI_chars);
        CMAddKey(op, "SystemName", SystemName, CMPI_chars);
        CMAddKey(op, "SystemCreationClassName", SystemClassName, CMPI_chars);
        CMAddKey(op, "CreationClassName", classname, CMPI_chars);

	DEBUG_LEAVE();
        return op;
}

int
get_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * ref,
                  char ** properties, uint32_t type, CMPIStatus * rc)
{
        CMPIInstance * 	ci = NULL;
        CMPIData 	data_name;
        char * 		rscid = NULL;
        uint32_t 	this_type;

	if ( cim_get_hb_status() != HB_RUNNING ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_WARNING, "Heartbeat not running.");
		return HA_FAIL;
	}

        data_name = CMGetKey(ref, "Id", rc);
        if ( data_name.value.string == NULL ) {
                cl_log(LOG_WARNING, "key %s is NULL", "Id");
                return HA_FAIL;
        }

        rscid = CMGetCharPtr(data_name.value.string);
        this_type = (type == 0) 
		? cim_get_resource_type(rscid)	: type;

        ci = make_instance_byid(broker, ref, rscid, this_type, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_WARNING, "Can not create resource instance.");
		return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
	return HA_OK;
}

/* should return primitives in group for primitive provider? no */
int 
enumerate_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                   CMPIResult * rslt, CMPIObjectPath * ref, int need_inst,
                   uint32_t type, CMPIStatus * rc)
{
        int i;
	CIMArray * names;

	DEBUG_ENTER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_WARNING, "Heartbeat not running.");
		return HA_FAIL;
	}

        if ( ( names = cim_get_array(GET_RSC_LIST, NULL, NULL)) == NULL ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_WARNING, "Get resource list failed.");
                return HA_FAIL;
        }

        for ( i = 0; i < cim_array_len(names); i++) {
		char * rsc = cim_array_index_v(names, i).v.str;
                if ( type != cim_get_resource_type(rsc)){
                        continue;
                }
		/* should we return all sub resource of group/clone/master
		   for the primitive resources enumeration operation? */
                if ( need_inst ) {
                        CMPIInstance * ci = NULL;
                        ci = make_instance_byid(broker, ref, rsc, type, rc);
			if( ci == NULL) {
				cim_array_free(names);
                                return HA_FAIL;
                        }
                        CMReturnInstance(rslt, ci);
                } else {
                        CMPIObjectPath * op;
                        op = make_objectpath_byid(broker, ref, rsc, type, rc);
			if (op == NULL ) {
				cim_array_free(names);
				return HA_FAIL;
			}
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }
        }
        CMReturnDone(rslt);
	cim_array_free(names);
	DEBUG_LEAVE();
        return HA_OK;
}

int
resource_cleanup(CMPIBroker * broker, char * classname, CMPIInstanceMI * mi, 
                 CMPIContext * ctx, uint32_t type, CMPIStatus * rc)
{
        return HA_OK;
}

int
delete_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * ref, CMPIStatus * rc)
{
        char * rscid;
        CMPIString * string;
	int ret = HA_FAIL;
        if ((string = CMGetKey(ref, "Id", rc).value.string) == NULL ) {
		cl_log(LOG_WARNING, "Resource id missing.");
                return HA_FAIL;
        }
        rscid = CMGetCharPtr(string);
	ret = cim_update(DEL_RESOURCE, rscid, NULL, NULL);
	rc->rc = (ret==HA_OK)? CMPI_RC_OK : CMPI_RC_ERR_FAILED;
	return ret;
}

#define GET_PROPERTY(INST,T,P,K,rc)					\
{									\
        CMPIData data;							\
        data = CMGetProperty(INST, P, rc);				\
        if ( rc->rc == CMPI_RC_OK && data.value.string != NULL ) {	\
                char * v = CMGetCharPtr(data.value.string);		\
                cim_table_strdup_replace(T, K, v);     			\
        }								\
}

static int
primitive_get_properties(CIMTable * t, CMPIInstance * ci, CMPIStatus * rc)
{
	char * groupid;
	DEBUG_ENTER();
	GET_PROPERTY(ci, t, "Id", "id", rc);
	GET_PROPERTY(ci, t, "ResourceClass", "class", rc);
	GET_PROPERTY(ci, t, "Provider", "provider", rc);
	GET_PROPERTY(ci, t, "Type", "type", rc);
	GET_PROPERTY(ci, t, "Groupid", "groupid", rc);

	/* null means this primitive resource does not belong to any group */
	groupid = cim_table_lookup_v(t, "groupid").v.str;
	if ( groupid && strcmp(groupid, "null") == 0 ) {
		cim_table_strdup_replace(t, "groupid", "");
	}
	DEBUG_LEAVE();
	return HA_OK;
}

static int
clone_get_properties(CIMTable * t, CMPIInstance * ci, CMPIStatus * rc)
{
	DEBUG_ENTER();
        GET_PROPERTY(ci, t, "Notify", "notify", rc);
        GET_PROPERTY(ci, t, "Ordered", "ordered", rc);
        GET_PROPERTY(ci, t, "Interleave", "interleave", rc);
        GET_PROPERTY(ci, t, "CloneMax", "clone_max", rc);
        GET_PROPERTY(ci, t, "CloneNodeMax", "clone_node_max", rc);
	DEBUG_LEAVE();
	return HA_OK;
}

static int
master_get_properties(CIMTable * t, CMPIInstance * ci, CMPIStatus * rc)
{
	DEBUG_ENTER();
        GET_PROPERTY(ci, t, "CloneMax", "clone_max", rc);
        GET_PROPERTY(ci, t, "CloneNodeMax", "clone_node_max", rc);
        GET_PROPERTY(ci, t, "MaxMasters", "max_masters", rc);
        GET_PROPERTY(ci, t, "MaxNodeMasters", "max_node_masters", rc);
	DEBUG_LEAVE();
	return HA_OK;
}

static int
group_get_properties(CIMTable * t, CMPIInstance * ci, CMPIStatus * rc)
{
	/* nothing */
	return HA_OK;
}

int 
update_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci,
                char ** properties, uint32_t type, CMPIStatus * rc)
{
	CIMTable * table = NULL;
	CIMArray * attrs = NULL;
	int ret = HA_FAIL;
	char * rscid;

	DEBUG_ENTER();
	/* get resource id */
        if ((rscid = CMGetKeyString(cop, "Id", rc)) == NULL ) {
		return HA_FAIL;
	}
	/* get original values, and set new values */
	switch(type){
	case TID_RES_PRIMITIVE:
		table = cim_get(GET_PRIMITIVE, rscid, NULL);
		ret = primitive_get_properties(table, ci, rc);		
		break;
	case TID_RES_CLONE:
		table = cim_get(GET_CLONE, rscid, NULL);
		ret = clone_get_properties(table, ci, rc);
		ret = cim_update(UPDATE_CLONE, NULL, table, NULL); 
		break;
	case TID_RES_MASTER:
		table = cim_get(GET_MASTER, rscid, NULL);
		ret = master_get_properties(table, ci, rc);
		ret = cim_update(UPDATE_MASTER, NULL, table, NULL);
		break;
	case TID_RES_GROUP:
		table = cim_table_new();
		ret = group_get_properties(table, ci, rc);
		break;
	}

	cim_table_free(table);
        /* update attributes */
	attrs = instance_get_attributes(cop, ci, rc);		
	if ( attrs ) {
		cim_update(UPDATE_ATTRIBUTES, rscid, attrs, NULL);
		cim_array_free(attrs);
	}

	rc->rc = (ret==HA_OK) ? CMPI_RC_OK: CMPI_RC_ERR_FAILED;
	DEBUG_LEAVE();
	return ret;
}

int
create_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci,
                uint32_t type, CMPIStatus * rc)
{
	CIMTable * table = NULL;
	CIMArray * attrs = NULL;
	int ret = HA_FAIL;
	char * rscid;

	DEBUG_ENTER();
	if((table = cim_table_new()) == NULL ) {
		return HA_FAIL;
	}

	/* get resource id */
        if ((rscid = CMGetKeyString(cop, "Id", rc)) == NULL ) {
		return HA_FAIL;
	}

	/* get user's attribute */
	attrs = instance_get_attributes(cop, ci, rc);
	if ( attrs == NULL) {
		return HA_FAIL;
	}
	cim_table_replace(table, cim_strdup("array"), makeArrayData(attrs));

	switch(type){
	case TID_RES_PRIMITIVE:
		ret = primitive_get_properties(table, ci, rc);		
		break;
	case TID_RES_CLONE:
		cim_table_strdup_replace(table, "advance", "clone");
		ret = clone_get_properties(table, ci, rc);
		break;
	case TID_RES_MASTER:
		ret = master_get_properties(table, ci, rc);
		cim_table_strdup_replace(table, "advance", "master");
		break;
	case TID_RES_GROUP:
		ret = group_get_properties(table, ci, rc);
		break;
	}

	ret = cim_update(CREATE_RESOURCE, rscid, table, NULL);
	rc->rc = (ret==HA_OK) ? CMPI_RC_OK: CMPI_RC_ERR_FAILED;
	cim_table_free(table);
	DEBUG_LEAVE();
	return ret;

}

