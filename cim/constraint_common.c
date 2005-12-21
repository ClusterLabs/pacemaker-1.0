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

static char sys_name         [] = "LinuxHACluster";
static char sys_cr_classname [] = "HA_Cluster";


static void 
set_location_rules(CMPIBroker * broker, CMPIInstance * ci, 
                   struct ci_table * constraint, CMPIStatus * rc)
{
        CMPIArray * array = NULL;
        struct ci_table * rule, * exp;
        int i, size;

        rule = CITableGet(constraint, "rule").value.table;
        if ( rule == NULL ) return;
        size = CITableSize(rule);

        if (( array = CMNewArray(broker, size, CMPI_chars, rc)) == NULL ){
                return;
        }

        for ( i = 0; i < size; i++ ) {
                char * id, * attribute, * operation, * value;
                int str_len;
                char * tmp;
                exp = CITableGetAt(rule, i).value.table;
                if ( exp == NULL ) continue;

                id = CITableGet(exp, "id").value.string;
                attribute = CITableGet(exp, "attribute").value.string;
                operation = CITableGet(exp, "operation").value.string;
                value = CITableGet(exp, "value").value.string;
                
                str_len = strlen(id) + strlen(attribute) + 
                        strlen(operation) + strlen(value) +
                        strlen("id") + strlen("attribute") +
                        strlen("operation") + strlen("value") + 8;
                if ( (tmp = (char *)CIM_MALLOC(str_len)) == NULL ) return;

                sprintf(tmp, "id=%s,attribute=%s,operation=%s,value=%s",
                        id, attribute, operation, value);

                CMSetArrayElementAt(array, i, &tmp, CMPI_chars);
        }

        CMSetProperty(ci, "Rule", &array, CMPI_charsA);
}

CMPIInstance *
make_cons_inst(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
               struct ci_table * constraint, int type, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        char * id;
        char type_location [] = "Location";
        char type_colocation [] = "Colocation";
        char type_order [] = "Order";
        char caption[256];
        if ( constraint == NULL ) return NULL;

        ci = CMNewInstance(broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }
        
        id = constraint->get_data(constraint, "id").value.string;
        if ( id == NULL ) return NULL;

        /* setting properties */
        CMSetProperty(ci, "Id", id, CMPI_chars);

        CMSetProperty(ci, "SystemCreationClassName", sys_cr_classname, CMPI_chars);
        CMSetProperty(ci, "SystemName", sys_name, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);

        if ( type == TID_CONS_ORDER ) {
                 struct ci_table * order = constraint;
                 char * to, * from , * action, * order_type;

                 to = order->get_data(order, "to").value.string;
                 from = order->get_data(order, "from").value.string;
                 action = order->get_data(order, "action").value.string;
                 order_type = order->get_data(order, "type").value.string;

                 if(to) CMSetProperty(ci, "To", to, CMPI_chars);
                 if(from) CMSetProperty(ci, "From", from, CMPI_chars);
                 if(action) CMSetProperty(ci, "Action", action, CMPI_chars);
                 if(order_type) CMSetProperty(ci, "OrderType", order_type, CMPI_chars);

                 CMSetProperty(ci, "Type", type_order, CMPI_chars);
        } else if ( type == TID_CONS_LOCATION ) {
                 struct ci_table * location = constraint;
                 char * resource, * score;

                 resource = CITableGet(location, "resource").value.string;
                 score = CITableGet(location, "score").value.string;

                 if(resource) CMSetProperty(ci, "Resource", resource, CMPI_chars);
                 if(score) CMSetProperty(ci, "Score", score, CMPI_chars);
                 CMSetProperty(ci, "Type", type_location, CMPI_chars);

                 /* currently Rules is an array, which should be modeled as a class */
                 set_location_rules(broker, ci, constraint, rc);
        } else if ( type == TID_CONS_COLOCATION ) {
                 struct ci_table * colocation = constraint;
                 char * to, * from , * score;

                 to = colocation->get_data(colocation, "to").value.string;
                 from = colocation->get_data(colocation, "from").value.string;
                 score = colocation->get_data(colocation, "score").value.string;

                 if(to) CMSetProperty(ci, "To", to, CMPI_chars);
                 if(from) CMSetProperty(ci, "From", from, CMPI_chars);
                 if(score) CMSetProperty(ci, "Score", score, CMPI_chars);

                 CMSetProperty(ci, "Type", type_colocation, CMPI_chars);
        }

        sprintf(caption, "Constraint.%s", id);
        /* set Caption */
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
        
out:
        constraint->free(constraint);
        return ci; 
}

CMPIInstance *
make_cons_inst_by_id(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                     char * id, uint32_t type, CMPIStatus * rc) 
{
        struct ci_table * constraint;
        if ( id == NULL ) return NULL;

        switch(type) {
        case TID_CONS_LOCATION:
             constraint = ci_get_location_constraint(id);
             break;
        case TID_CONS_COLOCATION:
             constraint = ci_get_colocation_constraint(id);
             break;
        case TID_CONS_ORDER:
             constraint = ci_get_order_constraint(id);
             break;
        default:
             return NULL;
        }

        if ( constraint == NULL ) return NULL;
        return make_cons_inst(broker, classname, op, constraint, type, rc);
}

int
get_inst_cons(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
              CMPIResult * rslt, CMPIObjectPath * cop,
              char ** properties, uint32_t type, CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIObjectPath* op = NULL;
        CMPIString * key_id;
        char * cons_id = NULL;
        int ret = 0;
                
        /* get the key from the object path */
        if ( (key_id = CMGetKey(cop, "Id", rc).value.string) == NULL ) {
                cl_log(LOG_WARNING, "key Id is NULL.");
                ret = HA_FAIL;
                goto out;
        }

        if ( ( cons_id = CMGetCharPtr(key_id)) == NULL ) {
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
        ci = make_cons_inst_by_id(broker, classname, op, cons_id, type, rc);
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
        return ret;
}

int 
enum_inst_cons(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, 
               int need_inst, uint32_t type, CMPIStatus * rc)
{

        char * namespace = NULL;
        CMPIObjectPath * op = NULL;
        GPtrArray * table;
        int i;

        /* create object path */
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ){
                return HA_FAIL;
        }
        
        /* get constraint name table */
        if ( ( table = ci_get_constraint_name_table (type) ) == NULL ) {
                return HA_FAIL;
        }
        /* for each constraint */
        for ( i = 0; i < table->len; i++) {
                char * consid;
                if ( ( consid = (char *)g_ptr_array_index(table, i)) == NULL ) {
                        continue;
                }

                if ( need_inst ) {
                        /* if need instance, make instance an return it */
                        CMPIInstance * ci = NULL;
                        ci = make_cons_inst_by_id(broker, classname, op, 
                                                  consid, type, rc); 
                        if ( CMIsNullObject(ci) ){
                                cl_log(LOG_WARNING, 
                                   "%s: can not make instance", __FUNCTION__);
                                ci_free_ptr_array(table, ci_safe_free);
                                return HA_FAIL;
                        }
                        
                        cl_log(LOG_INFO, "%s: return instance", __FUNCTION__);
                        CMReturnInstance(rslt, ci);
                } else {
                        /* otherwise, just add keys to objectpath and return it */
                        CMAddKey(op, "Id", consid, CMPI_chars);      
                        CMAddKey(op, "SystemName", sys_name, CMPI_chars);
                        CMAddKey(op, "SystemCreationClassName", sys_cr_classname, CMPI_chars);
                        CMAddKey(op, "CreationClassName", classname, CMPI_chars);

                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }

        }

        ci_free_ptr_array(table, ci_safe_free);
        CMReturnDone(rslt);
        return HA_OK;
}
