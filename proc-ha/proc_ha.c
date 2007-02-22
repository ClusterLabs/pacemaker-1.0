/*
 * Linux-HA /proc interface
 *
 * Author(s): Volker Wiegand <wiegand@suse.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 * A copy of the License can be found at /usr/src/linux/COPYING.
 *
 * The following sources provided invaluable information for me:
 * - the book "Linux Device Drivers" by Alessandro Rubini
 *            published by O'Reilly & Associates
 * - the file lvm.c from the LVM package by Heinz Mauelshagen
 *            <mauelsha@ez-darmstadt.telekom.de>
 * - ... and of course the Linux Kernel -- amazing stuff indeed.
 *
 * Please visit the end of this file for a complete history log.
 */

#if !defined(__KERNEL__)
#  define __KERNEL__
#endif
#if !defined(MODULE)
#  define MODULE
#endif

#define HA_CTRL		"/proc/ha/.control: "

#define MAX_LINE	(4096)
#define MAX_ARGS	(128)


#include <linux/module.h>

#if !defined(VERSION_CODE)
#	define VERSION_CODE(v,r,s) (((v) << 16) + ((r) << 8) + (s))
#endif

/*	Not sure when I need this... Wanger told me to put it in :-) */
/*	Advice appreciated... :-) */


#include <portability.h>
#include <linux/malloc.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/types.h>

#if LINUX_VERSION_CODE >= VERSION_CODE(2,1,0)
#  include <asm/uaccess.h>
#  define memcpy_fromfs		copy_from_user
#  define memcpy_tofs		copy_to_user
#  define proc_register_dynamic	proc_register
#  define verify_area		!access_ok
#  define SSIZE_T ssize_t
#else
#  define SSIZE_T int
#endif


/****************************************************
 * Declare the local data types
 ****************************************************/

/*
 *	{Name,Value} pairs
 *	A fundamental type for our work.
 */

#define	NVMAGIC	0xFEEDBEEF
struct nvpair {
	int	nvmagic;
	int	npairs;
	char **	names;
	char **	values;
};


struct node_info {
	struct nvpair*	attributes;
};

#define	HBTIME	"hbtime"

#if 0
static void dump_nv(const struct nvpair*p);
#endif
static char * nvlookup(const struct nvpair*p, const char * name);
static void nvdelete(struct nvpair*p);
static struct nvpair * argv2pairs (int argc, char ** argv, char ** valp);
static int add_entry2dir(struct proc_dir_entry *dir, const char * name);

/****************************************************
 * We need some basic string functions
 *	They are defines as inline in linux/string.h,
 *	but they kinda cannot be used within modules.
 ****************************************************/

static inline int mystrcmp(const char * s1, const char * s2)
{
	if (s1 == NULL || s2 == NULL)
		return 0;	/* Hmmm, what should it actually be? */

	while (*s1 != '\0' && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (((int) *s1) - ((int) *s2));
}

static inline int mystrlen(const char * s)
{
	int i;

	if (s == NULL)
		return 0;	/* Hmmm, what should it actually be? */

	for (i = 0; *s != '\0'; s++)
		i++;
	return i;
}

static inline void * mymemset(const void * p, int v, size_t c)
{
	void *q;

	if ((q = (void *) p) == NULL)
		return NULL;	/* Hmmm, what should it actually be? */

	while (c-- > 0)
		* (char *) p++ = (char) v;

	return q;
}

static inline void * mymemcpy(const void * p1, const void * p2, size_t c)
{
	void *q;

	if ((q = (void *) p1) == NULL || p2 == NULL)
		return NULL;	/* Hmmm, what should it actually be? */

	while (c-- > 0)
		* (char *) p1++ = * (char *) p2++;

	return q;
}


/****************************************************
 * Declare the local functions
 ****************************************************/

static SSIZE_T proc_hactl_write(struct file *, const char *, size_t, loff_t *);
static SSIZE_T proc_hb_read(struct file *, char *, size_t, loff_t *);
static SSIZE_T proc_read_allattrs(struct file * file, char *, size_t, loff_t *);

static int proc_hactl_add(int, char **, char **);
static int proc_hactl_del(int, char **, char **);
static int proc_hactl_hbt(int, char **, char **);

static int proc_add_node(const char *);
static int proc_del_node(const char *);


/****************************************************
 * Now the data structures for the /proc entries
 ****************************************************/

/*
 * Our "root" directory
 */
static struct proc_dir_entry proc_ha = {
	0, 2, "ha",				/* inode, name */
	S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0,	/* mode, nlink, uid, gid */
	0, &proc_dir_inode_operations,		/* size, ops */
	NULL, NULL,				/* get_info, fill_inode */
	NULL,					/* next */
	NULL, NULL				/* parent, subdir */
};


/*
 * The common entry point for updating the configuration
 */
static struct file_operations proc_hactl_operations = {
	NULL,			/* lseek - default */
	NULL,			/* read - bad */
	proc_hactl_write,	/* write - update configuration */
	NULL,			/* readdir */
	NULL,			/* select - default */
	NULL,			/* ioctl - default */
	NULL,			/* mmap */
	NULL,			/* no special open code */
	NULL,			/* no special release code */
	NULL			/* can't fsync */
};

/*
 * this file is just good for writing ...
 */
static struct inode_operations proc_hactl_inode_operations = {
	&proc_hactl_operations,	/* control file-ops */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};

static struct proc_dir_entry proc_ha_control = {
	0, C_STRLEN(".control"), ".control",	/* inode, name */
	S_IFREG | S_IWUSR, 1, 0, 0,		/* mode, nlink, uid, gid */
	0, &proc_hactl_inode_operations,	/* size, ops */
	NULL, NULL,				/* get_info, fill_inode */
	NULL,					/* next */
	NULL, NULL				/* parent, subdir */
};

static struct file_operations proc_attr_dir_operations;
static struct inode_operations proc_attr_dir_inode_operations;

/*
 * Nodes (the machines comprising the cluster)
 */
static struct proc_dir_entry proc_ha_nodes = {
	0, C_STRLEN("nodes"), "nodes",		/* inode, name */
	S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0,	/* mode, nlink, uid, gid */
	0, &proc_dir_inode_operations,		/* size, ops */
	NULL, NULL,				/* get_info, fill_inode */
	NULL,					/* next */
	NULL, NULL				/* parent, subdir */
};

static struct file_operations proc_hb_operations = {
	NULL,			/* lseek */
	proc_hb_read,		/* read heartbeat */
	NULL,			/* write */
	NULL,			/* readdir */
	NULL,			/* poll */
	NULL,			/* ioctl */
	NULL,			/* mmap */
	NULL,			/* open */
	NULL,			/* flush */
	NULL,			/* release */
	NULL			/* fsync */
};

static struct inode_operations proc_hb_inode_operations = {
	&proc_hb_operations,	/* heartbeat file-ops */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};


/*
 * Resources (various things, like IP-addresses, applications, etc.)
 */
static struct proc_dir_entry proc_ha_resources = {
	0, C_STRLEN("resources"), "resources",	/* inode, name */
	S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0,	/* mode, nlink, uid, gid */
	0, &proc_dir_inode_operations,		/* size, ops */
	NULL, NULL,				/* get_info, fill_inode */
	NULL,					/* next */
	NULL, NULL				/* parent, subdir */
};


/****************************************************
 * Local functions to support update operations
 ****************************************************/

static SSIZE_T proc_hactl_write(struct file * file,
		const char * buf, size_t count, loff_t *ppos)
{
	char line[MAX_LINE], *p, *argv[MAX_ARGS], *valp[MAX_ARGS];
	int argc, i;

	/*
	** FIXME: on my 2.2.5 kernel the writing program
	**	*ALWAYS* returns 0 exit status. WHY ????
	*/
	if (count >= sizeof(line))
		return -E2BIG;
	if (memcpy_fromfs(line, buf, count))
		return -EFAULT;

	line[count] = '\0';
	for (p = line, argc = 0; *p; ) {
		while (*p == ' ' ||
				*p == '\t' ||
				*p == '\n' ||
				*p == '\r')
			p++;		/* skip white space	*/
		if (*p == '\0')
			break;

		argv[argc] = p;		/* found an argument	*/
		while (*p != '\0' &&
				*p != '='  &&
				*p != ' '  &&
				*p != '\t' &&
				*p != '\n' &&
				*p != '\r')
			p++;
		if (*p == '=') {
			*p++ = '\0';	/* now get the value	*/
			valp[argc] = p;	/* found arg's value	*/

			/* FIXME: allow quoted strings */
			
			while (*p != '\0' &&
#if 0
					*p != ' '  &&
					*p != '\t' &&
#endif
					*p != '\n' &&
					*p != '\r')
				p++;
		} else
			valp[argc] = NULL;
		if (*p != '\0')
			*p++ = '\0';	/* terminate argv/valp	*/

		if (++argc >= MAX_ARGS)
			break;		/* rude overflow check	*/
	}
	if (argc < 1)
		return 0;

#if 0
	for (i = 0; i < argc; i++) {
		printk(KERN_INFO HA_CTRL "%2d: '%s'='%s'\n", i + 1,
				argv[i], valp[i] ? valp[i] : "(null)");
	}
#endif

	/*
	** Now for the real work ...
	**
	** FIXME: this whole args scanning could be table driven, but
	**	  for now it should serve the purpose good enough.
	*/
	if (mystrcmp(argv[0], "add") == 0) {
		i = proc_hactl_add(--argc, &argv[1], &valp[1]);
		return ((i < 0) ? i : count);
	}
	if (mystrcmp(argv[0], "del") == 0) {
		i = proc_hactl_del(--argc, &argv[1], &valp[1]);
		return ((i < 0) ? i : count);
	}
	if (mystrcmp(argv[0], "hb") == 0) {
		i = proc_hactl_hbt(--argc, &argv[1], &valp[1]);
		return ((i < 0) ? i : count);
	}
	/* add more basic commands here if you like ... */

	printk(KERN_INFO HA_CTRL "unknown cmd '%s'\n", argv[0]);
	return -EINVAL;
}

static int proc_hactl_add(int argc, char ** argv, char ** valp)
{
	char *ptype = NULL, *pname = NULL;
	int i;

	for (i = 0; i < argc; i++) {
		if (mystrcmp(argv[i], "type") == 0)
			ptype = valp[i];
		else if (mystrcmp(argv[i], "node") == 0)
			pname = valp[i];
		else {
			printk(KERN_INFO HA_CTRL
				"add: unknown arg-type '%s' (ignore)\n",
				argv[i]);
		}
	}

	if (ptype == NULL) {
		printk(KERN_INFO HA_CTRL "add: missing type\n");
		return -EINVAL;
	}
	if (pname == NULL) {
		printk(KERN_INFO HA_CTRL "add: missing name\n");
		return -EINVAL;
	}

	/*
	** Now see what we have to add ...
	*/
	if (mystrcmp(ptype, "node") == 0)
		return proc_add_node(pname);
	/* add more types to add here if you like ... */

	printk(KERN_INFO HA_CTRL "add: unknown type '%s'\n", ptype);
	return -EINVAL;
}

static int proc_hactl_del(int argc, char ** argv, char ** valp)
{
	char *ptype = NULL, *pname = NULL;
	int i;

	for (i = 0; i < argc; i++) {
		if (mystrcmp(argv[i], "type") == 0)
			ptype = valp[i];
		else if (mystrcmp(argv[i], "node") == 0)
			pname = valp[i];
		else {
			printk(KERN_INFO HA_CTRL
				"del: unknown arg-type '%s' (ignore)\n",
				argv[i]);
		}
	}

	if (ptype == NULL) {
		printk(KERN_INFO HA_CTRL "del: missing type\n");
		return -EINVAL;
	}
	if (pname == NULL) {
		printk(KERN_INFO HA_CTRL "del: missing name\n");
		return -EINVAL;
	}

	/*
	** Now see what we have to del ...
	*/
	if (mystrcmp(ptype, "node") == 0)
		return proc_del_node(pname);
	/* add more types to del here if you like ... */

	printk(KERN_INFO HA_CTRL "del: unknown type '%s'\n", ptype);
	return -EINVAL;
}

static int proc_hactl_hbt(int argc, char ** argv, char ** valp)
{
	char *pname = NULL;
	struct proc_dir_entry *ent;
	struct node_info *pni;
	int i, len;

	for (i = 0; i < argc; i++) {
		if (mystrcmp(argv[i], "node") == 0)
			pname = valp[i];
	}

	if (pname == NULL) {
		printk(KERN_INFO HA_CTRL "hb: missing node\n");
		return -EINVAL;
	}
	len = mystrlen(pname);

	for (ent = proc_ha_nodes.subdir; ent; ent = ent->next) {
		struct proc_dir_entry *attr, *nextattr;
		if (ent->namelen != len)
			continue;
		if (mystrcmp(ent->name, pname) != 0)
			continue;

		if ((pni = (struct node_info *) ent->data) == NULL)
			return -EFAULT;
		if (pni->attributes) {
			nvdelete(pni->attributes); /* Trash old values */
		}
		if ((pni->attributes = argv2pairs(argc, argv, valp)) == NULL) {
			return(-ENOMEM);
		}
		if (nvlookup(pni->attributes, HBTIME) == NULL) {
			printk(KERN_INFO HA_CTRL "hb: No Timestamp '%s'\n"
			,	pname);
		}

#if 0
		printk(KERN_INFO HA_CTRL "hb: from '%s'\n", pname);
#endif
		for (i = 0; i < argc; i++) {
			if (mystrcmp(argv[i], "node") == 0)
				continue;
#if 0
			printk(KERN_INFO "Adding entry %s to directory %s\n"
			,	argv[i], ent->name);
#endif
			add_entry2dir(ent, argv[i]);
		}
		for (attr = ent->subdir; attr; attr=nextattr)  {
			nextattr=attr->next;
			if (nvlookup(pni->attributes, attr->name) == NULL) {
				proc_unregister(ent, attr->low_ino);
			}
		}
		return 0;
	}

	printk(KERN_INFO HA_CTRL "hb: unknown node '%s'\n", pname);
	return -ENOENT;
}


/****************************************************
 * Local functions working on nodes
 ****************************************************/

static int proc_add_node(const char * name)
{
	struct proc_dir_entry *ent;
	struct proc_dir_entry *sub;
	struct node_info *pni;
	int len;

	if (name == NULL || *name == '\0')
		return -EINVAL;
	len = mystrlen(name);

	/* Is this node already added? */
	for (sub = proc_ha_nodes.subdir; sub; sub=sub->next) {
		if (sub->namelen != len)
			continue;
		if (mystrcmp(sub->name, name) == 0) {
			printk ("node %s already added\n", name);
			return 0;
		}
	}

	if ((ent = (struct proc_dir_entry *) kmalloc(sizeof(*ent)
				+ len + 1, GFP_KERNEL)) == NULL)
		return -ENOMEM;
	if ((pni = (struct node_info *) kmalloc(sizeof(*pni),
				GFP_KERNEL)) == NULL) {
		kfree(ent);
		return -ENOMEM;
	}
	mymemset(pni, 0, sizeof(*pni));

	mymemset(ent, 0, sizeof(*ent));
	mymemcpy(((char *) ent) + sizeof(*ent), name, len+1);
	ent->name     = ((char *) ent) + sizeof(*ent);
	ent->namelen  = len;
	ent->mode     = S_IFDIR | S_IRUGO | S_IXUGO;
	ent->nlink    = 2;
	ent->ops      = &proc_attr_dir_inode_operations;
	ent->data     = (void *) pni;

	/* FIXME: check for errors ??? */
	proc_register_dynamic(&proc_ha_nodes, ent);

	return 0;
}

static int
add_entry2dir(struct proc_dir_entry *dir, const char * name)
{
	struct proc_dir_entry *sub;
	struct proc_dir_entry *newent;
	int		len = mystrlen(name);


	for (sub = dir->subdir; sub; sub=sub->next) {
		if (sub->namelen != len)
			continue;
		if (mystrcmp(sub->name, name) != 0)
			continue;
		/* Already exists.  Fine. */
		return 0;
	}
	if ((newent = (struct proc_dir_entry *) kmalloc(sizeof(*newent)
			+ len + 1, GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	mymemset(newent, 0, sizeof(*newent));
	newent->name    = ((char *) newent) + sizeof(*newent);
	mymemcpy(newent->name, name, len+1);
	newent->namelen = len;
	newent->mode    = S_IFREG | S_IRUGO;
	newent->nlink   = 1;
	newent->ops      = &proc_hb_inode_operations;
	newent->data     = NULL;

	/* FIXME: check for errors ??? */
	proc_register_dynamic(dir, newent);
	return 0;
}

static int proc_del_node(const char * name)
{
	struct proc_dir_entry *ent, *sub, *nextent, *prevent;
	int len;

	if (name == NULL || *name == '\0')
		return -EINVAL;
	len = mystrlen(name);

	prevent = NULL;
	for (ent = proc_ha_nodes.subdir; ent; (prevent=ent, ent = nextent)) {
		nextent = ent->next;
		if (ent->namelen != len)
			continue;
		if (mystrcmp(ent->name, name) != 0)
			continue;

		for (sub = ent->subdir; sub; ) {
			struct proc_dir_entry *tmp = sub->next;
			proc_unregister(ent, sub->low_ino);
			kfree(sub);
			sub = tmp;
		}
		proc_unregister(&proc_ha_nodes, ent->low_ino);

		if (ent->data) {
			struct node_info *pni = ent->data;
			if (pni->attributes) {
				nvdelete(pni->attributes);
			}
			kfree(ent->data);
		}
		kfree(ent);
		break;
	}

	/* Did we find a matching entry? */
	if (ent == NULL) {
		printk("del_node %s failed!\n", name);
		/* No, return an error */
		return -EEXIST;
	}
	if (prevent) {
		prevent->next = nextent;
	}else{
		proc_ha_nodes.subdir = nextent;
	}

	return 0;
}

/* Clean up before exiting... */
static void proc_del_all_nodes(void)
{
	struct proc_dir_entry *ent, *nextent;

	for (ent = proc_ha_nodes.subdir; ent; ent = nextent) {
		nextent = ent->next;
		proc_del_node(ent->name);
	}
		
}

#define	COPYOUT(s) {							\
			const char * sp = (s);				\
			SSIZE_T len;					\
			len = mystrlen(sp);				\
			if (count < len) {				\
				count = len;				\
			}						\
			if (count > 0 && memcpy_tofs(buf, sp, len)) {	\
				return(EFAULT);				\
			}						\
			*ppos += len;					\
			buf += len;					\
			rc += len;					\
			count -= len;					\
		}
static SSIZE_T proc_hb_read(struct file * file,
		char * buf, size_t count, loff_t * ppos)
{
	SSIZE_T len = 0;
	struct inode * inode = file->f_dentry->d_inode;
	struct proc_dir_entry *dp;
	struct node_info *ni;
	const char *	attrval;
	int		rc = 0;

	if (*ppos > 0) {
		return(0);	/* Give EOF for subsequent reads */
	}

	if (inode == NULL) {
		printk(KERN_INFO "proc_hb_read: NULL inode\n");
		return(-EINVAL);
	}
	if ((dp = (struct proc_dir_entry *) inode->u.generic_ip) == NULL) {
		printk(KERN_INFO "proc_hb_read: NULL dp\n");
		return(-EINVAL);
	}
	if ((ni = (struct node_info *) dp->parent->data) == NULL) {
		printk(KERN_INFO "proc_hb_read: NULL ni\n");
		return(-EINVAL);
	}

	{
		/* The cool thing about this code is that it doesn't
		 * gets the name of the attribute to look up from the
		 * name of the file the user has opened.  We don't much
		 * care what that name is.  Of course, someone still
		 * needs to do all the right proc_register_dynamic() calls
		 * etc. to set all this up right so this code works.
		 */
		attrval = nvlookup(ni->attributes, dp->name);
		if (attrval == NULL) {
			/* End of File.  Should it be an error? */
			len = 0;
		}else{
			COPYOUT(attrval);
			COPYOUT("\n");
		}
	}


	*ppos = len;
	return rc;
}

static SSIZE_T proc_read_allattrs(struct file * file,
		char * buf, size_t count, loff_t * ppos)
{
	struct inode * inode = file->f_dentry->d_inode;
	struct proc_dir_entry *dp;
	struct node_info *ni;
	int		j;
	int		rc = 0;

#if 0
	printk(KERN_INFO "in proc_read_allattrs\n");
#endif
	if (*ppos > 0) {
		return(0);	/* Give EOF for subsequent reads */
	}

	if (inode == NULL) {
		printk(KERN_INFO "proc_hb_read: NULL inode\n");
		return(-EINVAL);
	}
	if ((dp = (struct proc_dir_entry *) inode->u.generic_ip) == NULL) {
		printk(KERN_INFO "proc_read_allattrs: NULL dp\n");
		return(-EINVAL);
	}
	if ((ni = (struct node_info *) dp->data) == NULL) {
		printk(KERN_INFO "proc_read_allattrs: NULL ni\n");
		return(-EINVAL);
	}



	for (j=0; j < ni->attributes->npairs; ++j) {
		/* Skip the name */
		if (mystrcmp(ni->attributes->names[j], "node") == 0)
			continue;
		COPYOUT(ni->attributes->names[j]);
		COPYOUT("=");
		COPYOUT(ni->attributes->values[j]);
		COPYOUT("\n");
	}

	return rc;
}


/****************************************************
 * Local functions working on resources
 ****************************************************/


/****************************************************
 * Local functions working on {name,value} pairs
 ****************************************************/

static char *
nvlookup(const struct nvpair*p, const char * name)
{
	int	j;

	for (j=0; j < p->npairs; ++j) {
		if (mystrcmp(name, p->names[j]) == 0) {
			return(p->values[j]);
		}
	}
	return(NULL);
}

static void
nvdelete(struct nvpair*p)
{
	int	j;

	if (p == NULL) {
		printk(KERN_INFO "deleting NULL nvpair");
		return;
	}
	if (p->nvmagic != NVMAGIC) {
		printk(KERN_INFO "deleting bogus nvpair 0x%x"
		,	p->nvmagic);
		return;
	}
	p->nvmagic = ~NVMAGIC;

	for (j=0; j < p->npairs; ++j) {
		if (p->names[j]) {
			kfree(p->names[j]);
		}
		if (p->values[j]) {
			kfree(p->values[j]);
		}
	}
	kfree(p->names);
	kfree(p->values);
	kfree(p);
}

static struct nvpair *
argv2pairs (int argc, char ** argv, char ** valp)
{
	struct nvpair *ret;
	char **		n;	/* The list of names */
	char **		v;	/* The list of values */
	int		j;
	if ((ret = (struct nvpair*) kmalloc(sizeof(*ret),GFP_KERNEL)) == NULL) {
		return(NULL);
	}
	if ((n = (char **) kmalloc((argc*sizeof(*n)), GFP_KERNEL)) == NULL) {
		kfree(ret);
		return(NULL);
	}
	if ((v = (char **) kmalloc((argc*sizeof(*v)), GFP_KERNEL)) == NULL) {
		kfree(n);
		kfree(ret);
		return(NULL);
	}
	mymemset(ret, 0, sizeof(*ret));
	mymemset(n, 0, argc*sizeof(*n));
	mymemset(v, 0, argc*sizeof(*v));
	ret->names = n;
	ret->values = v;
	ret->npairs = argc;
	ret->nvmagic = NVMAGIC;

	/* From this point on, nvdelete is able to destroy "ret" on error */

	for (j=0; j < argc; ++j) {
		int	nsize = mystrlen(argv[j])+1;
		int	vsize = mystrlen(valp[j])+1;

		if ((n[j] = (char *) kmalloc((nsize*sizeof(char)), GFP_KERNEL))
		==	NULL) {
			nvdelete(ret);
			return(NULL);
		}
		mymemcpy(n[j], argv[j], nsize);
		
		if ((v[j] = (char *) kmalloc((vsize*sizeof(char)), GFP_KERNEL))
		==	NULL) {
			nvdelete(ret);
			return(NULL);
		}
		mymemcpy(v[j], valp[j], vsize);
	}
#if 0
	printk(KERN_INFO "Created %d {n,v} pairs:\n", argc);
	dump_nv(ret);
#endif
	return(ret);
}
#if 0
static void
dump_nv(const struct nvpair*p)
{
	int		j;

	printk(KERN_INFO "Dumping %d {name,value} pairs\n", p->npairs);

	for (j=0; j < p->npairs; ++j) {
		printk(KERN_INFO "%s=%s\n"
		,	(p->names[j]  ? p->names[j]  : "(null)")
		,	(p->values[j] ? p->values[j] : "(null)"));
	}
}
#endif

/****************************************************
 * Initialization and termination code
 ****************************************************/

int init_module(void)
{
	printk(KERN_INFO "Installing /proc/ha interface\n");
	proc_attr_dir_inode_operations = proc_dir_inode_operations;
	proc_attr_dir_operations
	=	*proc_attr_dir_inode_operations.default_file_ops;
	proc_attr_dir_inode_operations.default_file_ops
	=	&proc_attr_dir_operations;
	proc_attr_dir_operations.read = proc_read_allattrs;
	/* There's got to be a better way... */

	/* FIXME: check for errors ??? */
	proc_register_dynamic(&proc_root, &proc_ha);
	proc_register_dynamic(&proc_ha,   &proc_ha_nodes);
	proc_register_dynamic(&proc_ha,   &proc_ha_resources);
	proc_register_dynamic(&proc_ha,   &proc_ha_control);

	return 0;
}

void cleanup_module(void)
{
	printk(KERN_INFO "Removing /proc/ha interface\n");
	/* FIXME: need to remove nodes, resources, etc. before going */
	/* out of business (this may now be done) */

	proc_del_all_nodes();
	proc_unregister(&proc_ha,   proc_ha_control.low_ino);
	proc_unregister(&proc_ha,   proc_ha_resources.low_ino);
	proc_unregister(&proc_ha,   proc_ha_nodes.low_ino);
	proc_unregister(&proc_root, proc_ha.low_ino);
}


