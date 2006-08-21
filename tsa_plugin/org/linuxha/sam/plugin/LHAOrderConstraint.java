package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZRelation;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceToResourceRelation;

class LHAOrderConstraint extends LHAConstraint {
	public final static String ACTION_AFTER = "after";
	public final static String ACTION_BEFORE = "before";
	
	private String from;
	private String to;
	private String action;
	
	public LHAOrderConstraint(String from, String to, String action)
	{
		this.type = LHAConstraint.TYPE_ORDER;
		this.from = from;
		this.to = to;
		this.action = action;
	}
	

	public EEZRelation getEEZRelation(LHAClusterManager mgr)
	{
		EEZResourceToResourceRelation rel = new EEZResourceToResourceRelation();
		rel.setRelationName(EEZRelation.STARTS_AFTER);
		
		EEZResource sourceResource = mgr.findLHAResource(from);
		EEZResource targetResource = mgr.findLHAResource(to);
		
		if (action.equals(LHAOrderConstraint.ACTION_AFTER)){
			rel.setSourceKey(sourceResource.resourceKey());
			rel.setTargetKey(targetResource.resourceKey());
		} else if (action.equals(LHAOrderConstraint.ACTION_BEFORE)){
			rel.setSourceKey(targetResource.resourceKey());
			rel.setTargetKey(sourceResource.resourceKey());			
		}
		return rel;
	}
}
