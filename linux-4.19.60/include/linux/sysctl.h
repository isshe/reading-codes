/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sysctl.h: General linux system control interface
 *
 * Begun 24 March 1995, Stephen Tweedie
 *
 ****************************************************************
 ****************************************************************
 **
 **  WARNING:
 **  The values in this file are exported to user space via 
 **  the sysctl() binary interface.  Do *NOT* change the
 **  numbering of any existing values here, and do not change
 **  any numbers within any one set of values.  If you have to
 **  redefine an existing interface, use a new number for it.
 **  The kernel will then return -ENOTDIR to any application using
 **  the old binary interface.
 **
 ****************************************************************
 ****************************************************************
 */
#ifndef _LINUX_SYSCTL_H
#define _LINUX_SYSCTL_H

#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/wait.h>
#include <linux/rbtree.h>
#include <linux/uidgid.h>
#include <uapi/linux/sysctl.h>

/* For the /proc/sys support */
struct completion;
struct ctl_table;
struct nsproxy;
struct ctl_table_root;
struct ctl_table_header;
struct ctl_dir;

typedef int proc_handler (struct ctl_table *ctl, int write,
			  void __user *buffer, size_t *lenp, loff_t *ppos);
/*
 * 以下是一些初始化proc_handler的函数。
 */
// 读/写一个字符串
extern int proc_dostring(struct ctl_table *, int,
			 void __user *, size_t *, loff_t *);
// 读写一个包含一个或多个整数的数组
extern int proc_dointvec(struct ctl_table *, int,
			 void __user *, size_t *, loff_t *);
// 读写一个包含一个或多个无符号整数的数组
extern int proc_douintvec(struct ctl_table *, int,
			 void __user *, size_t *, loff_t *);
//类似proc_dointvec,但是要确定输入数据在min/max范围内。不符合的会被拒绝。
extern int proc_dointvec_minmax(struct ctl_table *, int,
				void __user *, size_t *, loff_t *);
extern int proc_douintvec_minmax(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos);
// 读写一个整数数组。内核变量单位是jiffies。返回给用户之前会先转成秒数。
extern int proc_dointvec_jiffies(struct ctl_table *, int,
				 void __user *, size_t *, loff_t *);
extern int proc_dointvec_userhz_jiffies(struct ctl_table *, int,
					void __user *, size_t *, loff_t *);
// 读写一个整数数组。内核单位是jiffies。返回给用户之前会先转成毫秒数。
extern int proc_dointvec_ms_jiffies(struct ctl_table *, int,
				    void __user *, size_t *, loff_t *);
// 读写长整型数组
extern int proc_doulongvec_minmax(struct ctl_table *, int,
				  void __user *, size_t *, loff_t *);
extern int proc_doulongvec_ms_jiffies_minmax(struct ctl_table *table, int,
				      void __user *, size_t *, loff_t *);
extern int proc_do_large_bitmap(struct ctl_table *, int,
				void __user *, size_t *, loff_t *);

/*
 * Register a set of sysctl names by calling register_sysctl_table
 * with an initialised array of struct ctl_table's.  An entry with 
 * NULL procname terminates the table.  table->de will be
 * set up by the registration and need not be initialised in advance.
 *
 * sysctl names can be mirrored automatically under /proc/sys.  The
 * procname supplied controls /proc naming.
 *
 * The table's mode will be honoured both for sys_sysctl(2) and
 * proc-fs access.
 *
 * Leaf nodes in the sysctl tree will be represented by a single file
 * under /proc; non-leaf nodes will be represented by directories.  A
 * null procname disables /proc mirroring at this node.
 *
 * sysctl(2) can automatically manage read and write requests through
 * the sysctl table.  The data and maxlen fields of the ctl_table
 * struct enable minimal validation of the values being written to be
 * performed, and the mode field allows minimal authentication.
 * 
 * There must be a proc_handler routine for any terminal nodes
 * mirrored under /proc/sys (non-terminals are handled by a built-in
 * directory handler).  Several default handlers are available to
 * cover common cases.
 */

/* Support for userspace poll() to watch for changes */
struct ctl_table_poll {
	atomic_t event;
	wait_queue_head_t wait;
};

static inline void *proc_sys_poll_event(struct ctl_table_poll *poll)
{
	return (void *)(unsigned long)atomic_read(&poll->event);
}

#define __CTL_TABLE_POLL_INITIALIZER(name) {				\
	.event = ATOMIC_INIT(0),					\
	.wait = __WAIT_QUEUE_HEAD_INITIALIZER(name.wait) }

#define DEFINE_CTL_TABLE_POLL(name)					\
	struct ctl_table_poll name = __CTL_TABLE_POLL_INITIALIZER(name)

/*
 * A sysctl table is an array of struct ctl_table:
 * /proc/sys中的文件和目录都是以ctl_table结构定义的
 */
struct ctl_table 
{
	// 名称
	const char *procname;		/* Text ID for /proc/sys, or zero */
	void *data;
	int maxlen;	// 内核变量的尺寸大小
	umode_t mode;	// 相关联文件或目录的访问权限
	struct ctl_table *child;	/* Deprecated */	// 用于建立目录与文件之间的父子关系（已弃用！）
	// 读写/proc/sys文件时，完成读取或写入操作的函数。
	proc_handler *proc_handler;	/* Callback for text formatting */
	struct ctl_table_poll *poll;
	void *extra1;	// 以下是两个可选参数，通常用于定义变量的最大值和最小值。
	void *extra2;
} __randomize_layout;

struct ctl_node {
	struct rb_node node;
	struct ctl_table_header *header;
};

/* struct ctl_table_header is used to maintain dynamic lists of
   struct ctl_table trees. */
struct ctl_table_header
{
	union {
		struct {
			struct ctl_table *ctl_table;
			int used;
			int count;
			int nreg;
		};
		struct rcu_head rcu;
	};
	struct completion *unregistering;
	struct ctl_table *ctl_table_arg;
	struct ctl_table_root *root;
	struct ctl_table_set *set;
	struct ctl_dir *parent;
	struct ctl_node *node;
	struct hlist_head inodes; /* head for proc_inode->sysctl_inodes */
};

struct ctl_dir {
	/* Header must be at the start of ctl_dir */
	struct ctl_table_header header;
	struct rb_root root;
};

struct ctl_table_set {
	int (*is_seen)(struct ctl_table_set *);
	struct ctl_dir dir;
};

struct ctl_table_root {
	struct ctl_table_set default_set;
	struct ctl_table_set *(*lookup)(struct ctl_table_root *root);
	void (*set_ownership)(struct ctl_table_header *head,
			      struct ctl_table *table,
			      kuid_t *uid, kgid_t *gid);
	int (*permissions)(struct ctl_table_header *head, struct ctl_table *table);
};

/* struct ctl_path describes where in the hierarchy a table is added */
struct ctl_path {
	const char *procname;
};

#ifdef CONFIG_SYSCTL

void proc_sys_poll_notify(struct ctl_table_poll *poll);

extern void setup_sysctl_set(struct ctl_table_set *p,
	struct ctl_table_root *root,
	int (*is_seen)(struct ctl_table_set *));
extern void retire_sysctl_set(struct ctl_table_set *set);

struct ctl_table_header *__register_sysctl_table(
	struct ctl_table_set *set,
	const char *path, struct ctl_table *table);
struct ctl_table_header *__register_sysctl_paths(
	struct ctl_table_set *set,
	const struct ctl_path *path, struct ctl_table *table);
struct ctl_table_header *register_sysctl(const char *path, struct ctl_table *table);
struct ctl_table_header *register_sysctl_table(struct ctl_table * table);
struct ctl_table_header *register_sysctl_paths(const struct ctl_path *path,
						struct ctl_table *table);

void unregister_sysctl_table(struct ctl_table_header * table);

extern int sysctl_init(void);

extern struct ctl_table sysctl_mount_point[];

#else /* CONFIG_SYSCTL */
static inline struct ctl_table_header *register_sysctl_table(struct ctl_table * table)
{
	return NULL;
}

static inline struct ctl_table_header *register_sysctl_paths(
			const struct ctl_path *path, struct ctl_table *table)
{
	return NULL;
}

static inline struct ctl_table_header *register_sysctl(const char *path, struct ctl_table *table)
{
	return NULL;
}

static inline void unregister_sysctl_table(struct ctl_table_header * table)
{
}

static inline void setup_sysctl_set(struct ctl_table_set *p,
	struct ctl_table_root *root,
	int (*is_seen)(struct ctl_table_set *))
{
}

#endif /* CONFIG_SYSCTL */

int sysctl_max_threads(struct ctl_table *table, int write,
		       void __user *buffer, size_t *lenp, loff_t *ppos);

#endif /* _LINUX_SYSCTL_H */
