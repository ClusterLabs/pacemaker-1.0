#ifndef _HBAGENTV2_H 
#define _HBAGENTV2_H 



struct hb_rsinfov2 {
    size_t index;
    char * resourceid;
    uint32_t type;
    uint32_t status;
    char * node;
    uint32_t is_managed;
    uint32_t failcount;
    char * parent;
};

int init_hbagentv2(void);
void free_hbagentv2(void);

int get_cib_fd(void);
int handle_cib_msg(void);

int init_resource_table_v2(void);
int update_resource_table_v2(void);
GPtrArray *get_resource_table_v2(void);
void free_resource_table_v2(void);

#endif  /* _HBAGENTV2_H */
