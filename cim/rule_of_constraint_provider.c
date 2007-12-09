/*
 * rule_of_constraint_provider.c: HA_RuleOfLocationConstraint provider
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

#include <hb_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <hb_api.h> 
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID       = "cim-ruleof";
static CMPIBroker * Broker            = NULL;
static char 	    ClassName      [] = "HA_RuleOfLocationConstraint"; 
static char 	    Left           [] = "Constraint"; 
static char 	    Right          [] = "Rule";
static char 	    LeftClassName  [] = "HA_LocationConstraint";
static char 	    RightClassName [] = "HA_LocationConstraintRule"; 

DeclareInstanceFunctions   (RuleOfLocationConstraint);
DeclareAssociationFunctions(RuleOfLocationConstraint);

static int 
constraint_has_rule(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * consop, CMPIObjectPath * ruleop,CMPIStatus * rc)
{
	char *id1, *id2;
	id1 = CMGetKeyString(consop, "Id", rc);
	id2 = CMGetKeyString(ruleop, "ConstraintId", rc);

	if ( id1 == NULL || id2 == NULL ) {
		return FALSE;
	}

	if ( strncmp(id1, id2, MAXLEN) == 0 ) {
		return TRUE;
	} 
	return FALSE;
}


/**********************************************
 * Instance 
 **********************************************/

static CMPIStatus 
RuleOfLocationConstraintCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
RuleOfLocationConstraintEnumInstanceNames(CMPIInstanceMI * mi, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, constraint_has_rule, NULL, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
RuleOfLocationConstraintEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, constraint_has_rule, NULL, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
RuleOfLocationConstraintGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_get_inst(Broker, ClassName, ctx, rslt, cop, Left, Right, &rc)
		!= HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
        
}

static CMPIStatus 
RuleOfLocationConstraintCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		 CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
RuleOfLocationConstraintSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci, 
		char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}

static CMPIStatus 
RuleOfLocationConstraintDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        return rc;
}

static CMPIStatus 
RuleOfLocationConstraintExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, 
		char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/****************************************************
 * Association
 ****************************************************/
static CMPIStatus 
RuleOfLocationConstraintAssociationCleanup(CMPIAssociationMI * mi, 
			CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
RuleOfLocationConstraintAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, 
		const char * assoc_class, const char * result_class,
		const char * role, const char * result_role, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName, 
			assoc_class, result_class, role, result_role, 
			constraint_has_rule, NULL, TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
RuleOfLocationConstraintAssociatorNames(CMPIAssociationMI * mi, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
		const char * assoc_class, const char * result_class,
		const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			assoc_class, result_class, role, result_role, 
			constraint_has_rule, NULL, FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
RuleOfLocationConstraintReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, const char * result_class,
		const char * role, char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, NULL, NULL, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        
	}
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
RuleOfLocationConstraintReferenceNames(CMPIAssociationMI * mi, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
		const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "RuleofConstraint: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, constraint_has_rule, NULL, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *      MI stub
 *************************************************************/
DeclareInstanceMI   (RuleOfLocationConstraint, HA_RuleOfLocationConstraintProvider, Broker);
DeclareAssociationMI(RuleOfLocationConstraint, HA_RuleOfLocationConstraintProvider, Broker);

