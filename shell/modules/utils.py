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
import pwd
from tempfile import mkstemp
import subprocess
import re
import glob
import time

from userprefs import Options, UserPrefs
from vars import Vars
from msg import *

def is_program(prog):
    return subprocess.call("which %s >/dev/null 2>&1"%prog, shell=True) == 0

def ask(msg):
    # if there's no terminal, no use asking and default to "no"
    if not sys.stdin.isatty():
        return False
    print_msg = True
    while True:
        try:
            ans = raw_input(msg + ' ')
        except EOFError:
            ans = 'n'
        if not ans or ans[0].lower() not in ('n','y'):
            if print_msg:
                print "Please answer with y[es] or n[o]"
                print_msg = False
        else:
            return ans[0].lower() == 'y'

def verify_boolean(opt):
    return opt.lower() in ("yes","true","on") or \
        opt.lower() in ("no","false","off")

def keyword_cmp(string1, string2):
    return string1.lower() == string2.lower()

from UserDict import DictMixin
class odict(DictMixin):
    def __init__(self, data=None, **kwdata):
        self._keys = []
        self._data = {}
    def __setitem__(self, key, value):
        if key not in self._data:
            self._keys.append(key)
        self._data[key] = value
    def __getitem__(self, key):
        if key not in self._data:
            return self._data[key.lower()]
        return self._data[key]
    def __delitem__(self, key):
        del self._data[key]
        self._keys.remove(key)
    def keys(self):
        return list(self._keys)
    def copy(self):
        copyDict = odict()
        copyDict._data = self._data.copy()
        copyDict._keys = self._keys[:]
        return copyDict

class olist(list):
    def __init__(self, keys):
        #print "Init %s" % (repr(keys))
        super(olist, self).__init__()
        for key in keys:
            self.append(key)
            self.append(key.upper())

def setup_aliases(obj):
    for cmd in obj.cmd_aliases.keys():
        for alias in obj.cmd_aliases[cmd]:
            if obj.help_table:
                obj.help_table[alias] = obj.help_table[cmd]
            obj.cmd_table[alias] = obj.cmd_table[cmd]

def os_types_list(path):
    l = []
    for f in glob.glob(path):
        if os.access(f,os.X_OK) and os.path.isfile(f):
            a = f.split("/")
            l.append(a[-1])
    return l

def listtemplates():
    l = []
    for f in os.listdir(vars.tmpl_dir):
        if os.path.isfile("%s/%s" % (vars.tmpl_dir,f)):
            l.append(f)
    return l
def listconfigs():
    l = []
    for f in os.listdir(vars.tmpl_conf_dir):
        if os.path.isfile("%s/%s" % (vars.tmpl_conf_dir,f)):
            l.append(f)
    return l

def add_sudo(cmd):
    if user_prefs.crm_user:
        return "sudo -E -u %s %s"%(user_prefs.crm_user,cmd)
    return cmd
def pipe_string(cmd,s):
    rc = -1 # command failed
    cmd = add_sudo(cmd)
    p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE)
    try:
        p.communicate(s)
        p.wait()
        rc = p.returncode
    except IOError, msg:
        if not ("Broken pipe" in msg):
            common_err(msg)
    return rc

def str2tmp(s):
    '''
    Write the given string to a temporary file. Return the name
    of the file.
    '''
    fd,tmp = mkstemp()
    try: f = os.fdopen(fd,"w")
    except IOError, msg:
        common_err(msg)
        return
    f.write(s)
    f.close()
    return tmp

def is_filename_sane(name):
    if re.search("['`/#*?$\[\]]",name):
        common_err("%s: bad name"%name)
        return False
    return True
def is_name_sane(name):
    if re.search("[']",name):
        common_err("%s: bad name"%name)
        return False
    return True
def is_value_sane(name):
    if re.search("[']",name):
        common_err("%s: bad name"%name)
        return False
    return True

def ext_cmd(cmd):
    if options.regression_tests:
        print ".EXT", cmd
    return subprocess.call(add_sudo(cmd), shell=True)

def get_stdout(cmd, stderr_on = True):
    '''
    Run a cmd, return stdin output.
    stderr_on controls whether to show output which comes on stderr.
    '''
    if stderr_on:
        stderr = None
    else:
        stderr = subprocess.PIPE
    proc = subprocess.Popen(cmd, shell = True, \
        stdout = subprocess.PIPE, stderr = stderr)
    outp = proc.communicate()[0]
    proc.wait()
    outp = outp.strip()
    return outp
def stdout2list(cmd, stderr_on = True):
    '''
    Run a cmd, fetch output, return it as a list of lines.
    stderr_on controls whether to show output which comes on stderr.
    '''
    s = get_stdout(add_sudo(cmd), stderr_on)
    return s.split('\n')

def wait4dc(what = "", show_progress = True):
    '''
    Wait for the DC to get into the S_IDLE state. This should be
    invoked only after a CIB modification which would exercise
    the PE. Parameter "what" is whatever the caller wants to be
    printed if showing progress.

    It is assumed that the DC is already in a different state,
    usually it should be either PENGINE or TRANSITION. This
    assumption may not be true, but there's a high chance that it
    is since crmd should be faster to move through states than
    this shell.

    Further, it may also be that crmd already calculated the new
    graph, did transition, and went back to the idle state. This
    may in particular be the case if the transition turned out to
    be empty.

    Tricky. Though in practice it shouldn't be an issue.

    There's no timeout, as we expect the DC to eventually becomes
    idle.
    '''
    cmd = "crmadmin -D"
    s = get_stdout(add_sudo(cmd))
    if not s.startswith("Designated"):
        common_warn("%s unexpected output: %s" % (cmd,s))
        return False
    dc = s.split()[-1]
    if not dc:
        common_warn("can't find DC in: %s" % s)
        return False
    cmd = "crmadmin -S %s" % dc
    cnt = 0
    output_started = 0
    while True:
        s = get_stdout(add_sudo(cmd))
        if not s.startswith("Status"):
            common_warn("%s unexpected output: %s" % (cmd,s))
            return False
        try: dc_status = s.split()[-2]
        except:
            common_warn("%s unexpected output: %s" % (cmd,s))
            return False
        if dc_status == "S_IDLE":
            if output_started:
                sys.stderr.write(" done\n")
            return True
        time.sleep(0.1)
        if show_progress:
            if not output_started:
                output_started = 1
                sys.stderr.write("waiting for %s to finish ." % what)
            cnt += 1
            if cnt % 5 == 0:
                sys.stderr.write(".")

def is_id_valid(id):
    """
    Verify that the id follows the definition:
    http://www.w3.org/TR/1999/REC-xml-names-19990114/#ns-qualnames
    """
    if not id:
        return False
    id_re = "^[A-Za-z_][\w._-]*$"
    return re.match(id_re,id)
def check_filename(fname):
    """
    Verify that the string is a filename.
    """
    fname_re = "^[^/]+$"
    return re.match(fname_re,id)

def is_process(s):
    proc = subprocess.Popen("ps -e -o pid,command | grep -qs '%s'" % s, \
        shell=True, stdout=subprocess.PIPE)
    proc.wait()
    return proc.returncode == 0
def cluster_stack():
    if is_process("heartbeat:.[m]aster"):
        return "heartbeat"
    elif is_process("[a]isexec"):
        return "openais"
    return ""

def edit_file(fname):
    'Edit a file.'
    if not fname:
        return
    if not user_prefs.editor:
        return
    return ext_cmd("%s %s" % (user_prefs.editor,fname))

def page_string(s):
    'Write string through a pager.'
    if not s:
        return
    w,h = get_winsize()
    if s.count('\n') < h:
        print s
    elif not user_prefs.pager or not sys.stdout.isatty() or options.batch:
        print s
    else:
        opts = ""
        if user_prefs.pager == "less":
            opts = "-R"
        pipe_string("%s %s" % (user_prefs.pager,opts), s)

def get_winsize():
    try:
        import curses
        curses.setupterm()
        w = curses.tigetnum('cols')
        h = curses.tigetnum('lines')
    except:
        try:
            w = os.environ['COLS']
            h = os.environ['LINES']
        except:
            w = 80; h = 25
    return w,h
def multicolumn(l):
    '''
    A ls-like representation of a list of strings.
    A naive approach.
    '''
    min_gap = 2
    w,h = get_winsize()
    max_len = 8
    for s in l:
        if len(s) > max_len:
            max_len = len(s)
    cols = w/(max_len + min_gap)  # approx.
    col_len = w/cols
    for i in range(len(l)/cols + 1):
        s = ''
        for j in range(i*cols,(i+1)*cols):
            if not j < len(l):
                break
            if not s:
                s = "%-*s" % (col_len,l[j])
            elif (j+1)%cols == 0:
                s = "%s%s" % (s,l[j])
            else:
                s = "%s%-*s" % (s,col_len,l[j])
        if s:
            print s

def find_value(pl,name):
    for n,v in pl:
        if n == name:
            return v
    return None

def lines2cli(s):
    '''
    Convert a string into a list of lines. Replace continuation
    characters. Strip white space, left and right. Drop empty lines.
    '''
    cl = []
    l = s.split('\n')
    cum = []
    for p in l:
        p = p.strip()
        if p.endswith('\\'):
            p = p.rstrip('\\')
            cum.append(p)
        else:
            cum.append(p)
            cl.append(''.join(cum).strip())
            cum = []
    if cum: # in case s ends with backslash
        cl.append(''.join(cum))
    return [x for x in cl if x]

user_prefs = UserPrefs.getInstance()
options = Options.getInstance()
vars = Vars.getInstance()
# vim:ts=4:sw=4:et:
