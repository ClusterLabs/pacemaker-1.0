#ifndef _CONSTRAINT_COMMON_H
#define _CONSTRAINT_COMMON_H

int	get_constraint(CMPIBroker * broker, char * classname, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
		char ** properties, uint32_t type, CMPIStatus * rc);

int 	enumerate_constraint(CMPIBroker * broker, char * classname, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
               int need_inst, uint32_t type, CMPIStatus * rc);

int	delete_constraint(CMPIBroker * broker, char * classname, 
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, uint32_t type, CMPIStatus * rc);

int	update_constraint(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, char ** properties,
		uint32_t type, CMPIStatus * rc);

int	create_constraint(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, uint32_t type, 
		CMPIStatus * rc);

#endif
