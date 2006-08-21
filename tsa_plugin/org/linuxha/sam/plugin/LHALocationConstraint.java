package org.linuxha.sam.plugin;

import java.util.ArrayList;

import com.ibm.eez.sdk.EEZHostingNodeRelation;
import com.ibm.eez.sdk.EEZRelation;


class LHARule {
	public final static String ATTRIBUTE_UNAME="#uname";
	public final static String OPERATION_EQUAL="eq";
	
	public String name ="";
	public String operation ="";
	public String value ="";
	
	public LHARule(String name, String operation, String value)
	{
		this.name = name;
		this.operation = operation;
		this.value = value;
	}
}


class LHALocationConstraint extends LHAConstraint {
	private String resource ="";
	private String score ="";
	private ArrayList ruleList = null;
	
	public LHALocationConstraint(String rsc, String score)
	{
		this.type = LHAConstraint.TYPE_LOCATION;
		this.resource = rsc;
		this.score = score;
		this.ruleList = new ArrayList();
	}
	
	public void setRule(String name, String operation, String value)
	{
		LHARule rule = new LHARule(name, operation, value);
		ruleList.add(rule);
	}
	

	public EEZRelation getEEZRelation(LHAClusterManager manager) {

		LHARule rule = null;
		if (ruleList == null || ruleList.size() == 0 ){
			return null;
		}
		
		rule = (LHARule)ruleList.get(0);
		if (rule.name.equals(LHARule.ATTRIBUTE_UNAME) 
				&& rule.operation.equals(LHARule.OPERATION_EQUAL)){
			String host = rule.value;
			LHAResource nodeRsc = manager.findLHAResource(rule.value);
			LHAResource nativeRsc = manager.findLHAResource(resource);
			nativeRsc.setNodeName(nodeRsc.getResourceName());
			//create a HOSTED_BY relation
			EEZRelation rel = new EEZRelation( EEZRelation.HOSTED_BY,
					EEZHostingNodeRelation.TYPE_HOSTING_NODE,
					nativeRsc.resourceKey(), nodeRsc.resourceKey());
			return rel;
		} 
		
		return null;
	}
}



