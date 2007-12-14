               SNMP Subagent Extention for CRM Resources

1. Introduction

    The purpose of this patch is to extend the SNMP subagent to get and
    receive a trap about the CRM resource information provided by
    Heartbeat Version 2.

    This patch introduces two new SNMP MIB objects.

    1) LHAResourceTable: resources' name, type, on which node they are
       running, and their status. On the other words, you can get the
       information which is provided with crm_mon through the SNMP interface.

    2) LHAResourceStatusUpdate: when a resource's status changes, you are
       notified with this SNMP trap.

2. Added MIB

    The following is the added MIB at this patch.

---------------------------------------------------------------------------
| OID         | Object Name             | Value type    | Description     |
---------------------------------------------------------------------------
|4682.8 |     | LHAResourceTable        | table         |                 |
---------------------------------------------------------------------------
|       |.1   |  LHAResourceEntry       |               |                 |
---------------------------------------------------------------------------
|       |.1.1 |   LHAResourceIndex      | Integer32     |                 |
---------------------------------------------------------------------------
|       |.1.2 |   LHAResourceName       | DisplayString |                 |
---------------------------------------------------------------------------
|       |.1.3 |   LHAResourceType       | INTEGER       | unknown(0)      | 
|       |     |                         |               | primitive(1)    |
|       |     |                         |               | group(2)        |
|       |     |                         |               | clone(3)        |
|       |     |                         |               | masterSlave(4)  |
---------------------------------------------------------------------------
|       |.1.4 |   LHAResourceNode       | DisplayString |                 |
---------------------------------------------------------------------------
|       |.1.5 |   LHAResourceStatus     | INTEGER       | unknown(0)      |
|       |     |                         |               | stopped(1)      |
|       |     |                         |               | started(2)      |
|       |     |                         |               | slave(3)        |
|       |     |                         |               | master(4)       |
---------------------------------------------------------------------------
|       |.1.6 |   LHAResourceIsManaged  | INTEGER       | unmanaged(0)    |
|       |     |                         |               | managed(1)      |
---------------------------------------------------------------------------
|       |.1.7 |   LHAResourceFailcount  | Integer32     |                 |
---------------------------------------------------------------------------
|       |.1.8 |   LHAResourceParent     | DisplayString |                 |
---------------------------------------------------------------------------


    NOTE :   "master" status means "promoted", and "slave" means "demoted".
           All master/slave resources start up as slave at first, and until
           they are demoted or promoted explicitly, heartbeat only knows
           they "started".
           So, LHAResourceStatus's value is according to the crm_mon output.
    NOTE :   For the present, you can get the information only about *running*
           resources or the resources that their values of fail-count are 
           larger than 1. Because it's difficult to decide which node 
           a resource *stopped* on...

3. Added Trap

    The following is the added Trap at this patch.

---------------------------------------------------------------------------
| OID         | Object Name             | Value type    | Description     |
---------------------------------------------------------------------------
|4682.900.8   | LHAResourceStatusUpdate |               |                 |
|             |------------------------------------------------------------
|             |  LHAResourceName        | DisplayString |                 |
|             |------------------------------------------------------------
|             |  LHAResourceNode        | DisplayString |                 |
|             |------------------------------------------------------------
|             |  LHAResourceStatus      | INTEGER       | 0 : unknown     |
|             |                         |               | 1 : stopped     |
|             |                         |               | 2 : started     |
|             |                         |               | 3 : slave       |
|             |                         |               | 4 : master      |
---------------------------------------------------------------------------

    NOTE :   This trap is sent only when the resource operation succeeds.
           Concretely, the extended hbagent gets the cib information when it
           changes, and parse it. And if the rc_code of the operation (like
           CRMD_ACTION_START) is "0", then the hbagent sends a trap.

4. Demo Output

    [root@u5node1 ~]# snmpwalk -v 1 \
                        -c public localhost LINUX-HA-MIB::LHAResourceTable
    LINUX-HA-MIB::LHAResourceName.1 = STRING: group0
    LINUX-HA-MIB::LHAResourceName.2 = STRING: prmIp
    LINUX-HA-MIB::LHAResourceName.3 = STRING: prmApPostgreSQLDB
    LINUX-HA-MIB::LHAResourceName.4 = STRING: clone0
    LINUX-HA-MIB::LHAResourceName.5 = STRING: clone0
    LINUX-HA-MIB::LHAResourceName.6 = STRING: clone0-dummy:0
    LINUX-HA-MIB::LHAResourceName.7 = STRING: clone0-dummy:1
    LINUX-HA-MIB::LHAResourceName.8 = STRING: ms-sf
    LINUX-HA-MIB::LHAResourceName.9 = STRING: ms-sf
    LINUX-HA-MIB::LHAResourceName.10 = STRING: master_slave_Stateful:0
    LINUX-HA-MIB::LHAResourceName.11 = STRING: master_slave_Stateful:1
    LINUX-HA-MIB::LHAResourceType.1 = INTEGER: group(2)
    LINUX-HA-MIB::LHAResourceType.2 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceType.3 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceType.4 = INTEGER: clone(3)
    LINUX-HA-MIB::LHAResourceType.5 = INTEGER: clone(3)
    LINUX-HA-MIB::LHAResourceType.6 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceType.7 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceType.8 = INTEGER: masterSlave(4)
    LINUX-HA-MIB::LHAResourceType.9 = INTEGER: masterSlave(4)
    LINUX-HA-MIB::LHAResourceType.10 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceType.11 = INTEGER: primitive(1)
    LINUX-HA-MIB::LHAResourceNode.1 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.2 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.3 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.4 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.5 = STRING: u5node2
    LINUX-HA-MIB::LHAResourceNode.6 = STRING: u5node2
    LINUX-HA-MIB::LHAResourceNode.7 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.8 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceNode.9 = STRING: u5node2
    LINUX-HA-MIB::LHAResourceNode.10 = STRING: u5node2
    LINUX-HA-MIB::LHAResourceNode.11 = STRING: u5node1
    LINUX-HA-MIB::LHAResourceStatus.1 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.2 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.3 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.4 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.5 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.6 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.7 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.8 = INTEGER: master(4)
    LINUX-HA-MIB::LHAResourceStatus.9 = INTEGER: master(4)
    LINUX-HA-MIB::LHAResourceStatus.10 = INTEGER: started(2)
    LINUX-HA-MIB::LHAResourceStatus.11 = INTEGER: master(4)
    LINUX-HA-MIB::LHAResourceIsManaged.1 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.2 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.3 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.4 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.5 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.6 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.7 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.8 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.9 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.10 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceIsManaged.11 = INTEGER: managed(1)
    LINUX-HA-MIB::LHAResourceFailCount.1 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.2 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.3 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.4 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.5 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.6 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.7 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.8 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.9 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.10 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceFailCount.11 = INTEGER: 0
    LINUX-HA-MIB::LHAResourceParent.1 = STRING:
    LINUX-HA-MIB::LHAResourceParent.2 = STRING: group0
    LINUX-HA-MIB::LHAResourceParent.3 = STRING: group0
    LINUX-HA-MIB::LHAResourceParent.4 = STRING:
    LINUX-HA-MIB::LHAResourceParent.5 = STRING:
    LINUX-HA-MIB::LHAResourceParent.6 = STRING: clone0
    LINUX-HA-MIB::LHAResourceParent.7 = STRING: clone0
    LINUX-HA-MIB::LHAResourceParent.8 = STRING:
    LINUX-HA-MIB::LHAResourceParent.9 = STRING:
    LINUX-HA-MIB::LHAResourceParent.10 = STRING: ms-sf
    LINUX-HA-MIB::LHAResourceParent.11 = STRING: ms-sf

    cf.) Then crm_mon's output is...
    
============
Last updated: Mon Dec  3 17:39:27 2007
Current DC: u5node2 (7e035df7-607d-42b4-a1e7-e8d9db108e6c)
2 Nodes configured.
3 Resources configured.
============

Node: u5node1 (77b7c7b1-68ba-4542-9f10-b75de73e9ffd): online
Node: u5node2 (7e035df7-607d-42b4-a1e7-e8d9db108e6c): online

Resource Group: group0
    prmIp       (heartbeat::ocf:IPaddr):        Started u5node1
    prmApPostgreSQLDB   (heartbeat::ocf:pgsql): Started u5node1
Clone Set: clone0
    clone0-dummy:0      (heartbeat::ocf:Dummy): Started u5node2
    clone0-dummy:1      (heartbeat::ocf:Dummy): Started u5node1
Master/Slave Set: ms-sf
    master_slave_Stateful:0     (heartbeat::ocf:Stateful):      Started u5node2
    master_slave_Stateful:1     (heartbeat::ocf:Stateful):      Master u5node1


    Sample SNMP traps

    Dec  3 17:38:31 manager_node snmptrapd[1343]: 2007-12-03 17:38:31 u5node2 [
    192.168.70.129]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (22408) 0
    :03:44.08   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: master_slave_Stateful:0 LI
    NUX-HA-MIB::LHAResourceNode = STRING: u5node2 LINUX-HA-MIB::LHAResourceStat
    us = INTEGER: started(2)
    Dec  3 17:38:31 manager_node snmptrapd[1343]: 2007-12-03 17:38:31 u5node2 [
    192.168.70.129]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (22420) 0
    :03:44.20   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: clone0-dummy:0  LINUX-HA-M
    IB::LHAResourceNode = STRING: u5node2 LINUX-HA-MIB::LHAResourceStatus = INT
    EGER: started(2)
    Dec  3 17:38:32 manager_node snmptrapd[1343]: 2007-12-03 17:38:32 u5node1 [
    192.168.70.128]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (19126) 0
    :03:11.26   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: clone0-dummy:1  LINUX-HA-M
    IB::LHAResourceNode = STRING: u5node1 LINUX-HA-MIB::LHAResourceStatus = INT
    EGER: started(2)
    Dec  3 17:38:32 manager_node snmptrapd[1343]: 2007-12-03 17:38:32 u5node1 [
    192.168.70.128]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (19129) 0
    :03:11.29   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: master_slave_Stateful:1 LI
    NUX-HA-MIB::LHAResourceNode = STRING: u5node1 LINUX-HA-MIB::LHAResourceStat
    us = INTEGER: started(2)
    Dec  3 17:38:34 manager_node snmptrapd[1343]: 2007-12-03 17:38:34 u5node1 [
    192.168.70.128]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (19314) 0
    :03:13.14   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: master_slave_Stateful:1 LI
    NUX-HA-MIB::LHAResourceNode = STRING: u5node1 LINUX-HA-MIB::LHAResourceStat
    us = INTEGER: master(4)
    Dec  3 17:38:36 manager_node snmptrapd[1343]: 2007-12-03 17:38:36 u5node1 [
    192.168.70.128]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (19516) 0
    :03:15.16   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: prmIp   LINUX-HA-MIB::LHAR
    esourceNode = STRING: u5node1 LINUX-HA-MIB::LHAResourceStatus = INTEGER: st
    arted(2)
    Dec  3 17:38:42 manager_node snmptrapd[1343]: 2007-12-03 17:38:42 u5node1 [
    192.168.70.128]: DISMAN-EVENT-MIB::sysUpTimeInstance = Timeticks: (20067) 0
    :03:20.67   SNMPv2-MIB::snmpTrapOID.0 = OID: LINUX-HA-MIB::LHAResourceStatu
    sUpdate  LINUX-HA-MIB::LHAResourceName = STRING: prmApPostgreSQLDB       LI
    NUX-HA-MIB::LHAResourceNode = STRING: u5node1 LINUX-HA-MIB::LHAResourceStat
    us = INTEGER: started(2)

5. Other changes

    This patch modifies the following too.
        1) Make SNMP_CACHE_TIME_OUT variable.
             apply the value which is specified with -r option.
        2) Fix some memory leaks.
             debug with valgrind.
        3) Update SNMPAgentSanityCheck to keep up with the new functionality.
