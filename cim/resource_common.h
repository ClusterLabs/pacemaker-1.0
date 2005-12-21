#ifndef _RESROUCE_COMMON_H
#define _RESOURCE_COMMON_H

int
get_inst_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * ref,
                  char ** properties, uint32_t type, CMPIStatus * rc);
int 
enum_inst_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                   CMPIResult * rslt, CMPIObjectPath * ref, int need_inst, 
                   uint32_t type, CMPIStatus * rc);

int
resource_cleanup(CMPIBroker * broker, char * classname, CMPIInstanceMI * mi, 
                 CMPIContext * ctx, uint32_t type, CMPIStatus * rc);

#endif
