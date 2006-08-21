package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZResource;

class LHANodeResource extends LHAResource {
	private String online_status ="";
	private String standby_status = "";
	private String dc_status = "";
	private String expected_up_status = "";
	
	public LHANodeResource()
	{
		super();
		this.setResourceType(EEZResource.TYPE_NODE);
	}
	
	public void setConfigStatus(String [] config)
	{
		online_status = config[2];
		standby_status = config[3];
	}
	public void completeResource()
	{
	
		String observed_state = EEZResource.OBSERVED_STATE_OFFLINE;
		String desired_state = EEZResource.DESIRED_STATE_OFFLINE;
		
		if (online_status.equals("True")){
			observed_state = EEZResource.OBSERVED_STATE_ONLINE;
			/* if it is online, then it is desired to be online */
			desired_state = EEZResource.DESIRED_STATE_ONLINE;
		}
		this.setObservedState(observed_state);
		this.setDesiredState(desired_state);

		this.setCompoundState(EEZResource.COMPOUND_STATE_OK);
		if(standby_status.equals("True")) {
			this.setIncludedForAutomation(false);
		} else {
			this.setIncludedForAutomation(true);
		}
		this.setResourceClass("LinuxHA.node");	
		this.setOperationalState(new String[]{EEZResource.OPERATIONAL_STATE_OK});

	}
	
}