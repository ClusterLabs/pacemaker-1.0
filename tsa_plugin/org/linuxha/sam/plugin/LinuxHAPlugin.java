package org.linuxha.sam.plugin;

import java.util.Properties;
import java.util.ArrayList;

import com.ibm.eez.sdk.exceptions.EEZAdapterException;
import com.ibm.eez.adapter.EEZAdapterLogger;
import com.ibm.eez.adapter.sdk.EEZAdapterInteraction;
import com.ibm.eez.adapter.sdk.EEZAdapterInteractionMethods;
import com.ibm.eez.adapter.sdk.EEZAdapterNoSuchMethodException;
import com.ibm.eez.sdk.EEZDomain;
import com.ibm.eez.sdk.EEZFilterCriteria;
import com.ibm.eez.sdk.EEZFilterCriteriaList;
import com.ibm.eez.sdk.EEZPropertyList;
import com.ibm.eez.sdk.EEZRelationList;
import com.ibm.eez.sdk.EEZRequest;
import com.ibm.eez.sdk.EEZRequestList;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceKey;
import com.ibm.eez.sdk.EEZResourceKeyList;
import com.ibm.eez.sdk.EEZResourceList;
import com.tivoli.tec.event_delivery.TECEvent;
import com.ibm.eez.sdk.EEZPolicyInfo;
import com.ibm.eez.sdk.EEZConst;

//import com.ibm.eez.sdk.EEZResourceToResourceRelation;
//import com.ibm.eez.sdk.EEZProperty;
//import com.ibm.eez.sdk.EEZResource;
//import com.ibm.eez.sdk.EEZResourceEvent;
//import com.ibm.eez.sdk.EEZRelation;
//import com.ibm.eez.sdk.EEZGroupMembershipRelation;
//import com.ibm.eez.sdk.EEZHostingNodeRelation;
//import com.ibm.eez.sdk.EEZEvent;
//import com.ibm.eez.sdk.EEZFilterCriteria;
//import com.ibm.eez.sdk.EEZEvent;
//import com.ibm.eez.sdk.EEZFilterCriteria;
import com.ibm.eez.adapter.sdk.EEZTecEvent;
import com.ibm.eez.adapter.sdk.EEZTecEmitter;

public class LinuxHAPlugin implements EEZAdapterInteraction {
	static final String cvCopyright="www.linux-ha.org";
	private ArrayList InteractionObjList;
	
	public LinuxHAPlugin()
	{
		InteractionObjList = new ArrayList();
	}
	
	public String getPluginVersion()
	{
	    return com.ibm.eez.utils.EEZCopyright.EEZProductVersionDetails;
	}
	
	public void invoke(String methodName, Properties context, 
			Object inputObject, Object outputObject) 
			throws EEZAdapterException
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAPlugin:invoke() " + methodName + " begin.");
		LinuxHAFLA fla = (LinuxHAFLA)getInteractionObj(context);
		
		if(methodName.equals(EEZConst.MAP_EVENT)) {
			if (!fla.isConnected()){
				fla.connect(context);
			}
			fla.mapEvent((TECEvent)inputObject, (EEZTecEvent)outputObject);
			
		} else if(methodName.equals(INIT_DOMAIN)){
			/* FIXME: according to the document, the emitter is passed for test purpose only */
			fla.setEventEmitter((EEZTecEmitter)inputObject);
			EEZDomain domain = fla.initializeDomain(context);
			((EEZDomain)outputObject).setDomainName(domain.getDomainName());
			((EEZDomain)outputObject).setDomainState(domain.getDomainState());
			((EEZDomain)outputObject).setAdapterProductName(domain.getAdapterProductName());
			((EEZDomain)outputObject).setAdapterVersion(domain.getAdapterVersion());
			((EEZDomain)outputObject).setAdapterStartupTime(domain.getAdapterStartupTime());
			((EEZDomain)outputObject).setAdapterLocation(domain.getAdapterLocation());
			((EEZDomain)outputObject).setAutomationProductName(domain.getAutomationProductName());
			((EEZDomain)outputObject).setAutomationVersion(domain.getAutomationVersion());
			((EEZDomain)outputObject).setAutomationStartupTime(domain.getAutomationStartupTime());
			((EEZDomain)outputObject).setAutomationLocation(domain.getAutomationLocation());
			((EEZDomain)outputObject).setAdapterCapabilityXML(domain.getAdapterCapabilityXML());
			((EEZDomain)outputObject).setPolicyInfo(domain.getPolicyInfo());
			((EEZDomain)outputObject).setProperties(domain.getProperties());
			
		} else if(methodName.equals(TERM_DOMAIN)){
			int size = this.InteractionObjList.size();
			for(int i=0; i < size; i++)	{
				((EEZAdapterInteractionMethods)InteractionObjList.get(i)).disconnect();
			}
			((EEZAdapterInteractionMethods)InteractionObjList.get(0)).terminateDomain();
		} else if(methodName.equals(EEZConst.GET_DOMAIN)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			
			EEZDomain domain = fla.getDomain(context);
			((EEZDomain)outputObject).setDomainName(domain.getDomainName());
			((EEZDomain)outputObject).setDomainState(domain.getDomainState());
			((EEZDomain)outputObject).setAdapterProductName(domain.getAdapterProductName());
			((EEZDomain)outputObject).setAdapterVersion(domain.getAdapterVersion());
			((EEZDomain)outputObject).setAdapterStartupTime(domain.getAdapterStartupTime());
			((EEZDomain)outputObject).setAdapterLocation(domain.getAdapterLocation());
			((EEZDomain)outputObject).setAutomationProductName(domain.getAutomationProductName());
			((EEZDomain)outputObject).setAutomationVersion(domain.getAutomationVersion());
			((EEZDomain)outputObject).setAutomationStartupTime(domain.getAutomationStartupTime());
			((EEZDomain)outputObject).setAutomationLocation(domain.getAutomationLocation());
			((EEZDomain)outputObject).setAdapterCapabilityXML(domain.getAdapterCapabilityXML());
			EEZPolicyInfo policyinfo = new EEZPolicyInfo();
			policyinfo.setPolicyName(domain.getPolicyInfo().getPolicyName());
			policyinfo.setPolicyActivationTime(domain.getPolicyInfo().getPolicyActivationTime());
			((EEZDomain)outputObject).setPolicyInfo(policyinfo);
			((EEZDomain)outputObject).setProperties(domain.getProperties());
			
		} else if(methodName.equals(EEZConst.ENUMERATE_GROUP_MEMBERS)){
			if(!fla.isConnected()){
				fla.connect(context);
			}
			EEZResourceList rsclist=fla.enumerateGroupMembers(
					(EEZResourceKey)((EEZResourceKeyList)inputObject).get(0),
					context);
			if(rsclist != null){
				((EEZResourceList)outputObject).addAll(rsclist);
			}
		} else if(methodName.equals(EEZConst.ENUMERATE_RELATION)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			
			EEZRelationList rltlst = fla.enumerateRelations(
					(EEZFilterCriteriaList)inputObject, context);

			if (rltlst != null)	{
				((EEZRelationList)outputObject).addAll(rltlst);			
			}
		} else if(methodName.equals(EEZConst.ENUMERATE_RESOURCE_BYFILTER)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			
			EEZResourceList rsclst = fla.enumerateResources(
					(EEZFilterCriteriaList)inputObject, context);
			((EEZResourceList)outputObject).addAll(rsclst);
			
		} else if(methodName.equals(EEZConst.ENUMERATE_RESOURCE_BYKEY)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			
			EEZResourceList rsclst = fla.enumerateResources(
					(EEZResourceKeyList)inputObject, context);

			if (rsclst != null)	{
				((EEZResourceList)outputObject).addAll(rsclst);			
			}
		} else if (methodName.equals(EEZConst.CHECK_HEALTH)){
			if(!fla.isConnected()) {
				fla.connect(context);
			}
			EEZDomain domain = fla.getDomain(context);
			((EEZDomain)outputObject).setDomainName(domain.getDomainName());
			((EEZDomain)outputObject).setDomainState(domain.getDomainState());
		} else if (methodName.equals(EEZConst.EXECUTE_SOLICITED_REQUEST)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}

			EEZPropertyList lst = fla.request(
					(EEZRequest)((EEZRequestList)inputObject).get(0),
					context);
					
			if (lst != null){
					((EEZPropertyList)outputObject).addAll(lst);			
			}
		} else if (methodName.equals(EEZConst.EXECUTE_UNSOLICITED_REQUEST)){
			if (!fla.isConnected())	{
					fla.connect(context);
			}
				
			fla.request((EEZRequestList)inputObject, context);
		} else if(methodName.equals(EEZConst.SUBSCRIBE_RESOURCE)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			EEZResourceKeyList failedList = fla.subscribeResources((EEZResourceKeyList)inputObject, context);
			((EEZResourceKeyList)outputObject).addAll(failedList);
		} else if (methodName.equals(EEZConst.UNSUBSCRIBE_RESOURCE)){
			if (!fla.isConnected())	{
				fla.connect(context);
			}
			fla.unsubscribeResources((EEZResourceKeyList)inputObject, context);
		} else {
			EEZAdapterNoSuchMethodException ex = 
				new EEZAdapterNoSuchMethodException(methodName);
			EEZAdapterLogger.throwing("LinuxHAPlugin","invoke",	ex);
			throw ex; 
		}
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "LinuxHAPlugin:invoke() "+ methodName +" end.");
	}	

	private synchronized EEZAdapterInteractionMethods 
	getInteractionObj(Properties context) throws EEZAdapterException
	{
		int size = InteractionObjList.size();
		EEZAdapterInteractionMethods methods = null;
		for(int i = 0; i < size; i++){
			methods = ((EEZAdapterInteractionMethods)InteractionObjList.get(i));
			if(methods.match(context)){
				return methods;
			}
		}
		methods = new LinuxHAFLA();
		InteractionObjList.add(methods);
		return methods;
	}
	
	
	// only for testing purpose
	public static void main(String args[]) throws Exception
	{
		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "----------------------- testing begin -----------------------");
		EEZResource node_rsc = null;

		LinuxHAPlugin plugin = new LinuxHAPlugin();
			
		EEZDomain domain = new EEZDomain();
		plugin.invoke(INIT_DOMAIN, null, null, domain);
		
		if ( true ) {
			// get cluster nodes
			EEZFilterCriteriaList filter = new EEZFilterCriteriaList();
			EEZFilterCriteria crit = new EEZFilterCriteria();
			EEZResourceList resList = new EEZResourceList();
			crit.setName(EEZFilterCriteria.RESOURCE_TYPE);
			crit.setOperation(EEZFilterCriteria.EQUAL);
			crit.setValues(new String[]{EEZResource.TYPE_NODE});
			filter.add(crit);
			plugin.invoke(EEZConst.ENUMERATE_RESOURCE_BYFILTER, null, filter, resList);
			System.out.println(resList);
			node_rsc = (EEZResource)resList.get(0);
		}

		if ( true ) {
			//get other resources
			EEZResourceList resList = new EEZResourceList();
			EEZFilterCriteriaList filter = new EEZFilterCriteriaList();
			EEZFilterCriteria crit = new EEZFilterCriteria();
			crit.setName(EEZFilterCriteria.RESOURCE_TYPE);
			crit.setOperation(EEZFilterCriteria.NOT_EQUAL);
			crit.setValues(new String[]{EEZResource.TYPE_NODE});
			filter.add(crit);
			plugin.invoke(EEZConst.ENUMERATE_RESOURCE_BYFILTER, null, filter, resList);
			System.out.println(resList);
		}

		
		if ( true ) {
			EEZFilterCriteriaList filter = new EEZFilterCriteriaList();
			EEZFilterCriteria crit = new EEZFilterCriteria();
			crit.setName(EEZFilterCriteria.RESOURCE_TYPE);
			crit.setOperation(EEZFilterCriteria.NOT_EQUAL);
			crit.setValues(new String[]{""});
			filter.add(crit);
		}
		if ( false ) {
			// request
			EEZRequest request = new EEZRequest(EEZRequest.INCLUDE_NODE, 
				EEZRequest.TYPE_E2E, null, node_rsc.resourceKey());
			EEZRequestList reqList = new EEZRequestList();
			reqList.add(request);
			plugin.invoke(EEZConst.EXECUTE_UNSOLICITED_REQUEST, null, reqList, null);
		}

		LHAMgmtLib.clLog(LHAMgmtLib.LOG_INFO, "----------------------- testing end -----------------------");
	}
}
