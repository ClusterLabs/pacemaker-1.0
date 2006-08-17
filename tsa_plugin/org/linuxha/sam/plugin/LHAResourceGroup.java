package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZResource;

class LHAResourceGroup extends LHAResource {
	public LHAResourceGroup ()
	{
		super();
		setResourceType(EEZResource.TYPE_RESOURCE_GROUP);
	}
	
	public void completeResource()
	{
		setCompoundState(EEZResource.COMPOUND_STATE_OK);
		setOperationalState(new String[]{EEZResource.OPERATIONAL_STATE_OK});
	}

}