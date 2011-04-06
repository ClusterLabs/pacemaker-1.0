# Copyright (C) 2008 Dejan Muhamedagic <dmuhamedagic@suse.de>
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

import os
import subprocess
import xml.dom.minidom

from userprefs import Options, UserPrefs
from vars import Vars, getuser, gethomedir
from msg import *
from utils import *

def xmlparse(f):
    try:
        doc = xml.dom.minidom.parse(f)
    except xml.parsers.expat.ExpatError,msg:
        common_err("cannot parse xml: %s" % msg)
        return None
    return doc
def file2doc(s):
    try: f = open(s,'r')
    except IOError, msg:
        common_err(msg)
        return None
    doc = xmlparse(f)
    f.close()
    return doc

cib_dump = "cibadmin -Ql"
def cibdump2doc(section = None):
    doc = None
    if section:
        cmd = "%s -o %s" % (cib_dump,section)
    else:
        cmd = cib_dump
    cmd = add_sudo(cmd)
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    try:
        doc = xmlparse(p.stdout)
        p.wait()
    except IOError, msg:
        common_err(msg)
        return None
    return doc
cib_piped = "cibadmin -p"
def commit_rsc(node):
    "Replace a resource definition using cibadmin -R"
    rc = pipe_string("%s -R -o %s" % \
        (cib_piped, "resources"), node.toxml())
    return rc == 0

def get_conf_elem(doc, tag):
    try:
        return doc.getElementsByTagName(tag)[0]
    except:
        return None
def read_cib(fun, params = None):
    doc = fun(params)
    if not doc:
        return doc,None
    cib = doc.childNodes[0]
    if not is_element(cib) or cib.tagName != "cib":
        cib_no_elem_err("cib")
        return doc,None
    return doc,cib

def sanity_check_nvpairs(id,node,attr_list):
    rc = 0
    for nvpair in node.childNodes:
        if not is_element(nvpair) or nvpair.tagName != "nvpair":
            continue
        n = nvpair.getAttribute("name")
        if n and not n in attr_list:
            common_err("%s: attribute %s does not exist" % (id,n))
            rc |= user_prefs.get_check_rc()
    return rc
def sanity_check_meta(id,node,attr_list):
    rc = 0
    if not node or not attr_list:
        return rc
    for c in node.childNodes:
        if not is_element(c):
            continue
        if c.tagName == "meta_attributes":
            rc |= sanity_check_nvpairs(id,c,attr_list)
    return rc
def get_interesting_nodes(node,nodes):
    for c in node.childNodes:
        if is_element(c) and c.tagName in vars.cib_cli_map:
            nodes.append(c)
        get_interesting_nodes(c,nodes)
    return nodes

def resources_xml():
    return cibdump2doc("resources")
def rsc2node(id):
    doc = resources_xml()
    if not doc:
        return None
    nodes = get_interesting_nodes(doc,[])
    for n in nodes:
        if is_resource(n) and n.getAttribute("id") == id:
            return n
def get_meta_param(id,param):
    rsc_meta_show = "crm_resource --meta -r '%s' -g '%s'"
    return get_stdout(rsc_meta_show % (id,param), stderr_on = False)
def listnodes():
    nodes = []
    doc = cibdump2doc("nodes")
    if not doc:
        return []
    nodes_node = get_conf_elem(doc, "nodes")
    if not nodes_node:
        return []
    for c in nodes_node.childNodes:
        if not is_element(c):
            continue
        if c.tagName != "node":
            continue
        if c.getAttribute("type") == 'normal':
            nodes.append(c.getAttribute("uname"))
    return nodes
def is_live_cib():
    '''We working with the live cluster?'''
    return not vars.cib_in_use and not os.getenv("CIB_file")

def cib_shadow_dir():
    if os.getenv("CIB_shadow_dir"):
        return os.getenv("CIB_shadow_dir")
    if getuser() in ("root",vars.crm_daemon_user):
        return vars.crm_conf_dir
    home = gethomedir()
    if home and home.startswith(os.path.sep):
        return os.path.join(home,".cib")
    return os.getenv("TMPDIR") or "/tmp"
def listshadows():
    dir = cib_shadow_dir()
    if os.path.isdir(dir):
        return stdout2list("ls %s | fgrep shadow. | sed 's/^shadow\.//'" % dir)
    else:
        return []
def shadowfile(name):
    return "%s/shadow.%s" % (cib_shadow_dir(), name)
def shadow2doc(name):
    return file2doc(shadowfile(name))

def is_rsc_running(id):
    if not is_live_cib():
        return False
    rsc_node = rsc2node(id)
    if not rsc_node:
        return False
    if not is_resource(rsc_node):
        return False
    rsc_status = "crm_resource -W -r '%s'"
    test_id = rsc_clone(id) or id
    outp = get_stdout(rsc_status % test_id, stderr_on = False)
    return outp.find("running") > 0 and outp.find("NOT") == -1
def is_rsc_clone(rsc_id):
    rsc_node = rsc2node(rsc_id)
    return is_clone(rsc_node)
def is_rsc_ms(rsc_id):
    rsc_node = rsc2node(rsc_id)
    return is_ms(rsc_node)
def rsc_clone(rsc_id):
    '''Get a clone of a resource.'''
    rsc_node = rsc2node(rsc_id)
    if not rsc_node or not rsc_node.parentNode:
        return None
    pnode = rsc_node.parentNode
    if is_group(pnode):
        pnode = pnode.parentNode
    if is_clonems(pnode):
        return pnode.getAttribute("id")
def get_topmost_rsc(node):
    '''
    Return a topmost node which is a resource and contains this resource
    '''
    if is_container(node.parentNode):
        return get_topmost_rsc(node.parentNode)
    return node
def get_cloned_rsc(rsc_id):
    rsc_node = rsc2node(rsc_id)
    if not rsc_node:
        return ""
    for c in rsc_node.childNodes:
        if is_child_rsc(c):
            return c.getAttribute("id")
    return ""
attr_defaults_missing = {
}
def add_missing_attr(node):
    try:
        for defaults in attr_defaults_missing[node.tagName]:
            if not node.hasAttribute(defaults[0]):
                node.setAttribute(defaults[0],defaults[1])
    except: pass
attr_defaults = {
    "rule": (("boolean-op","and"),),
    "expression": (("type","string"),),
}
def drop_attr_defaults(node, ts = 0):
    try:
        for defaults in attr_defaults[node.tagName]:
            if node.getAttribute(defaults[0]) == defaults[1]:
                node.removeAttribute(defaults[0])
    except: pass

def is_element(xmlnode):
    return xmlnode and xmlnode.nodeType == xmlnode.ELEMENT_NODE

def nameandid(xmlnode,level):
    if xmlnode.nodeType == xmlnode.ELEMENT_NODE:
        print level*' ',xmlnode.tagName,xmlnode.getAttribute("id"),xmlnode.getAttribute("name")

def xmltraverse(xmlnode,fun,ts=0):
    for c in xmlnode.childNodes:
        if is_element(c):
            fun(c,ts)
            xmltraverse(c,fun,ts+1)

def xmltraverse_thin(xmlnode,fun,ts=0):
    '''
    Skip elements which may be resources themselves.
    NB: Call this only on resource (or constraint) nodes, but
    never on cib or configuration!
    '''
    for c in xmlnode.childNodes:
        if is_element(c) and not c.tagName in ('primitive','group'):
            xmltraverse_thin(c,fun,ts+1)
    fun(xmlnode,ts)

def xml_processnodes(xmlnode,node_filter,proc):
    '''
    Process with proc all nodes that match filter.
    '''
    node_list = []
    for child in xmlnode.childNodes:
        if node_filter(child):
            node_list.append(child)
        if child.hasChildNodes():
            xml_processnodes(child,node_filter,proc)
    if node_list:
        proc(node_list)

# filter the cib
def is_whitespace(node):
    return node.nodeType == node.TEXT_NODE and not node.data.strip()
def is_comment(node):
    return node.nodeType == node.COMMENT_NODE
def is_status_node(node):
    return is_element(node) and node.tagName == "status"

def is_emptynvpairs(node):
    if is_element(node) and node.tagName in vars.nvpairs_tags:
        for a in vars.precious_attrs:
            if node.getAttribute(a):
                return False
        for n in node.childNodes:
            if is_element(n):
                return False
        return True
    else:
        return False
def is_group(node):
    return is_element(node) \
        and node.tagName == "group"
def is_ms(node):
    return is_element(node) \
        and node.tagName in ("master","ms")
def is_clone(node):
    return is_element(node) \
        and node.tagName == "clone"
def is_clonems(node):
    return is_element(node) \
        and node.tagName in vars.clonems_tags
def is_container(node):
    return is_element(node) \
        and node.tagName in vars.container_tags
def is_primitive(node):
    return is_element(node) \
        and node.tagName == "primitive"
def is_resource(node):
    return is_element(node) \
        and node.tagName in vars.resource_tags
def is_child_rsc(node):
    return is_element(node) \
        and node.tagName in vars.children_tags
def is_constraint(node):
    return is_element(node) \
        and node.tagName in vars.constraint_tags
def is_defaults(node):
    return is_element(node) \
        and node.tagName in vars.defaults_tags
def rsc_constraint(rsc_id,cons_node):
    if not is_element(cons_node):
        return False
    for attr in cons_node.attributes.keys():
        if attr in vars.constraint_rsc_refs \
                and rsc_id == cons_node.getAttribute(attr):
            return True
    for rref in cons_node.getElementsByTagName("resource_ref"):
        if rsc_id == rref.getAttribute("id"):
            return True
    return False

def sort_container_children(node_list):
    '''
    Make sure that attributes's nodes are first, followed by the
    elements (primitive/group). The order of elements is not
    disturbed, they are just shifted to end!
    '''
    for node in node_list:
        children = []
        for c in node.childNodes:
            if is_element(c) and c.tagName in vars.children_tags:
                children.append(c)
        for c in children:
            node.removeChild(c)
        for c in children:
            node.appendChild(c)
def rmnode(node):
    if node and node.parentNode:
        if node.parentNode:
            node.parentNode.removeChild(node)
        node.unlink()
def rmnodes(node_list):
    for node in node_list:
        rmnode(node)
def printid(node_list):
    for node in node_list:
        id = node.getAttribute("id")
        if id: print "node id:",id
def sanitize_cib(doc):
    xml_processnodes(doc,is_status_node,rmnodes)
    #xml_processnodes(doc,is_element,printid)
    xml_processnodes(doc,is_emptynvpairs,rmnodes)
    xml_processnodes(doc,is_whitespace,rmnodes)
    xml_processnodes(doc,is_comment,rmnodes)
    xml_processnodes(doc,is_container,sort_container_children)
    xmltraverse(doc,drop_attr_defaults)

def is_simpleconstraint(node):
    return len(node.getElementsByTagName("resource_ref")) == 0

match_list = {
    "node": ("uname",),
    "crm_config": (),
    "rsc_defaults": (),
    "op_defaults": (),
    "cluster_property_set": (),
    "instance_attributes": (),
    "meta_attributes": (),
    "operations": (),
    "nvpair": ("name",),
    "op": ("name","interval"),
    "rule": ("score","score-attribute","role"),
    "expression": ("attribute","operation","value"),
}
def add_comment(doc,node,s):
    '''
    Add comment s to node from doc.
    '''
    if not s:
        return
    comm_node = doc.createComment(s)
    if node.hasChildNodes():
        node.insertBefore(comm_node, node.firstChild)
    else:
        node.appendChild(comm_node)
def set_id_used_attr(node):
    node.setAttribute("__id_used", "Yes")
def is_id_used_attr(node):
    return node.getAttribute("__id_used") == "Yes"
def remove_id_used_attr(node,lvl):
    if is_element(node) and is_id_used_attr(node):
        node.removeAttribute("__id_used")
def remove_id_used_attributes(node):
    if node:
        xmltraverse(node, remove_id_used_attr)
def lookup_node(node,oldnode,location_only = False):
    '''
    Find a child of oldnode which matches node.
    This is used to "harvest" existing ids in order to prevent
    irrelevant changes to the XML code.
    The list of attributes to match is in the dictionary
    match_list.
    The "id" attribute is treated differently. In case the new node
    (the first parameter here) contains the id, then the "id"
    attribute is added to the match list.
    '''
    #print "lookup:",node.tagName,node.getAttribute("id")
    if not oldnode:
        return None
    #print "  in:",oldnode.tagName,oldnode.getAttribute("id")
    try:
        attr_list = list(match_list[node.tagName])
    except KeyError:
        attr_list = []
    if node.getAttribute("id"):
        #print "  add id attribute"
        attr_list.append("id")
    for c in oldnode.childNodes:
        if not is_element(c):
            continue
        if not location_only and is_id_used_attr(c):
            continue
        #print "  checking:",c.tagName,c.getAttribute("id")
        if node.tagName == c.tagName:
            failed = False
            for a in attr_list:
                if node.getAttribute(a) != c.getAttribute(a):
                    failed = True
                    break
            if not failed:
                #print "  found:",c.tagName,c.getAttribute("id")
                return c
    return None

def find_operation(rsc_node,name,interval):
    op_node_l = rsc_node.getElementsByTagName("operations")
    for ops in op_node_l:
        for c in ops.childNodes:
            if not is_element(c):
                continue
            if c.tagName != "op":
                continue
            if c.getAttribute("name") == name \
                    and c.getAttribute("interval") == interval:
                return c

def filter_on_tag(nl,tag):
    return [node for node in nl if node.tagName == tag]
def nodes(node_list):
    return filter_on_tag(node_list,"node")
def primitives(node_list):
    return filter_on_tag(node_list,"primitive")
def groups(node_list):
    return filter_on_tag(node_list,"group")
def clones(node_list):
    return filter_on_tag(node_list,"clone")
def mss(node_list):
    return filter_on_tag(node_list,"master")
def constraints(node_list):
    return filter_on_tag(node_list,"rsc_location") \
        + filter_on_tag(node_list,"rsc_colocation") \
        + filter_on_tag(node_list,"rsc_order")
def properties(node_list):
    return filter_on_tag(node_list,"cluster_property_set") \
        + filter_on_tag(node_list,"rsc_defaults") \
        + filter_on_tag(node_list,"op_defaults")
def processing_sort(nl):
    '''
    It's usually important to process cib objects in this order,
    i.e. simple objects first.
    '''
    return nodes(nl) + primitives(nl) + groups(nl) + mss(nl) + clones(nl) \
        + constraints(nl) + properties(nl)

def obj_cmp(obj1,obj2):
    return cmp(obj1.obj_id,obj2.obj_id)
def filter_on_type(cl,obj_type):
    if type(cl[0]) == type([]):
        l = [cli_list for cli_list in cl if cli_list[0][0] == obj_type]
        if user_prefs.get_sort_elems():
            l.sort(cmp = cmp)
    else:
        l = [obj for obj in cl if obj.obj_type == obj_type]
        if user_prefs.get_sort_elems():
            l.sort(cmp = obj_cmp)
    return l
def nodes_cli(cl):
    return filter_on_type(cl,"node")
def primitives_cli(cl):
    return filter_on_type(cl,"primitive")
def groups_cli(cl):
    return filter_on_type(cl,"group")
def clones_cli(cl):
    return filter_on_type(cl,"clone")
def mss_cli(cl):
    return filter_on_type(cl,"ms") + filter_on_type(cl,"master")
def constraints_cli(node_list):
    return filter_on_type(node_list,"location") \
        + filter_on_type(node_list,"colocation") \
        + filter_on_type(node_list,"collocation") \
        + filter_on_type(node_list,"order")
def properties_cli(cl):
    return filter_on_type(cl,"property") \
        + filter_on_type(cl,"rsc_defaults") \
        + filter_on_type(cl,"op_defaults")
def ops_cli(cl):
    return filter_on_type(cl,"op")
def processing_sort_cli(cl):
    '''
    Return the given list in this order:
    nodes, primitives, groups, ms, clones, constraints, rest
    Both a list of objects (CibObject) and list of cli
    representations accepted.
    '''
    return nodes_cli(cl) + primitives_cli(cl) + groups_cli(cl) + mss_cli(cl) + clones_cli(cl) \
        + constraints_cli(cl) + properties_cli(cl) + ops_cli(cl)

def is_resource_cli(s):
    return s in olist(vars.resource_cli_names)
def is_constraint_cli(s):
    return s in olist(vars.constraint_cli_names)

def referenced_resources(node):
    if not is_constraint(node):
        return []
    xml_obj_type = node.tagName
    if xml_obj_type == "rsc_location":
        node_list = node.getElementsByTagName("rsc")
    elif node.getElementsByTagName("resource_ref"): # resource sets
        node_list = node.getElementsByTagName("resource_ref")
    elif xml_obj_type == "rsc_colocation":
        node_list = node.getElementsByTagName("rsc") + \
            node.getElementsByTagName("with-rsc")
    elif xml_obj_type == "rsc_order":
        node_list = node.getElementsByTagName("first") + \
            node.getElementsByTagName("then")
    return [x.getAttribute("id") for x in node_list]

def rename_id(node,old_id,new_id):
    if node.getAttribute("id") == old_id:
        node.setAttribute("id", new_id)
def rename_rscref_simple(c_obj,old_id,new_id):
    c_modified = False
    for attr in c_obj.node.attributes.keys():
        if attr in vars.constraint_rsc_refs and \
                c_obj.node.getAttribute(attr) == old_id:
            c_obj.node.setAttribute(attr, new_id)
            c_obj.updated = True
            c_modified = True
    return c_modified
def delete_rscref_simple(c_obj,rsc_id):
    c_modified = False
    for attr in c_obj.node.attributes.keys():
        if attr in vars.constraint_rsc_refs and \
                c_obj.node.getAttribute(attr) == rsc_id:
            c_obj.node.removeAttribute(attr)
            c_obj.updated = True
            c_modified = True
    return c_modified
def rset_uniq(c_obj,d):
    '''
    Drop duplicate resource references.
    '''
    l = []
    for rref in c_obj.node.getElementsByTagName("resource_ref"):
        rsc_id = rref.getAttribute("id")
        if d[rsc_id] > 1: # drop one
            l.append(rref)
            d[rsc_id] -= 1
    rmnodes(l)
def delete_rscref_rset(c_obj,rsc_id):
    '''
    Drop all reference to rsc_id.
    '''
    c_modified = False
    l = []
    for rref in c_obj.node.getElementsByTagName("resource_ref"):
        if rsc_id == rref.getAttribute("id"):
            l.append(rref)
            c_obj.updated = True
            c_modified = True
    rmnodes(l)
    l = []
    for rset in c_obj.node.getElementsByTagName("resource_set"):
        if len(rset.getElementsByTagName("resource_ref")) == 0:
            l.append(rset)
            c_obj.updated = True
            c_modified = True
    rmnodes(l)
    return c_modified
def rset_convert(c_obj):
    l = c_obj.node.getElementsByTagName("resource_ref")
    if len(l) != 2:
        return # eh?
    c_obj.modified = True
    cli = c_obj.repr_cli(format = -1)
    newnode = c_obj.cli2node(cli)
    if newnode:
        c_obj.node.parentNode.replaceChild(newnode,c_obj.node)
        c_obj.node.unlink()
def rename_rscref_rset(c_obj,old_id,new_id):
    c_modified = False
    d = {}
    for rref in c_obj.node.getElementsByTagName("resource_ref"):
        rsc_id = rref.getAttribute("id")
        if rsc_id == old_id:
            rref.setAttribute("id", new_id)
            rsc_id = new_id
            c_obj.updated = True
            c_modified = True
        if not rsc_id in d:
            d[rsc_id] = 0
        else: 
            d[rsc_id] += 1
    rset_uniq(c_obj,d)
    # if only two resource references remained then, to preserve
    # sanity, convert it to a simple constraint (sigh)
    cnt = 0
    for key in d:
        cnt += d[key]
    if cnt == 2:
        rset_convert(c_obj)
    return c_modified
def rename_rscref(c_obj,old_id,new_id):
    if rename_rscref_simple(c_obj,old_id,new_id) or \
            rename_rscref_rset(c_obj,old_id,new_id):
        err_buf.info("resource references in %s updated" % c_obj.obj_string())
def delete_rscref(c_obj,rsc_id):
    return delete_rscref_simple(c_obj,rsc_id) or \
        delete_rscref_rset(c_obj,rsc_id)
def silly_constraint(c_node,rsc_id):
    '''
    Remove a constraint from rsc_id to rsc_id.
    Or an invalid one.
    '''
    if c_node.getElementsByTagName("resource_ref"):
        # it's a resource set
        # the resource sets have already been uniq-ed
        return len(c_node.getElementsByTagName("resource_ref")) <= 1
    cnt = 0  # total count of referenced resources have to be at least two
    rsc_cnt = 0
    for attr in c_node.attributes.keys():
        if attr in vars.constraint_rsc_refs:
            cnt += 1
            if c_node.getAttribute(attr) == rsc_id:
                rsc_cnt += 1
    if c_node.tagName == "rsc_location":  # locations are never silly
        return cnt < 1
    else:
        return rsc_cnt == 2 or cnt < 2

def get_rsc_children_ids(node):
    return [x.getAttribute("id") \
        for x in node.childNodes if is_child_rsc(x)]
def get_rscop_defaults_meta_node(node):
    for c in node.childNodes:
        if not is_element(c) or c.tagName != "meta_attributes":
            continue
        return c
    return None

def new_cib():
    doc = xml.dom.minidom.Document()
    cib = doc.createElement("cib")
    doc.appendChild(cib)
    configuration = doc.createElement("configuration")
    cib.appendChild(configuration)
    for name in vars.req_sections:
        node = doc.createElement(name)
        configuration.appendChild(node)
    return doc
def get_topnode(doc, tag):
    "Get configuration element or create/append if there's none."
    try:
        e = doc.getElementsByTagName(tag)[0]
    except:
        e = doc.createElement(tag)
        conf = doc.getElementsByTagName("configuration")[0]
        if conf:
            conf.appendChild(e)
        else:
            return None
    return e
def new_cib_element(node,tagname,id_pfx):
    base_id = node.getAttribute("id")
    newnode = node.ownerDocument.createElement(tagname)
    newnode.setAttribute("id", "%s-%s" % (base_id,id_pfx))
    node.appendChild(newnode)
    return newnode
def get_attr_in_set(node,attr):
    for c in node.childNodes:
        if not is_element(c):
            continue
        if c.tagName == "nvpair" and c.getAttribute("name") == attr:
            return c
    return None
def set_attr(node,attr,value):
    '''
    Set an attribute in the attribute set.
    '''
    nvpair = get_attr_in_set(node,attr)
    if not nvpair:
        nvpair = new_cib_element(node,"nvpair",attr)
    nvpair.setAttribute("name",attr)
    nvpair.setAttribute("value",value)
def get_set_nodes(node,setname,create = 0):
    'Return the attributes set nodes (create one if requested)'
    l = []
    for c in node.childNodes:
        if not is_element(c):
            continue
        if c.tagName == setname:
            l.append(c)
    if l:
        return l
    if create:
        l.append(new_cib_element(node,setname,setname))
    return l

def xml_cmp(n, m, show = False):
    rc = hash(n.toxml()) == hash(m.toxml())
    if not rc and show and user_prefs.get_debug():
        print "original:",n.toprettyxml()
        print "processed:",m.toprettyxml()
    return hash(n.toxml()) == hash(m.toxml())

def merge_nvpairs(dnode,snode):
    rc = False
    add_children = []
    for c in snode.childNodes:
        if not is_element(c):
            continue
        if c.tagName == "nvpair":
            dc = lookup_node(c,dnode)
            if dc:
                dc.setAttribute("value",c.getAttribute("value"))
            else:
                add_children.append(c)
            rc = True
    for c in add_children:
        dnode.appendChild(c)
    return rc

def merge_nodes(dnode,snode):
    '''
    Import elements from snode into dnode.
    If an element is attributes set (vars.nvpairs_tags), then
    merge nvpairs by the name attribute.
    Otherwise, replace the whole element. (TBD)
    '''
    #print "1:",dnode.toprettyxml()
    #print "2:",snode.toprettyxml()
    #vars.nvpairs_tags
    rc = False # any changes done?
    if not dnode or not snode:
        return rc
    for c in snode.childNodes:
        dc = lookup_node(c,dnode)
        if not dc:
            if c.tagName in vars.nvpairs_tags:
                dnode.appendChild(c)
                rc = True
            continue
        if dc.tagName in vars.nvpairs_tags:
            rc = rc or merge_nvpairs(dc,c)
    return rc

user_prefs = UserPrefs.getInstance()
vars = Vars.getInstance()
# vim:ts=4:sw=4:et:
