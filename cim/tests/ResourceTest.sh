#!/bin/sh

USER=$1
PASSWD=$2
CIB=/var/lib/heartbeat/crm/cib.xml

##############################################################
# primitive resource
##############################################################

#create a primitive resource
wbemcli ci http://$USER:$PASSWD@localhost/root/cimv2:HA_PrimitiveResource.Id="test_resource_id",CreationClassName="HA_PrimitiveResource",SystemName="LinuxHACluster",SystemCreationClassName="HA_LinuxHA" ResourceClass="ocf",Type="IPaddr",Provider="heartbeat",Id="test_resource_id",CreationClassName="HA_PrimitiveResource",SystemName="LinuxHACluster",SystemCreationClassName="HA_LinuxHA"

#add to CIB
wbemcli cm http://$USER:$PASSWD@localhost/root/cimv2:HA_ClusteringService.Id=default_service_id AddResource.Resource=http://localhost/root/cimv2:HA_PrimitiveResource.Id="test_resource_id"

# check CIB
grep "test_resource_id" $CIB  > /dev/null

if [ $? != 0 ]; then 
	echo "*** add primitive resource test_resource_id failed."
fi

#delete  
wbemcli di http://$USER:$PASSWD@localhost/root/cimv2:HA_PrimitiveResource.Id="test_resource_id",CreationClassName="HA_PrimitiveResource",SystemName="LinuxHACluster",SystemCreationClassName="HA_LinuxHA"


#############################################################
# resource group
#############################################################

