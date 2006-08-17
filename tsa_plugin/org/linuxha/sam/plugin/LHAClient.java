/*
 * Created on Aug 10, 2006
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package org.linuxha.sam.plugin;

import java.util.ArrayList;

import com.ibm.eez.sdk.EEZDomain;
import com.ibm.eez.sdk.EEZPropertyList;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceKey;
import com.ibm.eez.sdk.EEZResourceList;

public interface LHAClient {
	EEZDomain getDomain();
	EEZResourceList getNodeResourceList();
	EEZResourceList getNativeResourceList();
	EEZResourceList getResourceGroupList();
	EEZResourceList getSubResourceList(EEZResourceKey key);
	ArrayList getLocationConstraintList();
	ArrayList getColocationConstraintList();
	ArrayList getOrderConstraintList();
	void setResourceActiveState(String resourceName, boolean online);
	void setNodeActiveState(String resourceName, boolean online);
	void setNodeStandbyState(String resourceName, boolean standby);
	
}


abstract class LHALIBClient implements LHAClient {
	abstract String[] processCommand(String[] cmd);
	private LHANodeResource makeNodeResource(String nodeName)
	{
		LHANodeResource nodeRsc = new LHANodeResource();
		/* domain name will be set in the Cluster class later */
		nodeRsc.setNodeName(nodeName);
		nodeRsc.setResourceName(nodeName);
		nodeRsc.setResourceClass("");
		nodeRsc.setDisplayString(nodeName);
		
		String[] nodeconfig = processCommand(
				new String[]{LHAMgmtLib.MSG_NODE_CONFIG, nodeName});
		nodeRsc.setConfigStatus(nodeconfig);

		nodeRsc.setDescription(nodeName);
		return nodeRsc;
	}
	
	
	private LHANativeResource makeNativeResource(String rscname)
	{
		LHANativeResource rsc  = new LHANativeResource();
		rsc.setResourceName(rscname);
		rsc.setNodeName("");
		String[] status = processCommand(
				new String[]{LHAMgmtLib.MSG_RSC_STATUS, rscname});
		rsc.setStatus(status[1]);
		String[] attrs = processCommand(
				new String[]{LHAMgmtLib.MSG_RSC_PARAMS, rscname});
		
		int pos = 1;
		while(attrs.length > pos + 2){
			if (attrs[pos + 1].equals("target_role")){
				rsc.setTargetRole(attrs[pos + 2]);
				break;
			}
			pos += 3;
		}
		
		return rsc;
	}
	
	private LHAResourceGroup makeResourceGroup(String rscname)
	{
		LHAResourceGroup group = new LHAResourceGroup();
		group.setResourceName(rscname);
		group.setResourceClass(EEZResource.GROUP_CLASS_COLLECTION);
		group.setGroupClass(EEZResource.GROUP_CLASS_COLLECTION);
		return group;
	}

	public EEZDomain getDomain() {
		EEZDomain domain = new EEZDomain();
		String[] result = processCommand(new String[]{LHAMgmtLib.MSG_HB_CONFIG});
		domain.setAutomationVersion(result[8]);
		domain.setDomainName(result[22]);
		EEZPropertyList extraProperties =  new EEZPropertyList();;
		domain.setProperties(extraProperties);
		
		return domain;	
	}

	public EEZResourceList getNodeResourceList() {
		EEZResourceList list = new EEZResourceList();
		String[] result = processCommand(new String[]{LHAMgmtLib.MSG_ALLNODES});
		if(!result[0].equals(LHAMgmtLib.MSG_OK)){
			return list;
		}
		for(int i=1; i<result.length; i++){
			String nodeName = result[i];
			list.add(makeNodeResource(nodeName));
		}
		return list;
	}

	public EEZResourceList getNativeResourceList() {
		EEZResourceList list = new EEZResourceList();
		String[] result = processCommand(new String[]{LHAMgmtLib.MSG_ALL_RSC});
		for(int i=1; i<result.length; i++){
			String rscname = result[i];
			String[] type = processCommand(
					new String[]{LHAMgmtLib.MSG_RSC_TYPE, rscname});
			if (type[1].equals("native")){
				list.add(makeNativeResource(rscname));
			}
		}
		return list;
	}

	public EEZResourceList getResourceGroupList() {
		EEZResourceList list = new EEZResourceList();
		String[] result = processCommand(new String[]{LHAMgmtLib.MSG_ALL_RSC});
		for(int i=1; i<result.length; i++){
			String rscname = result[i];
			String[] type = processCommand(
					new String[]{LHAMgmtLib.MSG_RSC_TYPE, rscname});
			if (type[1].equals("group")){
				list.add(makeResourceGroup(rscname));
			}
		}
		return list;
	}

	public EEZResourceList getSubResourceList(EEZResourceKey key) {
		EEZResourceList list = new EEZResourceList();
		String[] result = processCommand(
				new String[]{LHAMgmtLib.MSG_SUB_RSC, key.getResourceName()});
		for(int i=1; i<result.length; i++){
			String rscname = result[i];
			String[] type = processCommand(
					new String[]{LHAMgmtLib.MSG_RSC_TYPE, rscname});
			if (type[1].equals("native")){
				list.add(makeNativeResource(rscname));
			}
		}
		return list;
	}

	public ArrayList getLocationConstraintList() {
		ArrayList list = new ArrayList();
		String[] consid = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINTS, "rsc_location"});
		for(int i = 1; i < consid.length; i++){
			String[] attr = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINT, 
						"rsc_location", consid[i]});
			LHALocationConstraint cons = 
				new LHALocationConstraint(attr[2], attr[3]);
			int pos = 3;
			while ( attr.length > pos + 4){
				cons.setRule(attr[pos + 2], attr[pos + 3], attr[pos + 4]);
				pos += 4;
			}
			list.add(cons);
		}
		return list;
	}

	public ArrayList getColocationConstraintList() {
		ArrayList list = new ArrayList();
		String[] consid = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINTS, "rsc_colocation"});
		for(int i=1; i<consid.length; i++){
			String[] attr = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINT, 
						"rsc_colocation", consid[i]});
			LHAColocationConstraint cons = 
				new LHAColocationConstraint(attr[2], attr[3], attr[4]);
			list.add(cons);
		}
		return list;
	}

	public ArrayList getOrderConstraintList() {
		ArrayList list = new ArrayList();
		String[] consid = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINTS, "rsc_order"});
		for(int i=1; i<consid.length; i++){
			String[] attr = processCommand(
				new String[]{LHAMgmtLib.MSG_GET_CONSTRAINT, 
						"rsc_order", consid[i]});
			LHAOrderConstraint cons = new 
				LHAOrderConstraint(attr[2], attr[4], attr[3]);
			list.add(cons);
		}
		return list;
	}

	// start or stop heartbeat on this node
	public void setNodeActiveState(String node_name, boolean online) 
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setNodeActiveState() begin. online = " + online);
		if( online ) {
			LHAMgmtLib.start_heartbeat(node_name);
		} else {
			LHAMgmtLib.stop_heartbeat(node_name);
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setNodeActiveState() end.");
	}
	
	// start or stop a resource
	public void setResourceActiveState(String rsc_name, boolean online)
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setResourceActiveState: begin. online = " + online);
		String state = online? "started" : "stopped";
		processCommand(new String[]{LHAMgmtLib.MSG_SET_TARGET_ROLE, rsc_name, state});
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setResourceActiveState: end.");
	}
	
	// standby or active a node
	public void setNodeStandbyState(String name, boolean on)
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setNodeStandbyState() begin. on = " + on);
		String state = on? "on" : "off";
		processCommand(new String[]{LHAMgmtLib.MSG_STANDBY, name, state});
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LHAClient:setNodeStandbyState() end.");
	}
	
}
