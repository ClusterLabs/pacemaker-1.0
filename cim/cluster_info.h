
#ifndef _CLUSTER_INFO_H
#define _CLUSTER_INFO_H

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <saf/ais.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <mgmt/mgmt.h>

#define CIM_MALLOC  cl_malloc
#define CIM_FREE    cl_free
#define CIM_STRDUP  cl_strdup
#define CIM_REALLOC cl_realloc

#define DEBUG_ENTER() cl_log(LOG_INFO, "%s: --- ENTER ---", __FUNCTION__)
#define DEBUG_LEAVE() cl_log(LOG_INFO, "%s: --- LEAVE ---", __FUNCTION__)

#define DEBUG_PID() cl_log(LOG_INFO,                    \
                        "%s: my pid is %d", __FUNCTION__, (int)getpid())


#ifndef ASSERT
#ifdef HAVE_STRINGIZE
#       define  ASSERT(X)    {if(!(X)) ci_assert(#X, __LINE__, __FILE__);}
#else
#       define  ASSERT(X)    {if(!(X)) ci_assert("X", __LINE__, __FILE__);}
#endif
#endif

#define TO_PTR(type, ptr)  ((type *)(ptr))

#define STR_N_A "N/A"
#define STR_CIB_TRUE         "true"
#define STR_CIB_FALSE        "false"
#define STR_CIB_IGNORE       "ignore"
#define STR_CIB_STONITH      "stonith"
#define STR_CIB_BLOCK        "block"

#define STR_CIB_STOP_START   "stop_start"
#define STR_CIB_STOP_ONLY    "stop_only"
#define STR_CIB_BLOCK        "block"
#define STR_CIB_ZERO         "0"
#define STR_CIB_INFINITY     "INFINITY"
#define STR_CIB_NEG_INFINITY "-INFINITY"
#define STR_CIB_NOTHING      "nothing"
#define STR_CIB_QUORUM       "quorum"
#define STR_CIB_FENCING      "fencing"


#define NODE_STATUS_ONLINE      0x01
#define NODE_STATUS_STANDBY     0x02
#define NODE_STATUS_UNCLEAN     0x04
#define NODE_STATUS_SHUTDOWN    0x08
#define NODE_STATUS_EXPECTEDUP  0x16
#define NODE_STATUS_ISDC        0x32

#define STR_ONLINE     "online"
#define STR_STANDBY    "standby"
#define STR_UNCLEAN    "unclean"
#define STR_SHUTDOWN   "shutdown"
#define STR_EXPECTEDUP "expectedup"


#define STR_CONS_ORDER        "rsc_order"
#define STR_CONS_LOCATION     "rsc_location"
#define STR_CONS_COLOCATION   "rsc_colocation"

#define TID_UNKNOWN          0x0

#define TID_NVPAIR           0x1
#define TID_RULE             0x11
#define TID_EXP              0x12
#define TID_DATE_EXP         0x13

#define TID_OP               0x21
#define TID_OPS              0x22

#define TID_ATTRIBUTES       0x31
#define TID_INST_ATTRIBUTES  0x32

#define TID_RES_PRIMITIVE    0x51
#define TID_RES_GROUP        0x52
#define TID_RES_CLONE        0x53
#define TID_RES_MASTER       0x54

#define TID_CONS_ORDER       0x61
#define TID_CONS_LOCATION    0x62
#define TID_CONS_COLOCATION  0x63

#define TID_ARRAY            0x100

#define CI_unknown 0x0

#define CI_uint32  0x10
#define CI_string  0x11
#define CI_table   0x12

#define LIB_INIT_ALL (ENABLE_HB|ENABLE_LRM|ENABLE_CRM)


#define CITableGet(x, key)  (x)->get_data(x, key)
#define CITableGetAt(x, i)  (x)->get_data_at(x, i)
#define CITableSize(x)      (x)->get_data_size(x)
#define CITableFree(x)      (x)->free(x)
struct ci_data {
        char * key;
        union {
                uint32_t uint32;
                char * string;
                struct ci_table * table;
        }  value;
        uint32_t type;

};

struct ci_table {
        void * private;
        uint32_t tid;
        const char * debug_id;

        uint32_t (* get_data_size)(const struct ci_table * table);

        struct ci_data
               (* get_data)(const struct ci_table * table, 
                            const char * key);
        struct ci_data 
               (* get_data_at)(const struct ci_table * table, int index);

        GPtrArray * (* get_keys)(const struct ci_table * table);

        void (* free) (struct ci_table * table);
};


/* new/free for ci_table */
struct ci_table * ci_table_new(int withkey);

/* tables */
struct ci_table * ci_get_resource_name_table (void);
GPtrArray * ci_get_sub_resource_name_table ( const char * id);
char * ci_get_res_running_node(const char * id);

struct ci_table * ci_get_resource_instattrs_table(const char * id);
GPtrArray * ci_get_constraint_name_table (uint32_t type);
GPtrArray * ci_get_node_name_table (void);

/* crm */
struct ci_table * ci_get_crm_config (void);

/* cluster */
struct ci_table * ci_get_cluster_config (void);
char * ci_get_cluster_dc (void);

/* nodes */

struct ci_table * ci_get_nodeinfo(const char * id);

/* resource */
struct ci_table * ci_get_primitive_resource (const char * id);
struct ci_table * ci_get_resource_clone (const char * id);
struct ci_table * ci_get_master_resource (const char * id);
struct ci_table * ci_get_resource_group(const char * id);

struct ci_table * ci_get_inst_operations(const char * id);
struct ci_table * ci_get_attributes(const char * id);
char * ci_get_resource_status(const char * id);
uint32_t ci_get_resource_type(const char * id);

/* constraint */
struct ci_table * ci_get_order_constraint (const char * id);
struct ci_table * ci_get_location_constraint (const char * id);
struct ci_table * ci_get_colocation_constraint (const char * id);


/* metadata */
GPtrArray * ci_get_res_class_table (void);
GPtrArray * ci_get_res_type_table(const char * class);
GPtrArray * ci_get_res_provider_table(const char * class, const char * type);
GPtrArray * ci_get_metadata_table(const char * class, const char * type, 
                                  const char * provider); 

/********************************
 * utilities
 *******************************/
int run_shell_command(const char * cmnd, int * ret, 
                      char *** std_out, char *** std_err);
int regex_search(const char * reg, const char * str, char *** match);
int free_2d_array(char ** array);
char * uuid_to_str(const cl_uuid_t * uuid);
void ci_safe_free(void * ptr);

void ci_assert(const char * assertion, int line, const char * file);
int init_logger(const char * entity);
void ci_free_ptr_array(GPtrArray * table, void (* free_ptr)(void * ptr));
int ci_lib_initialize(void);
void ci_lib_finalize(void);


#endif    /* _CLUSTER_INFO_H */
