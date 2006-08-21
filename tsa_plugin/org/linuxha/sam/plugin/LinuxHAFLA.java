package org.linuxha.sam.plugin;

import java.util.Properties;

import com.ibm.eez.adapter.sdk.EEZAdapterInteractionMethods;
import com.ibm.eez.adapter.sdk.EEZTecEvent;
import com.ibm.eez.sdk.exceptions.EEZAdapterException;
import com.ibm.eez.sdk.EEZDomain;
import com.ibm.eez.sdk.EEZEvent;
import com.ibm.eez.sdk.EEZFilterCriteriaList;
import com.ibm.eez.sdk.EEZGroupMembershipRelation;
import com.ibm.eez.sdk.EEZPropertyList;
import com.ibm.eez.sdk.EEZRelationEvent;
import com.ibm.eez.sdk.EEZRelationList;
import com.ibm.eez.sdk.EEZRequest;
import com.ibm.eez.sdk.EEZRequestList;
import com.ibm.eez.sdk.EEZResourceKey;
import com.ibm.eez.sdk.EEZResourceKeyList;
import com.ibm.eez.sdk.EEZResourceList;
import com.tivoli.tec.event_delivery.TECEvent;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceEvent;
import com.ibm.eez.sdk.EEZRelation;
//import com.ibm.eez.sdk.EEZGroupMembershipRelation;
//import com.ibm.eez.sdk.EEZHostingNodeRelation;
//import com.ibm.eez.sdk.EEZEvent;
import com.ibm.eez.sdk.EEZFilterCriteria;
import com.ibm.eez.adapter.sdk.EEZAdapterRequestNotSupportedException;
import com.ibm.eez.adapter.sdk.EEZTecEmitter;
//import com.ibm.eez.adapter.EEZAdapterLogger;


public class LinuxHAFLA implements EEZAdapterInteractionMethods {
	private LHAClusterManager clusterManager;
	private EEZTecEmitter tecEmitter;

	
	public LinuxHAFLA() 
	{
		clusterManager = new LHAClusterManager(this);
	}
	
	public boolean match(Properties context)
	{
		return true;
	}
	
	public boolean isConnected()
	{
		return true;
	}

	public void connect(Properties context)
		throws EEZAdapterException {
	}

	public void disconnect()
	{
	}


	public EEZPropertyList request(EEZRequest request, Properties context)
		throws EEZAdapterException 
	{
		// not support !
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA: request(request) begin"); 
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA: request(request) end, FIXME: return what?");
		throw new EEZAdapterRequestNotSupportedException(request);
	}

	public void request(EEZRequestList reqlist, Properties context)
		throws EEZAdapterException
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:request(request list) begin, size = " 
				+ reqlist.size());
		for(int i = 0; i < reqlist.size(); i++){
			EEZRequest rq = (EEZRequest)reqlist.get(i);
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LinuxHAFLA:request " + i);
			LHAResource rsc = clusterManager.findResource(rq.resourceKey());
			String request_name = rq.getRequestName();
			
			if(request_name.equals(EEZRequest.REQUEST_OFFLINE)
				|| request_name.equals(EEZRequest.REQUEST_ONLINE)){
				/* try bring it online or offline */
				if ( rsc.getResourceType().equals(EEZResource.TYPE_RESOURCE_GROUP)  
						&& rsc.getResourceClass().equals(EEZResource.GROUP_CLASS_COLLECTION)){
					EEZResourceList list = enumerateGroupMembers(rq.resourceKey(), null);
					for(int j = 0; j < list.size(); j++) {
						EEZResource sub_rsc = (EEZResource)list.get(j);
						clusterManager.requestOnlineOffLine(sub_rsc, request_name);
					}
				}
				clusterManager.requestOnlineOffLine(rsc, request_name);
				
			} else if (request_name.equals(EEZRequest.EXCLUDE_NODE)
					|| request_name.equals(EEZRequest.INCLUDE_NODE)) {
				clusterManager.requestIncludeExclude(rsc, request_name);
			} else if (request_name.equals(EEZRequest.RESET_FROM_NON_RECOVERABLE_ERROR)){
			
			} else {
				throw new EEZAdapterRequestNotSupportedException(rq);
			}
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:request(request list) end.");
	}
	
	public EEZResourceList enumerateGroupMembers(EEZResourceKey key, 
				Properties context) throws EEZAdapterException
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:enumerateGroupMembers() begin: key = " 
				+ key.toString());
		EEZRelationList rel_list = 
			clusterManager.findRelations(EEZGroupMembershipRelation.TYPE_GROUP_MEMBERSHIP,
					EEZRelation.HAS_MEMBER, key, null);
		EEZResourceList list = new EEZResourceList();
		
		if (rel_list != null)	{
			int size = rel_list.size();
			for (int i = 0; i < size; i++) {
				EEZResourceKey member_key = (EEZResourceKey)
						((EEZRelation)rel_list.get(i)).getTargetKey();
				LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, 
						"LinuxHAFLA:enumerateGroupMembers(): member key = " 
						+ member_key.toString());
				LHAResource rsc = clusterManager.findResource(member_key);

				if (rsc != null) {
					list.add(rsc.getEEZResource());
				}
			}
		}
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateGroupMembers() end.");
		return list;
	}
	
	public EEZResourceList enumerateResources(EEZResourceKeyList keys, 
			Properties context) throws EEZAdapterException
	{
		EEZResourceList list = new EEZResourceList();
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateResources(key) end.");
		int size = keys.size();
		for(int i=0; i<size; i++){
			EEZResourceKey key = (EEZResourceKey)keys.get(i);
			LHAResource resource = (LHAResource)clusterManager.findResource(key);
			if (resource != null ) {
				list.add(resource.getEEZResource());
			}
		}

		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateResources(key) end.");
		return list;
	}
	
	public EEZResourceList enumerateResources(EEZFilterCriteriaList filter, 
			Properties context)	throws EEZAdapterException{
		EEZResourceList list = null;
		
		EEZFilterCriteria crit = (EEZFilterCriteria) filter.get(0);
		if ( crit.equals(EEZFilterCriteria.TOP_LEVEL_RESOURCE_CRITERIA)) {
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:enumerateResources() begin: TOP_LEVEL_RESOURCE_CRITERIA");
			list = clusterManager.getAllTopLevelResources();
		} else if (crit.equals(EEZFilterCriteria.QUERY_ALL_RESOURCES_CRITERIA)) {
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:enumerateResources() begin: QUERY_ALL_RESOURCES_CRITERIA");
			list = clusterManager.getAllResources();
		} else {
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateResources() begin");
			int nrfilter = filter.size();
			list = clusterManager.getAllResources();

			for(int i = 0; i < nrfilter; i++){
				crit = (EEZFilterCriteria) filter.get(i);
				String value = "";
				if ( crit.getValues() != null && crit.getValues().length > 0) {
					value = crit.getValues()[0];
				}
				LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateResources(): " 
					+ crit.getName() + crit.getOperation() 
					+ value);
				int nrrsc = list.size();
				for (int j = nrrsc -1 ; j >= 0; j--) {
					LHAResource rsc = (LHAResource) list.get(j);
					if (! matchResourceCriteria(rsc, crit)){
						list.remove(j);
					}
				}
			}
		}

		EEZResourceList new_list = new EEZResourceList();
		for(int i = 0; i < list.size(); i++) {
			LHAResource rsc = (LHAResource)list.get(i);
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:enumerateResources(): Got ResourceName = " 
				+ rsc.getResourceName());
			new_list.add(rsc.getEEZResource());
		}

		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateResources() done.");
		return new_list;
	}
		
	void addAllNoDuplicates(EEZRelationList target, EEZRelationList src) {
		int size = src.size();
		for (int i = 0; i < size; i++) {
			if (!target.contains(src.get(i))) {
				target.add(src.get(i));
			}
		}
	}

	public EEZRelationList enumerateRelations(EEZFilterCriteriaList filter, 
			Properties context)	throws EEZAdapterException {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
				"LinuxHAFLA:enumerateRelations() begin. filter contains " 
				+ filter.size() + " Criterias.");

		// first divide criteria into resource specific and relation specific
		EEZFilterCriteriaList res_filter = new EEZFilterCriteriaList();
		EEZFilterCriteriaList rel_filter = new EEZFilterCriteriaList();
		int size = filter.size();
		String rel_direction = null;
		
		for (int i = 0; i < size; i++)
		{
			EEZFilterCriteria crit = (EEZFilterCriteria)filter.get(i);
			
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateRelations() CriteriaName = "
						+ crit.getName());
			if (crit.getName().equals(EEZFilterCriteria.RESOURCE_NAME)
				|| crit.getName().equals(EEZFilterCriteria.RESOURCE_TYPE)
				|| crit.getName().equals(EEZFilterCriteria.RESOURCE_CLASS)
				|| crit.getName().equals(EEZFilterCriteria.RESOURCE_NODE)) {
				res_filter.add(crit);
			} else {
				if (crit.getName().equals(EEZFilterCriteria.RELATION_DIRECTION)) {
					if (crit.getOperation().equals(EEZFilterCriteria.EQUAL)) {
							rel_direction = crit.getValues()[0];
					} else {
						if (crit.getValues()[0].equals(EEZFilterCriteria.RELATION_DIRECTION_FORWARD))
							rel_direction = EEZFilterCriteria.RELATION_DIRECTION_FORWARD;
						else
							rel_direction = EEZFilterCriteria.RELATION_DIRECTION_FORWARD; 
					}
				} else if (crit.getName().equals(EEZFilterCriteria.RELATION_TYPE)) {
						rel_filter.add(crit);
				} else if (crit.getName().equals(EEZFilterCriteria.RELATION_NAME)) {
						rel_filter.add(crit);
				}
			}
		}
		if (rel_direction == null) {
			rel_direction = EEZFilterCriteria.RELATION_DIRECTION_FORWARD;
		}
		
		EEZRelationList relations = new EEZRelationList();
		if (res_filter.size() == 0) {
			relations.addAll(clusterManager.getAllRelations());
		} else {
			EEZResourceList rl = enumerateResources(res_filter, context);
		
			if (rl == null || rl.size() == 0) {
				return new EEZRelationList();
			}

			size = rl.size();
			for (int i = 0; i < size; i++) {
				EEZResource act_res = (EEZResource)rl.get(i);
				EEZRelationList res_rels;
				
				if (rel_direction.equals(EEZFilterCriteria.RELATION_DIRECTION_FORWARD)) {
					res_rels = clusterManager.findRelations(
						null, null, act_res.resourceKey(), null);
				} else {
					res_rels = clusterManager.findRelations(null, null, null, act_res.resourceKey());	
				}
				addAllNoDuplicates(relations, res_rels);			
			}
		}		

		int num_crit = rel_filter.size();
		for (int run = 0; run < num_crit; run++) {
			size = relations.size();
			for (int i = size-1; i >= 0; i--) {
				// the usual backwards filtering
				EEZRelation act_rel = (EEZRelation)relations.get(i);
				if (!matchRelationCriteria(act_rel, (EEZFilterCriteria)rel_filter.get(run))) {
					relations.remove(i);
				}
			}
		}
		
		for( int i = 0; i < relations.size(); i++){
			EEZRelation rel = (EEZRelation)relations.get(i);
			LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, 
					"LinuxHAFLA:enumerateRelations(): Got relation: "  
					+ rel.toString());
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:enumerateRelations() end.");
		return relations;
	}

	public EEZResourceKeyList subscribeResources(EEZResourceKeyList keylist, Properties context)
		throws EEZAdapterException {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:subscribeResources() begin.");
		EEZResourceKeyList failedList = new EEZResourceKeyList();
		for(int i = 0; i < keylist.size(); i++){
			LHAResource rsc = clusterManager.findLHAResource(((EEZResourceKey)keylist.get(i)).getResourceName());
			if (rsc != null ){
				rsc.setSubscribed(true);
			}else {
				failedList.add((EEZResourceKey)keylist.get(i));
			}
		} 
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:subscribeResources() end.");
		return failedList;
	}
		

	public EEZResourceKeyList unsubscribeResources(EEZResourceKeyList keylist, Properties context)
		throws EEZAdapterException {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:unsubscribeResources() begin.");
		EEZResourceKeyList failedList = new EEZResourceKeyList();
		for(int i=0; i<keylist.size(); i++){
			LHAResource rsc = clusterManager.findLHAResource(((EEZResourceKey)keylist.get(i)).getResourceName());
			if (rsc != null ){
				rsc.setSubscribed(false);
			}else {
				failedList.add((EEZResourceKey)keylist.get(i));
			}
		} 
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:unsubscribeResources() end.");
		return failedList;
	}

	public EEZDomain getDomain(Properties context)
		throws EEZAdapterException{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:getDomain() begin.");
		EEZDomain domain = clusterManager.getDomain();
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:getDomain() end.");
		return domain;
	}
		
	public EEZDomain initializeDomain(Properties context)
		throws EEZAdapterException{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:initializeDomain() begin.");

		/* init cluster manager */
		clusterManager.initialize();

		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:initializeDomain() end.");
		return clusterManager.getDomain();
	}

	public void terminateDomain()
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:terminateDomain() begin.");
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:terminateDomain() end.");
	}

	public void mapEvent(TECEvent in_evnt, EEZTecEvent out_evnt)
		throws EEZAdapterException
	{
		String props = in_evnt.getSlot(EEZTecEvent.SLOT_PROP_LIST);
		if (props != null ){
			EEZPropertyList list = EEZTecEvent.convertStringToProperties(props);
			if (list != null && list.getProperty("DoReject") != null
					&& list.getProperty("DoReject").getValue().equals("yes")){
				out_evnt.markRejected();
				return;
			}
		}
		out_evnt.copyEvent(in_evnt);
	}

	public EEZDomain getHealthState(Properties context)
		throws EEZAdapterException {
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:getHealthState() begin.");
		EEZDomain domain =  clusterManager.getDomain();
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:getHealthState() end.");
		return domain;
	}
	
	
	boolean matchResourceCriteria(EEZResource rsc, EEZFilterCriteria crit)
	{
		String match = "";	// the value to be matched.
		String critname = crit.getName();
		if (critname.equals(EEZFilterCriteria.RESOURCE_NAME)){
			match = rsc.getResourceName(); 
		} else if(critname.equals(EEZFilterCriteria.RESOURCE_CLASS)){
			match = rsc.getResourceClass();
		} else if(critname.equals(EEZFilterCriteria.RESOURCE_NODE)){
			match = rsc.getNodeName();
		} else if(critname.equals(EEZFilterCriteria.RESOURCE_TYPE)){
			match = rsc.getResourceType();
		}
		
		int nrvals = crit.getValues().length;
		for(int i=0; i<nrvals; i++){
			String regex = "^"+crit.getValues()[i].replaceAll("\\*", "\\.\\*")+"$";
			
			if (crit.getOperation().equals(EEZFilterCriteria.NOT_EQUAL) 
					&& !match.matches(regex)){
				return true;
			} else if ( crit.getOperation().equals(EEZFilterCriteria.EQUAL) 
					&& match.matches(regex)){
				return true;
			}
		}
		return false;
	}


	boolean matchRelationCriteria(EEZRelation rel, EEZFilterCriteria crit)
		throws EEZAdapterException {
		String match = "";

		if (crit.getName().equals(EEZFilterCriteria.RELATION_NAME)) {
			match = rel.getRelationName();
		} else if (crit.getName().equals(EEZFilterCriteria.RELATION_TYPE)) {
			match = rel.getRelationType();
		}

		int nrvals = crit.getValues().length;
		for (int i = 0; i < nrvals; i++) {
			String regex = "^"+crit.getValues()[i].replaceAll("\\*", "\\.\\*")+"$";
		
			if ( crit.getOperation().equals(EEZFilterCriteria.EQUAL) 
				&& match.matches(regex) ) {
				return true;
			} else if (crit.getOperation().equals(EEZFilterCriteria.NOT_EQUAL) 
				&& !match.matches(regex)) {
				return true;
			}
		}
		return false; 
	}
	
	

	public void handleEvent(String event, EEZResourceList oldRscList, EEZRelationList oldRelList )
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:handleEvents: enter");

		EEZResourceList rscList = clusterManager.getAllResources();
		EEZRelationList relList = clusterManager.getAllRelations();
		
		/* R E S O U R C E S */
		for( int i = 0; i < rscList.size(); i++) {
			boolean found = false;
			LHAResource rsc = (LHAResource)rscList.get(i);
			
			for(int j = 0; j < oldRscList.size(); j++){
				LHAResource oldRsc = (LHAResource)oldRscList.get(j);
				if (oldRsc.getResourceName().equals(rsc.getResourceName())) {
					oldRscList.remove(j);
					found = true;
					break;
				}
			}
			if ( found ){
				/* modified resources */
				sendEvent(rsc.getEEZResource(), EEZResourceEvent.EVENT_RESOURCE_MODIFIED);
			} else {
				/* new added resources */
				sendEvent(rsc.getEEZResource(), EEZResourceEvent.EVENT_RESOURCE_ADDED);
			}
		}
		
		/* deleted resources */
		for(int j = 0; j < oldRscList.size(); j++){
			LHAResource oldRsc = (LHAResource)oldRscList.get(j);
			sendEvent(oldRsc.getEEZResource(), EEZResourceEvent.EVENT_RESOURCE_DELETED);
		}
		
		
		/* R E L A T I O N S */ 
		EEZRelationList newRelList = new EEZRelationList(); 
		for( int i = 0; i < relList.size(); i++) {
			EEZRelation rel = (EEZRelation)relList.get(i);
			boolean found = false;
			for(int j = 0; j < oldRelList.size(); j++){
				EEZRelation oldRel = (EEZRelation)oldRelList.get(j);
				if (oldRel.getRelationName().equals(rel.getRelationName())) {
					oldRelList.remove(j);
					found = true;
					break;
				}
			}
			if ( !found ){
				newRelList.add((EEZRelation)relList.get(i));
			}
		}
		
		/* added relations */
		for(int j = 0; j < newRelList.size(); j++){
			EEZRelation newRel = (EEZRelation)newRelList.get(j);
			sendEvent(newRel, EEZRelationEvent.EVENT_RELATION_ADDED);
		}
		
		/* deleted relations */
		for(int j = 0; j < oldRelList.size(); j++){
			EEZRelation oldRel = (EEZRelation)oldRelList.get(j);
			sendEvent(oldRel, EEZRelationEvent.EVENT_RELATION_DELETED);
		}
		
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAFLA:handleEvents: exit");
	}
	
	public void sendEvent(EEZResource res, String reason )
	{
		if (tecEmitter == null) throw new IllegalStateException();
		EEZTecEvent event = new EEZTecEvent();
		
		event.setBaseEvent( EEZEvent.TYPE_RESOURCE, reason,
                            res.getDomainName(), 0 /* seq number */); 

		event.setResource(res.getResourceType(), res.getResourceName(),
                         res.getResourceClass(), res.getNodeName());

		event.setResourceStatus(res.getDesiredState(), res.getObservedState(),
                               res.getOperationalState(),
                               res.getCompoundState(), res.getIncludedForAutomation());

		LHAMgmtLib.clLog(LHAMgmtLib.LOG_DEBUG, "LinuxHAFLA:sendEvent: reason = " + reason
				+ " resource = " + res.resourceKey().toString() 
				+ " state = " + res.getObservedState()
				+ " IncludedForAutomation = " + res.getIncludedForAutomation());
		
		if ( res.getResourceType().equals(EEZResource.TYPE_RESOURCE_GROUP) )
				event.setResourceGroup( res.getGroupClass(), res.getAvailabilityTarget() );
		tecEmitter.send( event );
	}
	
	
	public void sendEvent(EEZRelation rel,  String reason)
	{
		if (tecEmitter == null) throw new IllegalStateException();
		EEZTecEvent event = new EEZTecEvent();
		String domainName = clusterManager.getDomain().getDomainName();
		
		event.setBaseEvent( EEZEvent.TYPE_RELATION, reason,
				domainName, 0 /* seq number */); 
		
		event.setRelation(rel.getRelationType(), rel.getRelationName());
		
		event.setRelationSourceKey(
				"", 	/*  @param type     Type of the source object.
                      This parameter exist for backward compatibitly only.
                      It will be stored in the event but no longer used in the convertion to EEZEvent. */
				((EEZResourceKey) rel.getSourceKey()).getResourceName(), 
				((EEZResourceKey) rel.getSourceKey()).getResourceClass(),
				((EEZResourceKey) rel.getSourceKey()).getNodeName());
		
		event.setRelationTargetKey(
				"",  
				((EEZResourceKey) rel.getTargetKey()).getResourceName(), 
				((EEZResourceKey) rel.getTargetKey()).getResourceClass(),
				((EEZResourceKey) rel.getTargetKey()).getNodeName());
		
		tecEmitter.send( event );
	}


	public void setEventEmitter(EEZTecEmitter emitter) {
		tecEmitter = emitter;
	}
	
} // class
