#ifndef _CONSTRAINT_COMMON_H
#define _CONSTRAINT_COMMON_H


CMPIInstance *
make_cons_inst_by_id(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                     char * id, uint32_t type, CMPIStatus * rc);

CMPIInstance *
make_cons_inst(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
               struct ci_table * constraint, int type, CMPIStatus * rc);

int
get_inst_cons(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
              CMPIResult * rslt, CMPIObjectPath * cop,  char ** properties, 
              uint32_t type, CMPIStatus * rc);

int 
enum_inst_cons(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, 
               int need_inst, uint32_t type, CMPIStatus * rc);

#endif
