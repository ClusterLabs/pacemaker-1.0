package org.linuxha.sam.plugin;


public class LHAConstraint {
	public final static String TYPE_LOCATION = "location";
	public final static String TYPE_COLOCATION = "colocation";
	public final static String TYPE_ORDER = "order";
	
	protected String type ="";
	
	public String getType()
	{
		return type;
	}
}

