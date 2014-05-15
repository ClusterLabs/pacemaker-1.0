pacemaker-1.0
=============

Code for the older 1.0 series of Pacemaker

After lingering on in a zombie like state for a number of years, this codebase is now officially retired.
Since its first release in October 2008, it recieved major updates until March 2010 and bugfixes until November 2013 (largely thanks to the efforts of NTT).
It had a good run but like all good things must come to an end.

Below is a list of known issues likely to be faced by the few remaining refugees that haven't switched to the 1.1 branch since it's introduction in early 2010.
Additional detail on all bugs listed here can be found on http://bugs.clusterlabs.org

Known Issues
============

* 5096	Ensure demote occurs before stop of multistate resource in Master role.
* 5103	About the replacement of the master/slave resource.
* 5211  Probe is not carried out. (When I execute crm_resource -C command.)
* 5095	Unmanaged services should block shutdown unless in maintainence mode
* 5048	The cluster fails in the stop of the node.
* 5120	Unnecessary Master/Slave resource restarts with co-locations
* 5133	The strange behavior of Master/Slave when it failed to demote.
