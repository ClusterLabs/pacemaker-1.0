package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZResource;


public abstract class LHAResource extends EEZResource {
	private boolean subscribed;
	private boolean toplevel;
	
	abstract public void completeResource();
	public void setSubscribed(boolean b) {
		subscribed = b;
	}
	
	public boolean isSubscribed(){
		return subscribed;
	}

	public void setTopLevel(boolean b) 
	{
		toplevel = b;
	}
	public boolean isTopLevel()
	{
		return toplevel;
	}
	
	public EEZResource getEEZResource()
	{
		EEZResource rsc = new EEZResource(this);
		return rsc;
	}
}





