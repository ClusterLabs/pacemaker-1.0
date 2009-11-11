#!/usr/bin/env python

 # Copyright (C) 2008,2009 Dejan Muhamedagic <dmuhamedagic@suse.de>
 # 
 # This program is free software; you can redistribute it and/or
 # modify it under the terms of the GNU General Public
 # License as published by the Free Software Foundation; either
 # version 2.1 of the License, or (at your option) any later version.
 # 
 # This software is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # General Public License for more details.
 # 
 # You should have received a copy of the GNU General Public
 # License along with this library; if not, write to the Free Software
 # Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 #

import os,sys
import getopt
import re
import xml.dom.minidom

def usage():
    print >> sys.stderr, "usage: %s [-T] [-c ha_cf] {set_property <name> <value>|analyze_cib|convert_cib|manage_ocfs2 {start|stop}|print_ocfs2_devs}"%sys.argv[0]
    sys.exit(1)

TEST = False
try:
    optlist, arglist = getopt.getopt(sys.argv[1:], "hTc:")
except getopt.GetoptError:
    usage()
for opt,arg in optlist:
    if opt == '-h':
        usage()
    elif opt == '-c':
        HA_CF = arg
    elif opt == '-T':
        TEST = True
if len(arglist) < 1:
    usage()

def load_cib():
    doc = xml.dom.minidom.parse(sys.stdin)
    return doc
def is_whitespace(node):
    return node.nodeType == node.TEXT_NODE and not node.data.strip()
def rmnodes(node_list):
    for node in node_list:
        node.parentNode.removeChild(node)
        node.unlink()
def get_children_cnt(node,taglist):
    cnt = 0
    for c in node.childNodes:
        if is_element(c) and c.tagName in taglist:
            cnt += 1
    return cnt
def is_empty_group(node):
    return is_element(node) and node.tagName == "group" and \
        get_children_cnt(node,["primitive"]) == 0
def is_empty_clone(node):
    return is_element(node) and node.tagName == "clone" and \
        get_children_cnt(node,["primitive","group"]) == 0
def is_constraint(node):
    return is_element(node) and node.tagName in cons_tags
def is_id_node(node):
    return is_element(node) and node.getAttribute("id")
def is_badid(id):
    return re.match("^[^a-zA-Z_]",id)
def fix_id(node):
    newid = "A"+node.getAttribute("id")
    node.setAttribute("id",newid)
    return newid
rename_ids = {}  # global used in rename_refs
def fix_ids(node_list):
    for node in node_list:
        oldid = node.getAttribute("id")
        if is_badid(oldid):
            newid = fix_id(node)
            rename_ids[oldid] = newid
def rename_refs(node_list):
    for node in node_list:
        for attr in cons_idattr_list[node.tagName]:
            ref = node.getAttribute(attr)
            if ref in rename_ids.keys():
                node.setAttribute(attr,rename_ids[ref])
def set_id2uname(node_list):
    for node in node_list:
        if node.tagName != "node":
            continue
        id = node.getAttribute("id")
        uname = node.getAttribute("uname")
        if uname:
            node.setAttribute("id",uname)
        else:
            print >> sys.stderr, "WARNING: node %s has no uname attribute" % id
def is_element(xmlnode):
    return xmlnode.nodeType == xmlnode.ELEMENT_NODE
def xml_processnodes(xmlnode,filter,proc):
    '''
    Process with proc all nodes that match filter.
    '''
    node_list = []
    for child in xmlnode.childNodes:
        if filter(child):
            node_list.append(child)
        if child.hasChildNodes():
            xml_processnodes(child,filter,proc)
    if node_list:
        proc(node_list)
def skip_first(s):
    l = s.split('\n')
    return '\n'.join(l[1:])
def get_attribute(tag,node,p):
    attr_set = node.getElementsByTagName(tag)
    if not attr_set:
        return ''
    attributes = attr_set[0].getElementsByTagName("attributes")
    if not attributes:
        return ''
    attributes = attributes[0]
    for nvpair in attributes.getElementsByTagName("nvpair"):
        if p == nvpair.getAttribute("name"):
            return nvpair.getAttribute("value")
    return ''
def get_param(node,p):
    return get_attribute("instance_attributes",node,p)
def mknvpair(id,name,value):
    nvpair = doc.createElement("nvpair")
    nvpair.setAttribute("id",id + "-" + name)
    nvpair.setAttribute("name",name)
    nvpair.setAttribute("value",value)
    return nvpair
def set_attribute(tag,node,p,value,overwrite = True):
    attr_set = node.getElementsByTagName(tag)
    if not attr_set:
        return ["",False]
    set_id = attr_set[0].getAttribute("id")
    attributes = attr_set[0].getElementsByTagName("attributes")
    if not attributes:
        attributes = doc.createElement("attributes")
        attr_set[0].appendChild(attributes)
    else:
        attributes = attributes[0]
    for nvp in attributes.getElementsByTagName("nvpair"):
        if p == nvp.getAttribute("name"):
            if overwrite:
                nvp.setAttribute("value",value)
            return [nvp.getAttribute("value"),overwrite]
    attributes.appendChild(mknvpair(set_id,p,value))
    return [value,True]

doc = load_cib()
xml_processnodes(doc,is_whitespace,rmnodes)
resources = doc.getElementsByTagName("resources")[0]
constraints = doc.getElementsByTagName("constraints")[0]
nodes = doc.getElementsByTagName("nodes")[0]
crm_config = doc.getElementsByTagName("crm_config")[0]
rsc_tags = ("primitive","group","clone","ms")
cons_tags = ("rsc_location","rsc_order","rsc_colocation")
cons_idattr_list = {
    "rsc_location": ("rsc",),
    "rsc_order": ("from","to"),
    "rsc_colocation": ("from","to"),
}
if not resources:
    print >> sys.stderr, "ERROR: sorry, no resources section in the CIB, cannot proceed"
    sys.exit(1)
if not constraints:
    print >> sys.stderr, "ERROR: sorry, no constraints section in the CIB, cannot proceed"
    sys.exit(1)
if not nodes:
    print >> sys.stderr, "ERROR: sorry, no nodes section in the CIB, cannot proceed"
    sys.exit(1)

if arglist[0] == "set_node_ids":
    xml_processnodes(nodes,lambda x:1,set_id2uname)
    s = skip_first(doc.toprettyxml())
    print s
    sys.exit(0)

if arglist[0] == "set_property":
    overwrite = False
    if len(arglist) == 4:
        if arglist[3] == "overwrite":
            overwrite = True
    elif len(arglist) != 3:
        usage()
    p = arglist[1]
    value = arglist[2]
    set_value,rc = set_attribute("cluster_property_set", \
        crm_config,p,value,overwrite)
    if not set_value and not rc:
        print >> sys.stderr, \
            "WARNING: cluster_property_set not found"
    elif not rc:
        print >> sys.stderr, \
            "INFO: cluster property %s is set to %s and NOT overwritten to %s" % (p,set_value,value)
    else:
        print >> sys.stderr, \
            "INFO: cluster property %s set to %s" % (p,set_value)
    s = skip_first(doc.toprettyxml())
    print s
    sys.exit(0)

if arglist[0] == "analyze_cib":
    rc = 0
    for rsc in doc.getElementsByTagName("primitive"):
        rsc_type = rsc.getAttribute("type")
        if rsc_type == "EvmsSCC":
            print >> sys.stderr, "INFO: evms configuration found; conversion required"
            rc = 1
        elif rsc_type == "Filesystem":
            if get_param(rsc,"fstype") == "ocfs2":
                print >> sys.stderr, "INFO: ocfs2 configuration found; conversion required"
                rc = 1
    sys.exit(rc)

if arglist[0] == "print_ocfs2_devs":
    for rsc in doc.getElementsByTagName("primitive"):
        if rsc.getAttribute("type") == "Filesystem":
            if get_param(rsc,"fstype") == "ocfs2":
                print get_param(rsc,"device")
    sys.exit(0)

def rm_attribute(tag,node,p):
    attr_set = node.getElementsByTagName(tag)
    if not attr_set:
        return ''
    attributes = attr_set[0].getElementsByTagName("attributes")
    if not attributes:
        return ''
    attributes = attributes[0]
    for nvpair in attributes.getElementsByTagName("nvpair"):
        if p == nvpair.getAttribute("name"):
            nvpair.parentNode.removeChild(nvpair)
def set_param(node,p,value):
    set_attribute("instance_attributes",node,p,value)
def rm_param(node,p):
    rm_attribute("instance_attributes",node,p)
def evms2lvm(node,a):
    v = node.getAttribute(a)
    if v:
        v = v.replace("EVMS","LVM")
        v = v.replace("Evms","LVM")
        v = v.replace("evms","lvm")
        node.setAttribute(a,v)
def replace_evms_strings(node_list):
    for node in node_list:
        evms2lvm(node,"id")
        if node.tagName in ("rsc_colocation","rsc_order"):
            evms2lvm(node,"to")
            evms2lvm(node,"from")

def get_input(msg):
    if TEST:
        print >> sys.stderr, "%s: setting to /dev/null" % msg
        return "/dev/null"
    while True:
        ans = raw_input(msg)
        if ans:
            if os.access(ans,os.F_OK):
                return ans
            else:
                print >> sys.stderr, "Cannot read %s" % ans
        print >> sys.stderr, "We do need this input to continue."
def mk_lvm(rsc_id,volgrp):
    print >> sys.stderr, \
        "INFO: creating LVM resource %s for vg %s" % (rsc_id,volgrp)
    node = doc.createElement("primitive")
    node.setAttribute("id",rsc_id)
    node.setAttribute("type","LVM")
    node.setAttribute("provider","heartbeat")
    node.setAttribute("class","ocf")
    operations = doc.createElement("operations")
    node.appendChild(operations)
    mon_op = doc.createElement("op")
    operations.appendChild(mon_op)
    mon_op.setAttribute("id", rsc_id + "_mon")
    mon_op.setAttribute("name","monitor")
    interval = "120s"
    timeout = "60s"
    mon_op.setAttribute("interval", interval)
    mon_op.setAttribute("timeout", timeout)
    instance_attributes = doc.createElement("instance_attributes")
    instance_attributes.setAttribute("id", rsc_id + "_inst_attr")
    node.appendChild(instance_attributes)
    attributes = doc.createElement("attributes")
    instance_attributes.appendChild(attributes)
    attributes.appendChild(mknvpair(rsc_id,"volgrpname",volgrp))
    return node
def mk_clone(id,ra_type,ra_class,prov):
    c = doc.createElement("clone")
    c.setAttribute("id",id + "-clone")
    meta = doc.createElement("meta_attributes")
    c.appendChild(meta)
    meta.setAttribute("id",id + "_meta")
    attributes = doc.createElement("attributes")
    meta.appendChild(attributes)
    attributes.appendChild(mknvpair(id,"globally-unique","false"))
    attributes.appendChild(mknvpair(id,"interleave","true"))
    p = doc.createElement("primitive")
    c.appendChild(p)
    p.setAttribute("id",id)
    p.setAttribute("type",ra_type)
    if prov:
        p.setAttribute("provider",prov)
    p.setAttribute("class",ra_class)
    operations = doc.createElement("operations")
    p.appendChild(operations)
    mon_op = doc.createElement("op")
    operations.appendChild(mon_op)
    mon_op.setAttribute("id", id + "_mon")
    mon_op.setAttribute("name","monitor")
    interval = "60s"
    timeout = "30s"
    mon_op.setAttribute("interval", interval)
    mon_op.setAttribute("timeout", timeout)
    return c
def add_ocfs_clones():
    c1 = mk_clone("o2cb","o2cb","ocf","ocfs2")
    c2 = mk_clone("dlm","controld","ocf","pacemaker")
    print >> sys.stderr, \
        "INFO: adding clones o2cb-clone and dlm-clone"
    resources.appendChild(c1)
    resources.appendChild(c2)
    c1 = mk_order("dlm-clone","o2cb-clone")
    c2 = mk_colocation("dlm-clone","o2cb-clone")
    constraints.appendChild(c1)
    constraints.appendChild(c2)
def mk_order(r1,r2):
    rsc_order = doc.createElement("rsc_order")
    rsc_order.setAttribute("id","rsc_order_"+r1+"_"+r2)
    rsc_order.setAttribute("from",r1)
    rsc_order.setAttribute("to",r2)
    rsc_order.setAttribute("type","before")
    rsc_order.setAttribute("score","INFINITY")
    rsc_order.setAttribute("symmetrical","true")
    return rsc_order
def mk_colocation(r1,r2):
    rsc_colocation = doc.createElement("rsc_colocation")
    rsc_colocation.setAttribute("id","rsc_colocation_"+r1+"_"+r2)
    rsc_colocation.setAttribute("from",r1)
    rsc_colocation.setAttribute("to",r2)
    rsc_colocation.setAttribute("score","INFINITY")
    return rsc_colocation
def add_ocfs_constraints(rsc):
    node = rsc.parentNode
    if node.tagName != "clone":
        node = rsc
    rsc_id = node.getAttribute("id")
    print >> sys.stderr, \
        "INFO: adding constraints for o2cb-clone and %s" % rsc_id
    c1 = mk_order("o2cb-clone",rsc_id)
    c2 = mk_colocation("o2cb-clone",rsc_id)
    constraints.appendChild(c1)
    constraints.appendChild(c2)
def add_lvm_constraints(lvm_id,rsc):
    node = rsc.parentNode
    if node.tagName != "clone":
        node = rsc
    rsc_id = node.getAttribute("id")
    print >> sys.stderr, \
        "INFO: adding constraints for %s and %s" % (lvm_id,rsc_id)
    c1 = mk_order(lvm_id,rsc_id)
    c2 = mk_colocation(lvm_id,rsc_id)
    constraints.appendChild(c1)
    constraints.appendChild(c2)
def change_ocfs2_device(rsc):
    print >> sys.stderr, "The current device for ocfs2 depends on evms: %s"%get_param(rsc,"device")
    dev = get_input("Please supply the device where %s ocfs2 resource resides: "%rsc.getAttribute("id"))
    set_param(rsc,"device",dev)
def set_target_role(rsc,target_role):
    node = rsc.parentNode
    if node.tagName != "clone":
        node = rsc
    id = node.getAttribute("id")
    l = rsc.getElementsByTagName("meta_attributes")
    if l:
        meta = l[0]
    else:
        meta = doc.createElement("meta_attributes")
        meta.setAttribute("id",id + "_meta")
        node.appendChild(meta)
        attributes = doc.createElement("attributes")
        meta.appendChild(attributes)
    rm_param(rsc,"target_role")
    set_attribute("meta_attributes",node,"target_role",target_role)
def start_ocfs2(node_list):
    for node in node_list:
        set_target_role(node,"Started")
def stop_ocfs2(node_list):
    for node in node_list:
        set_target_role(node,"Stopped")
def is_ocfs2_fs(node):
    return node.tagName == "primitive" and \
        node.getAttribute("type") == "Filesystem" and \
        get_param(node,"fstype") == "ocfs2"
def new_pingd_rsc(options,host_list):
    rsc_id = "pingd"
    c = mk_clone(rsc_id,"pingd","ocf","pacemaker")
    node = c.getElementsByTagName("primitive")[0]
    instance_attributes = doc.createElement("instance_attributes")
    instance_attributes.setAttribute("id", rsc_id + "_inst_attr")
    node.appendChild(instance_attributes)
    attributes = doc.createElement("attributes")
    instance_attributes.appendChild(attributes)
    if options:
        attributes.appendChild(mknvpair(rsc_id,"options",options))
    set_param(node,"host_list",host_list)
    return c
def new_cloned_rsc(rsc_class,rsc_provider,rsc_type):
    return mk_clone(rsc_type,rsc_type,rsc_class,rsc_provider)
def find_respawn(prog):
    rc = False
    f = open(HA_CF or "/etc/ha.d/ha.cf", 'r')
    for l in f:
        s = l.split()
        if not s:
            continue
        if s[0] == "respawn" and s[2].find(prog) > 0:
            rc = True
            break
    f.close()
    return rc
def parse_pingd_respawn():
    f = open(HA_CF or "/etc/ha.d/ha.cf", 'r')
    opts = ''
    ping_list = []
    for l in f:
        s = l.split()
        if not s:
            continue
        if s[0] == "respawn" and s[2].find("pingd") > 0:
            opts = ' '.join(s[3:])
        elif s[0] == "ping":
            ping_list.append(s[1])
    f.close()
    return opts,' '.join(ping_list)

class NewLVMfromEVMS2(object):
    def __init__(self):
        self.vgdict = {}
    def add_rsc(self,rsc,vg):
        if vg not in self.vgdict:
            self.vgdict[vg] = []
        self.vgdict[vg].append(rsc)
    def edit_attr(self,rsc,rsc_id,nvpair,vg,lv):
        v = "/dev/%s/%s" % (vg,lv)
        attr = nvpair.getAttribute("name")
        nvpair.setAttribute("value",v)
        print >> sys.stderr, \
            "INFO: set resource %s attribute %s to %s"%(rsc_id,attr,v)
    def proc_attr(self,rsc,rsc_id,nvpair):
        v = nvpair.getAttribute("value")
        path_elems = v.split("/")
        if v.startswith("/dev/evms/"):
            if v.find("/lvm2/") and len(path_elems) == 7:
                vg = path_elems[5]
                lv = path_elems[6]
                self.add_rsc(rsc,vg)
                self.edit_attr(rsc,rsc_id,nvpair,vg,lv)
            else:
                print >> sys.stderr, \
                    "ERROR: resource %s attribute %s=%s obviously" % \
                        (rsc_id,nvpair.getAttribute("name"),v)
                print >> sys.stderr, \
                    "ERROR: references an EVMS volume, but I don't know what to do about it"
                print >> sys.stderr, \
                    "ERROR: Please fix it on SLES10 (see README.hb2openais for more details)"
                sys.exit(1)
    def check_rsc(self,rsc,rsc_id):
        for inst_attr in rsc.getElementsByTagName("instance_attributes"):
            for nvpair in inst_attr.getElementsByTagName("nvpair"):
                self.proc_attr(rsc,rsc_id,nvpair)
    def mklvms(self):
        for vg in self.vgdict.keys():
            node = mk_lvm("LVM"+vg,vg)
            resources.appendChild(node)
            lvm_id = node.getAttribute("id")
            for rsc in self.vgdict[vg]:
                add_lvm_constraints(lvm_id,rsc)

def get_rsc_id_list(doc):
    l = []
    for tag in rsc_tags:
        for node in doc.getElementsByTagName(tag):
            l.append(node.getAttribute("id"))
    return l
def drop_degenerate_constraints(doc):
    degenerates = []
    rsc_id_list = get_rsc_id_list(doc)
    # 1. referenced resources don't exist
    for tag in cons_tags:
        for node in doc.getElementsByTagName(tag):
            for attr in cons_idattr_list[tag]:
                if node.getAttribute(attr) not in rsc_id_list:
                    degenerates.append(node)
                    break
    # 2. rules in rsc_location empty
    for node in doc.getElementsByTagName("rsc_location"):
        for rule in node.childNodes:
            if not is_element(rule) or rule.tagName != "rule":
                continue
            if get_children_cnt(rule,["expression"]) == 0:
                degenerates.append(node)
                break
    rmnodes(degenerates)

def process_evmsd(rsc,rsc_id):
    print >> sys.stderr, "INFO: Evmsd resource %s will change type to clvmd"%rsc_id
    rsc.setAttribute("type","clvmd")
    rsc.setAttribute("provider","lvm2")
    add_ocfs_constraints(rsc)
def process_evmsSCC(rsc,rsc_id):
    print >> sys.stderr, "INFO: EvmsSCC resource is going to be removed"
    parent = rsc.parentNode
    parent.removeChild(rsc)
    rsc.unlink()
def process_cib():
    ocfs_clones = []
    evms_present = False
    lvm_evms = NewLVMfromEVMS2()

    for rsc in doc.getElementsByTagName("primitive"):
        rsc_id = rsc.getAttribute("id")
        rsc_type = rsc.getAttribute("type")
        lvm_evms.check_rsc(rsc,rsc_id)
        if rsc_type == "Evmsd":
            process_evmsd(rsc,rsc_id)
        elif rsc_type == "EvmsSCC":
            evms_present = True
            process_evmsSCC(rsc,rsc_id)
        elif rsc_type == "Filesystem":
            if get_param(rsc,"fstype") == "ocfs2":
                ocfs_clones.append(rsc)
                id = rsc.getAttribute("id")
                add_ocfs_constraints(rsc)
    lvm_evms.mklvms()
    if ocfs_clones:
        add_ocfs_clones()
    if evms_present:
        xml_processnodes(doc,lambda x:1,replace_evms_strings)
    # drop degenerate groups/clones
    xml_processnodes(doc,is_empty_group,rmnodes)
    xml_processnodes(doc,is_empty_clone,rmnodes)
    drop_degenerate_constraints(doc)
    #xml_processnodes(doc,is_id_node,fix_ids)
    #xml_processnodes(doc,is_constraint,rename_refs)

if arglist[0] == "convert_cib":
    opts,pingd_host_list = parse_pingd_respawn()
    if pingd_host_list:
        clone = new_pingd_rsc(opts,pingd_host_list)
        resources.appendChild(clone)
    if find_respawn("evmsd"):
        resources.appendChild(new_cloned_rsc("ocf","lvm2","clvmd"))
    process_cib()
    s = skip_first(doc.toprettyxml())
    print s
    sys.exit(0)

if arglist[0] == "manage_ocfs2":
    if len(arglist) != 2:
        usage()
    if arglist[1] == "stop":
        xml_processnodes(doc,is_ocfs2_fs,stop_ocfs2)
    elif arglist[1] == "start":
        xml_processnodes(doc,is_ocfs2_fs,start_ocfs2)
    s = skip_first(doc.toprettyxml())
    print s
    sys.exit(0)

# shouldn't get here
usage()

# vim:ts=4:sw=4:et:
