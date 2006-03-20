#ifndef _RESROUCE_COMMON_H
#define _RESOURCE_COMMON_H


/* get a resource instance */
int get_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref,
		char ** properties, uint32_t type, CMPIStatus * rc);

/* enumerate instances or instance names */
int enumerate_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, int need_inst, 
		uint32_t type, CMPIStatus * rc);

/* cleanup provider */
int resource_cleanup(CMPIBroker * broker, char * classname, CMPIInstanceMI * mi, 
		CMPIContext * ctx, uint32_t type, CMPIStatus * rc);

/* delete resource */
int delete_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * ref, CMPIStatus * rc);

/* update resource */
int update_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci, 
		char ** properties, uint32_t type, CMPIStatus * rc);

/* create a resource */
int create_resource(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci, 
		uint32_t type, CMPIStatus * rc);

#endif
