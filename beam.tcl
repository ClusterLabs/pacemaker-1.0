source beam_default_parms.tcl

#######################################################################
#	Project standards issues
#######################################################################
set beam::allocation_may_return_null "yes"
 

set beam::MISTAKE21::enabling_policy "unsafe"

#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use strnlen(3) instead.",
#         category = unsafe
#       )
#} "strlen"
#
#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use strncmp(3)  instead.",
#         category = unsafe
#       )
#} "strcmp"
#
#

#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use strncpy(3)instead.",
#         category = unsafe
#       )
#} "strcpy"

#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use strncat(3) instead.",
#         category = unsafe
#       )
#} "strcat"

#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use snprintf(3) instead.",
#         category = unsafe
#       )
#} "sprintf"

#beam::attribute_by_signature {
#      advisory (
#         explanation = "This function is not safe. Use vsnprintf(3) instead.",
#         category = unsafe
#       )
#} "vsprintf"

beam::attribute_by_signature {
      advisory (
         explanation = "This function is not safe. Use fgets(3) instead.",
         category = unsafe
       )
} "gets"


#######################################################################
#	useful project definitions...
#######################################################################
beam::attribute_by_signature { noreturn } "cleanexit"
beam::attribute_by_signature { noreturn } "yy_fatal_error"



#######################################################################
#	Things broken outside of our control...
#######################################################################
lappend beam::MISTAKE15::disabled_macros	YYSTYPE XSRETURN LT_STMT_START __DBGTRACE
set beam::ERROR33::disabled_files "/*/*glib*/glib.h"
set beam::ERROR7::disabled_files "*/lib/bindings/perl/cl_raw/cl_raw_wrap.c"
set beam::disabled_files "*/libltdl/*"
# I think this yydestruct problem is a BEAM bug...
set  beam::MISTAKE1::disabled_functions  "yydestruct"
set  beam::ERROR33::disabled_functions  "g_bit_nth_msf"




#######################################################################
#	Stuff missing from glibc definitions
#######################################################################


beam::attribute_by_signature {
	allocator (
		return_index = return,
		initial_state = initialized_to_unknown,
		if_out_of_memory = return_null,
		resource = heap_memory
	),
	property (index = return,
                num_dereference = 0,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from malloc"
	),
	advisory (
		explanation = "This function is not safe. Use strndup() instead.",
		category = unsafe
       )
} "strdup"
beam::attribute_by_signature {
	allocator (
		return_index = return,
		initial_state = initialized_to_unknown,
		if_out_of_memory = return_null,
		resource = heap_memory
	),
	property (index = return,
                num_dereference = 0,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from malloc"
	),
} "strndup"

#######################################################################

beam::attribute_by_signature { allocator (
		size_index = 1,
		return_index = return,
		initial_state = uninitialized,
		if_size_is_0 = error,
		if_size_is_negative = error,
		if_out_of_memory = return_null,
		resource = heap_memory
	),
	property (index = return,
		type = provides,
                num_dereference = 0,
		property_name = "memory allocation source",
		property_value = "from cl_malloc"
	)
} "cl_malloc"

beam::attribute_by_signature {
	allocator (
		size_index = 1,
		return_index = return,
		initial_state = initialized_to_zero,
		if_size_is_0 = error,
		if_size_is_negative = error,
		if_out_of_memory = return_null,
		resource = heap_memory
	),
	property (index = return,
		type = provides,
                num_dereference = 0,
		property_name = "memory allocation source",
		property_value = "from cl_malloc"
	)
} "cl_calloc"

beam::attribute_by_signature {
	deallocator (
		pointer_index = 1,
		resource = heap_memory
	),
	property (index = 1,
		type = requires,
		property_name = "memory allocation source",
		property_value = "from cl_malloc"
	)
} "cl_free"

beam::resource_create { 
    name = "cl_msg",
    display = "cl_msg",       
    allocating_verb = "creating",
    allocated_verb = "created",
    freeing_verb = "destroying",
    freed_verb = "destroyed"
}

beam::attribute_by_signature {
	allocator (
		size_index = 1,
		return_index = return,
		initial_state = initialized_to_unknown,
		if_size_is_0 = error,
		if_size_is_negative = error,
		if_out_of_memory = return_null,
		resource = cl_msg
	),
	property (index = return,
		type = provides,
                num_dereference = 0,
		property_name = "memory allocation source",
		property_value = "from cl_msg_new"
	)
} "ha_msg_new" "cl_msg_new" "cl_msg_copy" "ha_msg_copy" "string2msg" "string2msg_ll" "wirefmt2msg" "wirefmt2msg_ll" "netstring2msg" "msgfromstream_string" "msgfromstream_netstring" "msgfromstream" "msgfromIPC" "msgfromIPC_noauth" "msgfromIPC_ll"

beam::attribute_by_signature {
	deallocator (
		pointer_index = 1,
		resource = cl_msg
	),
	property (index = 1,
		type = requires,
		property_name = "memory allocation source",
		property_value = "from cl_msg_new"
	)
} "cl_msg_del" "ha_msg_del"

#
#	glib memory malloc/free things
#
#	Note that glib memory allocation will *never* fail.
#
#	It will abort(3) instead.
#
#	So regardless of what policies you have for other memory,
#	glib memory needs then have the if_out_of_memory = ok attribute on all
#	the allocators, and be shown as from a different source.
#	This is true for all glib data structures.
#

beam::attribute_by_signature {
	allocator (
		size_index = 1,
		return_index = return,
		initial_state = uninitialized,
		if_size_is_0 = error,
		if_size_is_negative = error,
		if_out_of_memory = ok,
	),
	property (index = return,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_malloc"
	)
} "g_malloc"

beam::attribute_by_signature {
	allocator (
		size_index = 1,
		return_index = return,
		initial_state = initialized_to_zero,
		if_size_is_0 = error,
		if_size_is_negative = error,
		if_out_of_memory = ok,
	),
	property (index = return,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_malloc"
	)
} "g_malloc0"


beam::attribute_by_signature {
	deallocator (
		pointer_index = 1,
		resource = heap_memory
	),
	property (index = 1,
		type = requires,
		property_name = "memory allocation source",
		property_value = "from g_malloc"
	)
} "g_free"


beam::attribute_by_signature {
	allocator (
		return_index = return,
		initial_state = initialized_to_unknown,
		if_out_of_memory = ok,
		resource = heap_memory
	),
	property (index = return,
                num_dereference = 0,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_malloc"
	)
} "g_strdup"

#
#	Glib hash tables - GHashTable
#

beam::resource_create { 
    name = "GHashTable",
    display = "GHashTable",       
    allocating_verb = "creating",
    allocated_verb = "created",
    freeing_verb = "destroying",
    freed_verb = "destroyed"
}

beam::attribute_by_signature {
	allocator (
		return_index = return,
		if_out_of_memory = ok,
		initial_state = initialized_to_unknown,
		resource = GHashTable
	),
	property (index = return,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_hash_table_new"
	)
} "g_hash_table_new"

beam::attribute_by_signature {
	deallocator (
		pointer_index = 1,
		resource = GHashTable
	),
	property (index = 1,
		type = requires,
		property_name = "memory allocation source",
		property_value = "from g_hash_table_new"
	)
} "g_hash_table_destroy"

#
#	Glib doubly linked lists - GList
#

beam::resource_create { 
    name = "GList",
    display = "GList",       
    allocating_verb = "creating",
    allocated_verb = "created",
    freeing_verb = "destroying",
    freed_verb = "destroyed"
}

beam::attribute_by_signature {
	allocator (
		return_index = return,
		if_out_of_memory = ok,
		initial_state = initialized_to_unknown,
		resource = GList
	),
	property (index = return,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_list_alloc"
	)
} "g_list_alloc"

beam::attribute_by_signature {
	deallocator (
		pointer_index = 1,
		resource = GList
	),
	property (index = 1,
		type = requires,
		property_name = "memory allocation source",
		property_value = "from g_list_alloc"
	)
} "g_list_free" "g_list_free1"

#
#	Glib callback hooks - GHook
#

beam::resource_create { 
    name = "GHook",
    display = "GHook",       
    allocating_verb = "creating",
    allocated_verb = "created",
    freeing_verb = "destroying",
    freed_verb = "destroyed"
}
beam::attribute_by_signature {
	allocator (
		return_index = return,
		if_out_of_memory = ok,
		initial_state = initialized_to_unknown,
		resource = GHook
	),
	property (index = return,
                type = provides,
                property_name = "memory allocation source",
                property_value = "from g_hook_alloc"
	)
} "g_hook_alloc"

beam::attribute_by_signature {
	deallocator (
		pointer_index = 2,
		resource = GHook
	),
	property (index = 1,
                type = requires,
                property_name = "memory allocation source",
                property_value = "from g_hook_alloc"
	)
} "g_hook_free" "g_hook_unref"
