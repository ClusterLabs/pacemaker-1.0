package org.linuxha.sam.plugin;

import com.ibm.eez.sdk.EEZRelation;
import com.ibm.eez.sdk.EEZResource;
import com.ibm.eez.sdk.EEZResourceToResourceRelation;


public class LHAColocationConstraint extends LHAConstraint {
	private String from;
	private String to;
	private String score;
	
	public final static String SCORE_INFINITY="INFINITY";
	public final static String SCORE_NEGINFINITY="-INFINITY";
	
	public LHAColocationConstraint(String from, String to, String score)
	{
		this.type = LHAConstraint.TYPE_COLOCATION;
		this.from = from;
		this.to = to;
		this.score = score;
	}
	

	public EEZRelation getEEZRelation(LHAClusterManager manager) {
		EEZResource sourceResource = manager.findLHAResource(from);
		EEZResource targetResource = manager.findLHAResource(to);
		
		EEZResourceToResourceRelation rel = new EEZResourceToResourceRelation();
		rel.setSourceKey(sourceResource.resourceKey());
		rel.setTargetKey(targetResource.resourceKey());
		
		if ( score.equals(LHAColocationConstraint.SCORE_INFINITY) ){
			rel.setRelationName(EEZRelation.COLLOCATED);
		} else if (score.equals(LHAColocationConstraint.SCORE_NEGINFINITY)){
			rel.setRelationName(EEZRelation.ANTI_COLLOCATED);
		} else {
			// don't know how to deal with this case, raise an exception.
		}
		return rel;
	}
}