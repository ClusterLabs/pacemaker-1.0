package org.linuxha.sam.plugin;

import java.io.IOException;
import java.io.InputStream;

public class Utilities {
	
	static String[] execCmd(String command)
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "Utilities:command=" + command);
		StringBuffer sb = new StringBuffer();		
	
		try {
			Process process = Runtime.getRuntime().exec(command);
			InputStream in = process.getInputStream();

			int c;
			while ((c = in.read()) != -1){
				sb.append((char)c);
			} 
		}catch (IOException e){
			System.out.println(e);
		}
	
		String result = sb.toString();
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "Utilities:result=" + result);
		return result.split("\n");
	}
}
