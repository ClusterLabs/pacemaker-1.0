package org.linuxha.sam.plugin;


public class LHACLIClient extends LHALIBClient
{
	synchronized String[] processCommand(String[] command)
	{
		String msg = "";
		for(int i=0; i<command.length; i++){
			msg += command[i];
			msg += " ";
		}
		String result = LHAMgmtLib.process_cmnd_external(msg);
		return result.split("\n");
	}
	
	
}

