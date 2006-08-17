package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZResource;
class LHANativeResource extends LHAResource {
	private String hosting_node = "";
	private String container_id = "";
	private String target_role = "";
	private String status = "";
	public LHANativeResource ()
	{
		super();
		this.setResourceType(EEZResource.TYPE_RESOURCE);
	}
	
	public void setHostingNode(String node)
	{
		hosting_node = node;
	}
	
	public String getHostingNode()
	{
		return hosting_node;
	}
	
	public void setContainerId(String id)
	{
		container_id = id;
	}
	
	public String getContainerId()
	{
		return container_id;
	}
	
	public void setTargetRole(String target_role)
	{
		this.target_role = target_role;
	}
	
	public void setStatus(String status)
	{
		this.status = status;
	}
	
	public void completeResource()
	{
		String observed_state = EEZResource.OBSERVED_STATE_OFFLINE;
		String desired_state = EEZResource.DESIRED_STATE_ONLINE;

		this.setResourceClass("");

		this.setCompoundState(EEZResource.COMPOUND_STATE_OK);
		this.setIncludedForAutomation(true);
		this.setOperationalState(new String[]{EEZResource.OPERATIONAL_STATE_OK});

		if (status.equals("running")){
			observed_state = EEZResource.OBSERVED_STATE_ONLINE;
		} 
		if (target_role.equals("stopped")){
			desired_state = EEZResource.DESIRED_STATE_OFFLINE;
		}
		this.setObservedState(observed_state);
		this.setDesiredState(desired_state);
	}
}
