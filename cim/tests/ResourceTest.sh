#!/bin/sh


if test $# != 2; then
	echo "usage: $0 user password."
	exit 1
fi

USER=$1
PASSWD=$2
CIB=/var/lib/heartbeat/crm/cib.xml

function wait_cib_updated()
{
	sleep 2
}

function resource_query ()
{
	crm_resource -Q -r $1
}

#create a primitive resource
function create_primitive_resource() 
{
TYPE=$1
RSC_ID=$2

echo "creating resource, type: $TYPE, id: $RSC_ID."
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_PrimitiveResource.Id="$RSC_ID",\
CreationClassName="HA_PrimitiveResource",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" \
ResourceClass="ocf",\
Type="$TYPE",\
Provider="heartbeat",\
Id="$RSC_ID",\
CreationClassName="HA_PrimitiveResource",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" > /dev/null
} 

#create its attributes
function create_attribute()
{
RSC_ID=$1
ATTR_ID=$2
NAME=$3
VALUE=$4

echo "creating attribute for resource: $RSC_ID, id: $ATTR_ID, $NAME=$VALUE."
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_InstanceAttributes.Id="$ATTR_ID",\
ResourceId="$RSC_ID" \
Id="$ATTR_ID",\
ResourceId="$RSC_ID",\
Name="$NAME",\
Value="$VALUE" > /dev/null
}

#create its operations
function create_operation()
{
RSC_ID=$1
OP_ID=$2
NAME=$3
INTERVAL=$4
TIMEOUT=$5
echo "creating opertion for resource: $RSC_ID, id: $OP_ID, $NAME,$INTERVAL,$TIMEOUT."
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_Operation.Id="$OP_ID",\
ResourceId="$RSC_ID" \
Id="$OP_ID",\
ResourceId="$RSC_ID",\
Name="$NAME",\
Interval="$INTERVAL",\
TimeOut="$TIMEOUT" > /dev/null
}


#add to CIB
function cib_add_resource()
{
RSC_ID=$1
echo "adding resource: $RSC_ID to CIB."
wbemcli cm http://$USER:$PASSWD@localhost/root/cimv2:HA_ClusteringService.Id=default_service_id \
AddResource.Resource=http://localhost/root/cimv2:HA_PrimitiveResource.Id="$RSC_ID" > /dev/null
}

#delete  
function delete_resource ()
{
CLASSNAME=$1
RSC_ID=$2

echo "deleting resource: $RSC_ID."
wbemcli di http://$USER:$PASSWD@localhost/root/cimv2:$CLASSNAME.Id="$RSC_ID",\
CreationClassName="$CLASSNAME",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA"
}


# create a resource group
function create_resource_group() 
{
GROUP_ID=$1

echo "creating resource group, id: $GROUP_ID."
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_ResourceGroup.Id="$GROUP_ID",\
CreationClassName="HA_ResourceGroup",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" \
Id="$GROUP_ID",\
CreationClassName="HA_ResourceGroup",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" > /dev/null
} 

#add resource to group
function group_add_resource()
{
GROUP_ID=$1
RSC_ID=$2
echo "adding resource: $RSC_ID to resource group $GROUP_ID."
wbemcli cm http://$USER:$PASSWD@localhost/root/cimv2:HA_ResourceGroup.Id=$GROUP_ID,\
CreationClassName="HA_ResourceGroup",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" \
AddPrimitiveResource.Resource=http://localhost/root/cimv2:HA_PrimitiveResource.Id="$RSC_ID" > /dev/null
}


function create_location_constraint() 
{
CONS_ID=$1
RSC_ID=$2
NODE=$3
SCORE=$4

echo "creating location_constraint $CONS_ID for id: $RSC_ID, $NODE,$SCORE"
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_LocationConstraint.Id="$CONS_ID",\
CreationClassName="HA_LocationConstraint",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA" \
Id="$CONS_ID",\
CreationClassName="HA_LocationConstraint",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA",\
Resource="$RSC_ID",\
Score=$SCORE
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_LocationConstraintRule.Id="${CONS_ID}_rule",\
ConstraintId="$CONS_ID" \
Id="${CONS_ID}_rule",\
ConstraintId="$CONS_ID",\
Attribute="node",\
Operation="eq",\
Value="$NODE"
} 

function delete_constraint()
{
CLASSNAME=$1
CONS_ID=$2

echo "deleting constraint: $CONS_ID."
wbemcli di http://$USER:$PASSWD@localhost/root/cimv2:$CLASSNAME.Id="$CONS_ID",\
CreationClassName="$CLASSNAME",\
SystemName="LinuxHACluster",\
SystemCreationClassName="HA_LinuxHA"
}


##############################################################
# primitive resource
##############################################################

echo "---------------------------------------------------"
echo "Primitive Resource Creation test"
echo "---------------------------------------------------"
RESOURCE_ID=test_primitive_resource
ATTRIBUTE_ID=${RESOURCE_ID}_ip

delete_resource HA_PrimitiveResource $RESOURCE_ID 2>/dev/null
create_primitive_resource "IPaddr" "$RESOURCE_ID"
create_attribute $RESOURCE_ID $ATTRIBUTE_ID "ip" "127.0.0.111"
create_attribute $RESOURCE_ID ${RESOURCE_ID}_nic "nic" "lo"
cib_add_resource $RESOURCE_ID
create_attribute $RESOURCE_ID ${RESOURCE_ID}_netmask "netmask" "255.255.255.0"
create_operation $RESOURCE_ID ${RESOURCE_ID}_monitor "monitor" "10s" "20s"
wait_cib_updated

rc=0
resource_query $RESOURCE_ID
resource_query $RESOURCE_ID | grep "$RESOURCE_ID">/dev/null \
	|| { echo "[FAILED] $RESOURCE_ID not found." && rc=1; }
resource_query $RESOURCE_ID | grep "$ATTRIBUTE_ID">/dev/null\
	|| { echo "[FAILED] $ATTRIBUTE_ID not found." && rc=1; }
if [ $rc = 0 ]; then 
	echo "[OK] create resource group:$GROUP_ID successfully."
fi


#delete_resource HA_PrimitiveResource $RESOURCE_ID
echo "---------------------------------------------------"
echo "Location Constraint creation test"
echo "---------------------------------------------------"
delete_constraint HA_LocationConstraint ${RESOURCE_ID}_location_cons
create_location_constraint ${RESOURCE_ID}_location_cons ${RESOURCE_ID} "node1" "1000"

#############################################################
# resource group
#############################################################

GROUP_ID=test_resource_group
delete_resource HA_ResourceGroup $GROUP_ID 2>/dev/null

echo "---------------------------------------------------"
echo "Resource Group Creation test"
echo "---------------------------------------------------"
SUB_RESOURCE_ID=sub_resource_1
SUB_ATTRIBUTE_ID=${SUB_RESOURCE_ID}_ip

create_primitive_resource "IPaddr" "$SUB_RESOURCE_ID"
create_attribute $SUB_RESOURCE_ID $SUB_ATTRIBUTE_ID "ip" "127.0.0.111"
create_resource_group $GROUP_ID
group_add_resource $GROUP_ID $SUB_RESOURCE_ID
cib_add_resource $GROUP_ID
wait_cib_updated
resource_query $GROUP_ID

rc=0
resource_query $GROUP_ID | grep "$GROUP_ID" > /dev/null		\
	|| { echo "[FAILED] $GROUP_ID not found in CIB." && rc=1; }
resource_query $GROUP_ID | grep "$SUB_RESOURCE_ID" >/dev/null	\
	|| { echo "[FAILED] $SUB_RESOURCE_ID not found in CIB." && rc=1; }
resource_query $GROUP_ID | grep "$SUB_ATTRIBUTE_ID" >/dev/null	\
	|| { echo "[FAILED] $SUB_ATTRIBUTE_ID not found in CIB." && rc=1; }

if [ $rc = 0 ]; then 
	echo "[OK] create resource group:$GROUP_ID successfully."
fi
echo "---------------------------------------------------"
echo "Resource Group Add Resource test"
echo "---------------------------------------------------"
SUB_RESOURCE_ID2=sub_resource_2
SUB_ATTRIBUTE_ID2=${SUB_RESOURCE_ID2}_ip

create_primitive_resource "IPaddr" $SUB_RESOURCE_ID2
create_attribute $SUB_RESOURCE_ID2 $SUB_ATTRIBUTE_ID2 "ip" "127.0.0.112"
group_add_resource $GROUP_ID $SUB_RESOURCE_ID2
wait_cib_updated
resource_query $GROUP_ID
resource_query $GROUP_ID | grep $SUB_RESOURCE_ID2 >/dev/null \
        || { echo "[FAILED] $SUB_RESOURCE_ID2 not found in CIB." && rc=1; }

#delete_resource HA_ResourceGroup $GROUP_ID



