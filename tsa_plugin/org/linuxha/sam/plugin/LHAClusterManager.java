package org.linuxha.sam.plugin;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.GregorianCalendar;

import com.ibm.eez.sdk.EEZDomain;
import com.ibm.eez.sdk.EEZGroupMembershipRelation;
import com.ibm.eez.sdk.EEZPolicyInfo;
import com.ibm.eez.sdk.EEZRelation;
import com.ibm.eez.sdk.EEZRelationList;
import com.ibm.eez.sdk.EEZRequest;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceKey;
import com.ibm.eez.sdk.EEZResourceList;

public class LHAClusterManager extends Thread
{
	/* locks required to protect these 3 objects */
	protected EEZResourceList resourceList;
	protected EEZRelationList relationList;
	protected EEZDomain haDomain;
	
	private LHAClient haClient;
	private Calendar startupTime = new GregorianCalendar();
	private LinuxHAFLA haFLA;


	public LHAClusterManager(LinuxHAFLA fla)
	{
		haClient = new LHAEventdClient();
		haFLA = fla;
	}
	
	public synchronized LHAResource findResource(EEZResourceKey key)
	{
		return findResource(key.getDomainName(), key.getNodeName(),
					key.getResourceName(), key.getResourceClass());
	}
	
	public synchronized LHAResource findResource(String domainName, String localtionName, 
						String resourceName, String resourceClass)
	{
		if (domainName == null || localtionName == null || 
				resourceName == null || resourceClass == null ) {
			return null;
		}
		int size = resourceList.size();
		for (int i=0; i<size; i++) {
			LHAResource rsc = ((LHAResource)resourceList.get(i));
			if (domainName.equals(rsc.getDomainName()) 
					&& localtionName.equals(rsc.getNodeName())
					&& resourceName.equals(rsc.getResourceName())
					&& resourceClass.equals(rsc.getResourceClass())) {
				return rsc;
			}
		}
		return null;
	}

	public synchronized EEZResourceList getAllResources()
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllResources enter.");
		EEZResourceList list = new EEZResourceList();
		for (int i = 0; i < resourceList.size(); i++) {
			LHAResource rsc = (LHAResource) resourceList.get(i);
			list.add(rsc);
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllResources leave.");
		return list;
	}
	
	public synchronized EEZResourceList getAllTopLevelResources() {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllTopLevelResources enter.");
		EEZResourceList list = new EEZResourceList();
		for (int i = 0; i < resourceList.size(); i++) {
			LHAResource rsc = (LHAResource) resourceList.get(i);
			if ( rsc.isTopLevel() ) {
				list.add(rsc);
			}
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllTopLevelResources leave.");
		return list;
	}

	public synchronized EEZRelationList getAllRelations() {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllRelations enter.");
		EEZRelationList list = new EEZRelationList();
		for (int i = 0; i < relationList.size(); i++) {
			EEZRelation rel = (EEZRelation) relationList.get(i);
			list.add(rel);
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:getAllRelations leave.");
		return list;
	}

	public synchronized void initialize()
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:initialize: get data from LinuxHA...");
		resourceList = new EEZResourceList();
		relationList = new EEZRelationList();
		
		updateDomain();
		updateResourceList();
		updateRelationList();
	
		// start thread
		this.start();
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:initialize: end.");
	}

	private void updateDomain()
	{
		// domain!
		haDomain = haClient.getDomain();
		haDomain.setAutomationProductName("www.linux-ha.org");
		haDomain.setAutomationStartupTime(startupTime); //not true
		haDomain.setDomainState(EEZDomain.STATE_ONLINE);
		haDomain.setAutomationLocation(""); 	//should it be set to the DC node?
		haDomain.setAdapterVersion("0.1");
		haDomain.setCommunicationState(EEZDomain.COMMUNICATION_STATE_OK);
		
		// get hostname
		String[] values = Utilities.execCmd("hostname");
		haDomain.setAdapterLocation(values[0]);

		haDomain.setAdapterStartupTime(startupTime);
		EEZPolicyInfo info = new EEZPolicyInfo();
		info.setPolicyName("LinuxHA Policy");
		info.setPolicyActivationTime(startupTime);
		haDomain.setPolicyInfo(info);

		haDomain.setAdapterCapabilityXML("");
	}
	
	private void updateResourceList()
	{
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:invalidateResources begin." );
		String domainName = haDomain.getDomainName();
		
		//add node resources
		EEZResourceList rsclist = haClient.getNodeResourceList();
		for(int i=0; i<rsclist.size(); i++){
			LHAResource rsc = (LHAResource)rsclist.get(i);
			rsc.setDomainName(domainName);
			rsc.setTopLevel(true);
			rsc.completeResource();
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, 
					"LHAClusterManager:invalidateResources name = " 
					+ rsc.getResourceName());
			resourceList.add(rsc);

		}
		
		//add TOP_LEVEL native resources
		rsclist = haClient.getNativeResourceList();
		for(int i=0; i<rsclist.size(); i++){
			LHAResource rsc = (LHAResource)rsclist.get(i);
			rsc.setDomainName(domainName);
			rsc.setTopLevel(true);
			rsc.completeResource();
			resourceList.add(rsc);
		}

		//add resource groups and its members
		rsclist = haClient.getResourceGroupList();
		for(int i=0; i<rsclist.size(); i++){
			LHAResource rscGroup = (LHAResource)rsclist.get(i);
			rscGroup.setDomainName(domainName);
			rscGroup.setTopLevel(true);

			EEZResourceList nativeList = 
				haClient.getSubResourceList(rscGroup.resourceKey());
			
			// all members should be running at the same node,
			String nodeName = "";
			// are all members running?
			boolean allNativeRunning = true;
			// are all members supposed to be running?
			boolean allNativeExpectedRunning = true;
			
			for(int j = 0; j < nativeList.size(); j++){
				LHAResource nativeRsc = (LHAResource)nativeList.get(j);
				nativeRsc.setTopLevel(false);
				nativeRsc.setDomainName(domainName);
				nativeRsc.completeResource();
				/* add member resource */
				resourceList.add(nativeRsc);
				
				nodeName = nativeRsc.getNodeName();
				if ( !nativeRsc.getObservedState().equals(EEZResource.OBSERVED_STATE_ONLINE)){
					allNativeRunning = false;
				}
				
				if ( !nativeRsc.getDesiredState().equals(EEZResource.DESIRED_STATE_ONLINE)){
					allNativeExpectedRunning = false;
				}
				

			}
			
			rscGroup.setNodeName(nodeName);
			rscGroup.setObservedState(allNativeRunning? 
					EEZResource.OBSERVED_STATE_ONLINE : EEZResource.OBSERVED_STATE_OFFLINE);
			rscGroup.setDesiredState(allNativeExpectedRunning?
					EEZResource.DESIRED_STATE_ONLINE : EEZResource.DESIRED_STATE_OFFLINE);
			rscGroup.completeResource();
			resourceList.add(rscGroup);
			
			for(int j = 0; j < nativeList.size(); j++){
				LHAResource nativeRsc = (LHAResource)nativeList.get(j);
				/* add HAS_MEMBER relation */
				relationList.add(new EEZRelation(EEZRelation.HAS_MEMBER,
						EEZGroupMembershipRelation.TYPE_GROUP_MEMBERSHIP,
						rscGroup.resourceKey(), nativeRsc.resourceKey()));
			}
		}
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:invalidateResources end." );
	}
	
	private void updateRelationList()
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:invalidateRelations begin." );
		String domainName = haDomain.getDomainName();

		//deal with location constraint(HOSTED_BY relation)
		ArrayList list = haClient.getLocationConstraintList();
		for(int i = 0; i < list.size(); i++){
			LHALocationConstraint cons = (LHALocationConstraint)list.get(i);
			EEZRelation rel = cons.getEEZRelation(this);
			if( rel != null ) relationList.add(rel);
		}

		//deal with colocation constraint(COLOCATION relation)
		list = haClient.getColocationConstraintList();
		for(int i = 0; i < list.size(); i++){
			LHAColocationConstraint cons = (LHAColocationConstraint)list.get(i);
			EEZRelation rel = cons.getEEZRelation(this);
			if( rel != null ) relationList.add(rel);
		}

		//deal with order constraint(ORDER relation)
		list = haClient.getOrderConstraintList();
		for(int i = 0; i < list.size(); i++){
			LHAOrderConstraint cons = (LHAOrderConstraint)list.get(i);
			EEZRelation rel = cons.getEEZRelation(this);
			if (rel != null ) relationList.add(rel);
		}
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:invalidateRelations end." );
	}


	public synchronized EEZDomain getDomain()
	{
		return haDomain;
	}

	/* find resource ignoring domain, class, nodename
	 */
	public synchronized LHAResource findLHAResource(String resourceName)
	{
		for(int i=0; i<resourceList.size(); i++){
			LHAResource rsc = (LHAResource)resourceList.get(i);
			if (rsc.getResourceName().equals(resourceName)){
					return rsc;
			}
		}
		return null;
	}

	public synchronized EEZRelationList findRelations(String type, 
			String name, EEZResourceKey source, EEZResourceKey target) 
	{
		EEZRelationList list = new EEZRelationList();
		for (int i = 0; i < relationList.size(); i++){
			EEZRelation rel = (EEZRelation)relationList.get(i);
			if( (type == null || rel.getRelationType().equals(type) )
				&& ( name == null || rel.getRelationName().equals(name))
				&& ( source == null || rel.getSourceKey().equals(source))
				&& ( target == null || rel.getTargetKey().equals(target))) {
				list.add(rel);
			}
		}
		return list;
	}
	
	public synchronized void requestOnlineOffLine(EEZResource rsc, String request_name)
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:requestOnlineOffLine begin." );
		boolean online = false;
		if ( request_name.equals(EEZRequest.REQUEST_ONLINE)) {
			online = true;
		} else if (request_name.equals(EEZRequest.REQUEST_OFFLINE)){
			online = false;
		}
		rsc.setDesiredState(online? EEZResource.DESIRED_STATE_ONLINE 
				: EEZResource.DESIRED_STATE_OFFLINE);
		
		
		if ( rsc.getResourceType().equals(EEZResource.TYPE_RESOURCE) 
				||	rsc.getResourceType().equals(EEZResource.TYPE_RESOURCE_GROUP)) {
			haClient.setResourceActiveState(rsc.getResourceName(), online);
		} else if ( rsc.getResourceType().equals(EEZResource.TYPE_NODE)) {
			haClient.setNodeActiveState(rsc.getResourceName(), online);
		}
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:requestOnlineOffLine end." );
	}
	
	public synchronized void requestIncludeExclude(EEZResource rsc, String request_name)
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:requestIncludeExclude begin." );
		boolean standby = false;
		if ( request_name.equals(EEZRequest.EXCLUDE_NODE)) {
			standby = true;
		} else if (request_name.equals(EEZRequest.INCLUDE_NODE)){
			standby = false;
		}
		
		haClient.setNodeStandbyState(rsc.getResourceName(), standby);
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager:requestIncludeExclude end." );
	}

	public void run()
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "event thread started.");
		while ( true ) {
			String event = LHAMgmtLib.wait_for_events();
			
			if ( event != null ) {
				// we got an event. Until now the only event we can get from
				// the library is cib-changed.
				LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LHAClusterManager: event received: " + event);
				EEZResourceList oldRscList = this.getAllResources();
				EEZRelationList oldRelList = this.getAllRelations();
				
				synchronized(this) {
					/* renew Lists */
					resourceList = new EEZResourceList();
					relationList = new EEZRelationList();

					updateResourceList();
					updateRelationList();
				}
				//update Data and send Events here?
				haFLA.handleEvent(event, oldRscList, oldRelList);

			}
		}
	}
}
