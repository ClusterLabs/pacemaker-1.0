#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cluster_info.h"
#include "constraint_common.h"
#include "cmpi_utils.h"

static char SystemName		[] = "LinuxHACluster";
static char SystemCrName	[] = "HA_Cluster";

static const mapping_t	OrderMap	[] = {MAPPING_HA_OrderConstraint};
static const mapping_t	LocationMap	[] = {MAPPING_HA_LocationConstraint};
static const mapping_t	ColocationMap	[] = {MAPPING_HA_ColocationConstraint};

static CMPIInstance *	make_instance_byid(CMPIBroker * broker, char * classname,
				CMPIObjectPath * op, char * id, uint32_t type, 
				CMPIStatus * rc);

static CMPIInstance *	make_instance(CMPIBroker * broker, char * classname, 
				CMPIObjectPath * op, CIMTable * cons, int type, 
				CMPIStatus * rc);
static void 		location_set_rules(CMPIBroker * broker, 
				CMPIInstance * ci, CIMTable * constraint, 
				CMPIStatus * rc);
static void		location_get_rules(CMPIBroker * broker, 
				CMPIInstance * ci, CIMTable * constraint,
				CMPIStatus * rc);
static void 
location_set_rules(CMPIBroker * broker, CMPIInstance * ci, 
                   CIMTable * constraint, CMPIStatus * rc)
{
        CMPIArray * array = NULL;
        CIMArray * rule;
	int i, size;

	DEBUG_ENTER();	
        if((rule = cim_table_lookup_v(constraint, "array").v.array) == NULL ) {
		return ;
	}
        size = cim_array_len(rule);

        if (( array = CMNewArray(broker, size, CMPI_chars, rc)) == NULL ){
                return;
        }

        for ( i = 0; i < size; i++ ) {
                char * id, * attribute, * operation, * value;
                int stringlen;
                char * tmp;
		CIMTable * exp = NULL;

                if ( (exp = cim_array_index_v(rule, i).v.table) == NULL ) {
			continue; 
		}
                id = cim_table_lookup_v(exp, "id").v.str;
                attribute = cim_table_lookup_v(exp, "attribute").v.str;
                operation = cim_table_lookup_v(exp, "operation").v.str;
                value = cim_table_lookup_v(exp, "value").v.str;
               	cl_log(LOG_INFO, "REMOVEME: id: %s, attr: %s, op: %s, v: %s",
				 id, attribute, operation, value);

                stringlen = strlen(id) + strlen(attribute) + 
                        strlen(operation) + strlen(value) +
                        strlen("id") + strlen("attribute") +
                        strlen("operation") + strlen("value") + 8;
                if ( (tmp = (char *)cim_malloc(stringlen)) == NULL ) { 
			return; 
		}

                snprintf(tmp,stringlen, "%s<%s>%s", attribute,operation,value);
                CMSetArrayElementAt(array, i, &tmp, CMPI_chars);
        }

        CMSetProperty(ci, "Rule", &array, CMPI_charsA);
	DEBUG_LEAVE();
}

static CMPIInstance *
make_instance(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
               CIMTable * constraint, int type, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        char * id;
        char caption[MAXLEN];

	DEBUG_ENTER();
        ci = CMNewInstance(broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }
        
        id = cim_table_lookup_v(constraint, "id").v.str;
        if ( id == NULL ) { return NULL; }

	dump_cim_table(constraint, "constraint");
        /* setting properties */
        CMSetProperty(ci, "Id", id, CMPI_chars);

        CMSetProperty(ci, "SystemCreationClassName", SystemCrName, CMPI_chars);
        CMSetProperty(ci, "SystemName", SystemName, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);

        if ( type == TID_CONS_ORDER ) {
		cmpi_set_properties(broker, ci, constraint, OrderMap, 
				MAPDIM(OrderMap), rc); 
        } else if ( type == TID_CONS_LOCATION ) {
		cmpi_set_properties(broker, ci, constraint, LocationMap,
				MAPDIM(LocationMap), rc); 
                location_set_rules(broker, ci, constraint, rc);
        } else if ( type == TID_CONS_COLOCATION ) {
		cmpi_set_properties(broker, ci, constraint, ColocationMap, 
				MAPDIM(ColocationMap), rc); 
        }

        snprintf(caption, MAXLEN, "Constraint.%s", id);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
out:
	DEBUG_LEAVE();
        return ci; 
}

static CMPIInstance *
make_instance_byid(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                     char * id, uint32_t type, CMPIStatus * rc) 
{
	CMPIInstance * ci;
	CIMTable * constraint;
	int funcid = 0;

	DEBUG_ENTER();
        switch(type) {
        case TID_CONS_LOCATION: funcid=GET_LOCATION_CONSTRAINT; break;
        case TID_CONS_COLOCATION: funcid=GET_COLOCATION_CONSTRAINT; break;
        case TID_CONS_ORDER: funcid=GET_ORDER_CONSTRAINT; break;
        default:break;
        }

        if ( (constraint= cim_get(funcid, id, NULL)) == NULL ) { 
		return NULL; 
	}
	ci = make_instance(broker, classname, op, constraint, type, rc);
	cim_table_free(constraint);
	DEBUG_LEAVE();
	return ci;
}

int
get_constraint(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
              CMPIResult * rslt, CMPIObjectPath * cop,
              char ** properties, uint32_t type, CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIObjectPath * op = NULL;
        char * consid = NULL;
        int ret = 0;
                
	DEBUG_ENTER();
        /* get the key from the object path */
	if (( consid = CMGetKeyString(cop, "Id", rc)) == NULL ) {
		ret = HA_FAIL;
		goto out;
	}

        /* create a object path */
        op = CMNewObjectPath(broker, CMGetCharPtr(CMGetNameSpace(cop, rc)), 
                             classname, rc);
        if ( CMIsNullObject(op) ){
                ret = HA_FAIL;
                cl_log(LOG_WARNING, "Could not create object path.");
                goto out;
        }

        /* make an instance */
        ci = make_instance_byid(broker, classname, op, consid, type, rc);
        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                cl_log(LOG_WARNING, "Could not create instance.");
                goto out;
        }

        /* add the instance to result */
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        ret = HA_OK;
out:
	DEBUG_LEAVE();
        return ret;
}

int 
enumerate_constraint(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, 
               int need_inst, uint32_t type, CMPIStatus * rc)
{
        char * namespace = NULL;
        CMPIObjectPath * op = NULL;
        int i;
	CIMArray * consarray;
	int	funcid = 0;

        /* create object path */
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ){
                return HA_FAIL;
        }
       
	switch(type){
		case TID_CONS_ORDER: 
			funcid = GET_ORDER_CONS_LIST;break;
		case TID_CONS_LOCATION: 
			funcid = GET_LOCATION_CONS_LIST; break;
		case TID_CONS_COLOCATION: 
			funcid = GET_COLOCATION_CONS_LIST; break;
		default: break;
	} 

        if ( ( consarray = cim_get_array(funcid, NULL, NULL) ) == NULL ) {
                return HA_FAIL;
        }

        /* for each constraint */
        for ( i = 0; i < cim_array_len(consarray); i++) {
                char * consid = cim_array_index_v(consarray,i).v.str;
                if ( need_inst ) {
                        /* if need instance, make instance an return it */
                        CMPIInstance * ci = NULL;
                        ci = make_instance_byid(broker, classname, op, 
                        		consid, type, rc); 
                        if ( CMIsNullObject(ci) ){
                                cl_log(LOG_WARNING, 
                                   "%s: can not make instance", __FUNCTION__);
                                return HA_FAIL;
                        }
                        
                        cl_log(LOG_INFO, "%s: return instance", __FUNCTION__);
                        CMReturnInstance(rslt, ci);
                } else {
                        /* otherwise, just add keys to objectpath and return it */
                        CMAddKey(op, "Id", consid, CMPI_chars);      
                        CMAddKey(op, "SystemName", SystemName, CMPI_chars);
                        CMAddKey(op, "SystemCreationClassName", SystemCrName, CMPI_chars);
                        CMAddKey(op, "CreationClassName", classname, CMPI_chars);

                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }
        }
        CMReturnDone(rslt);
	rc->rc = CMPI_RC_OK;
	cim_array_free(consarray);
        return HA_OK;
}

int	
delete_constraint(CMPIBroker * broker, char * classname, 
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, uint32_t type, CMPIStatus * rc)
{
	const char * key [] = {"Id", "CreationClassName", "SystemName",
			"SystemCreationClassName"};
	int funcid = 0;
	char * id;

	DEBUG_ENTER();
	if ((id = CMGetKeyString(cop, key[0], rc)) == NULL ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_ERR, "del_cons: can't get constraint id.");
		return HA_FAIL;
	}
	
	switch(type){
		case TID_CONS_ORDER: 
			funcid = DEL_ORDER_CONSTRAINT;
			break;
		case TID_CONS_LOCATION: 
			funcid = DEL_LOCATION_CONSTRAINT; 
			break;
		case TID_CONS_COLOCATION: 
			funcid = DEL_COLOCATION_CONSTRAINT; 
			break;
		default: 
			cl_log(LOG_WARNING, "del_cons: Unknown type");
			break;
	} 
	if ( cim_update(funcid, id, NULL, NULL) == HA_OK ) {
		rc->rc = CMPI_RC_OK;
	} else {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_ERR, "del_cons: cim_update return error.");
	}
	DEBUG_LEAVE();

	return HA_OK;
}

static void
location_get_rules(CMPIBroker * broker, CMPIInstance * ci, 
		CIMTable * constraint, CMPIStatus * rc)
{
	CMPIArray * array;
	int len, i;
	CIMArray * rules;

	DEBUG_ENTER();
	if ( ( rules = cim_array_new()) == NULL ) {
		cl_log(LOG_ERR, "%s: crate array failed.", __FUNCTION__);
		return;
	}

	array = CMGetProperty(ci, "Rule", rc).value.array;
	if(array == NULL || rc->rc != CMPI_RC_OK ) {
		cl_log(LOG_ERR, "%s: get array failed.", __FUNCTION__);
		return ;
	}
	len = CMGetArrayCount(array, rc);

	for(i=0; i<len; i++) {
		CMPIString * s = NULL;
		char * rule = NULL, tmp[MAXLEN]="exp_id_";
		char ** v = NULL;
		int vlen = 0;
		CIMTable * t;

		if( ( t = cim_table_new()) == NULL ) {
			return;
		}

		s = CMGetArrayElementAt(array, i, rc).value.string;
		if ( s == NULL ) {
			continue;
		}
		rule = CMGetCharPtr(s);		
		cl_log(LOG_INFO, "%s: rule = %s", __FUNCTION__, rule);

		/* parse v*/
		v = split_string(rule, &vlen, "<>");
		cim_table_strdup_replace(t, "attribute", v[0]);
		cim_table_strdup_replace(t, "operation", v[1]);
		cim_table_strdup_replace(t, "value", v[2]);

		strncat(tmp, v[0], MAXLEN);
		cim_table_strdup_replace(t, "id", tmp);

		cim_array_append(rules, makeTableData(t)); 
	}

	cim_table_replace(constraint,
			cim_strdup("array"), makeArrayData(rules));
	DEBUG_LEAVE();
}

int	
update_constraint(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, char ** properties,
		uint32_t type, CMPIStatus * rc)
{
	CIMTable * t = NULL;
	const char * key [] = {"Id", "CreationClassName", "SystemName",
				"SystemCreationClassName"};
	char * id, *crname, *sysname, *syscrname;
	int ret = HA_FAIL;

	DEBUG_ENTER();

	id = CMGetKeyString(cop, key[0], rc);
	crname = CMGetKeyString(cop, key[1], rc);
	sysname = CMGetKeyString(cop, key[2], rc);
	syscrname = CMGetKeyString(cop, key[3], rc);
	
	switch(type){
	case TID_CONS_ORDER:
		t = cim_get(GET_ORDER_CONSTRAINT, id, NULL); 
		cmpi_get_properties(ci, t, OrderMap, MAPDIM(OrderMap), rc);
		ret = cim_update(UPDATE_ORDER_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_LOCATION:
		t = cim_get(GET_LOCATION_CONSTRAINT, id, NULL); 
		cmpi_get_properties(ci,t,LocationMap,MAPDIM(LocationMap),rc);
		location_get_rules(broker, ci, t, rc);
		ret = cim_update(UPDATE_LOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_COLOCATION:
		t = cim_get(GET_COLOCATION_CONSTRAINT, id, NULL); 
		cmpi_get_properties(ci,t,ColocationMap,
					MAPDIM(ColocationMap),rc);
		ret = cim_update(UPDATE_COLOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	default: break;
	}

	cim_table_free(t);

	/* if update OK, return CMPI_RC_OK */
	rc->rc = (ret == HA_OK)? CMPI_RC_OK : CMPI_RC_ERR_FAILED;

	DEBUG_ENTER();
	return HA_OK;
}

int
create_constraint(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, uint32_t type, 
		CMPIStatus * rc)
{
	CIMTable * t = NULL;
	int ret = HA_FAIL;
	DEBUG_ENTER();
	if (( t = cim_table_new()) == NULL ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		DEBUG_LEAVE();
		return HA_FAIL;
	}
	switch(type){
	case TID_CONS_ORDER:
		cmpi_get_properties(ci, t, OrderMap, MAPDIM(OrderMap), rc);
		ret = cim_update(UPDATE_ORDER_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_LOCATION:
		cmpi_get_properties(ci,t,LocationMap,MAPDIM(LocationMap),rc);
		location_get_rules(broker, ci, t, rc);
		ret = cim_update(UPDATE_LOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_COLOCATION:
		cmpi_get_properties(ci,t,ColocationMap,MAPDIM(ColocationMap),rc);
		ret = cim_update(UPDATE_COLOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	default: break;
	}
	rc->rc = (ret==HA_OK)? CMPI_RC_OK : CMPI_RC_ERR_FAILED;
	cim_table_free(t);
	DEBUG_LEAVE();
	return ret;
}

