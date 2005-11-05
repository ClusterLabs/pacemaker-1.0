############################################################
# Invocation of beam_configure:
#
#   '/home/alanr/beam-3.0.1/bin/beam_configure' '--output' 'c-config-i686-pc-linux-gnu.tcl' '--c' '/usr/bin/gcc'
#
# Location of compiler:
#
#   /usr/bin/gcc
#
############################################################
#
# This is BEAM configuration file that describes a compiler
# and a target machine. This was generated with beam_configure
# version "1.1 (June 2005)".
#
# This information will help BEAM emulate this compiler's
# features, macros, and header file locations, so that BEAM
# can compile the same source code that the original compiler
# could compile, and understand it with respect to the machine's
# sizes and widths of types.
#
# The file format is Tcl, so basic Tcl knowledge may be beneficial
# for anything more than the simplest of modifications.
# 
# A quick Tcl primer:
# - Lines starting with "#" or ";#" are comments
# - Things inside balanced curly braces are literal strings {one string literal}
# - Things in square brackets that aren't in curly braces are function calls,
#   and will be expanded inline automatically. This causes the most problems in
#   double-quoted strings: "this is a function call: [some_func]"
#
# This file contains these sections:
#
# 1) Source language dialect
# 2) Default include paths
# 3) Target machine configuration
# 4) Predefined macros
# 5) Miscellaneous options
#
# Each section has variables that help configure BEAM. They should
# each be commented well. For additional documentation, please
# refer to the local documentation in the install point.
#
# Note that the order of the sections is not important,
# and variables may be set in any order.
#
############################################################

### This sets up the namespace that the rest of the file will
### live in. BEAM will look in certain namespaces depending
### on the source file that is being compiled.
###
### For C compilers, set the namespace to "::beam::compiler::c"
### For C++, set it to "::beam::compiler::cpp"

# set ::namesp ::beam::compiler::c
# set ::namesp ::beam::compiler::cpp

set ::namesp ::beam::compiler::c

namespace eval $::namesp {
  
### This should be first. It initializes the namespace
### correctly for BEAM's use.
  
::beam::compiler::init_settings

### This tells BEAM which pre-canned settings to load.
### BEAM comes with some function attributes and argument
### mappers for gcc, xlc, and vac. If unsure, set this to
### "default".

set cc "gcc"
  
############################################################
# Section 1: Source language dialect
############################################################
  
### The language_dialect variable selects among the available
### dialects of C and C++.
###
### By default, C files are set up as:
###
###    set language_dialect c
###    set c99_mode         0
###    set strict_mode      0
###    set gnu_mode         0
###
### and C++ files are set up as:
###
###    set language_dialect c++
###    set c99_mode         0
###    set strict_mode      0
###    set gnu_mode         0
###
### Note that the dialect must match the namespace.
### Don't set up the C++ language in the C namespace or
### things will probably fail.
###
### This defaults to be the same as the language being
### compiled (based on the source file extension).
### Normally, it should not be set.

# set language_dialect old_c ;# K&R
# set language_dialect c     ;# ANSI
# set language_dialect c++

### In addition to simply using C or C++, different
### modes are provided to enable or disable language
### extensions. Some modes are incompatible with eachother
### or with the language_dialect above, and will produce
### errors.

### C99 mode enables C99 extensions in C code. It is not
### compatible with C++ code. This overrides old_c, and
### instead forces regular C.

# set c99_mode 0
# set c99_mode 1

### Strict mode disables all non-ANSI/ISO features. It
### is compatible with C and C++ code, but not with old_c.
  
# set strict_mode 0
# set strict_mode 1

### GNU mode enables GNU C extensions in C code and
### GNU C++ extensions in C++ code. This overrides
### old_c, and instead forces regular C.
###
### The value should be a 5 digit number representing
### the version of GCC to emulate. It is of this format:
###
###    major_version_num * 10000 +
###    minor_version_num * 100   +
###    patch_version_num
###
### so, GCC version "3.4.3" should be "30403".
###
### The minimum allowable value is "30200".

# set gnu_mode 30200

set gnu_mode 30305

### Other miscellaneous language settings. The values shown
### here are the defaults if they remain unset.

# set language_friend_injection_enabled       0
# set language_use_nonstandard_for_init_scope 0
# set language_string_literals_are_const      1
# set language_allow_dollar_in_id_chars       1
# set language_end_of_line_comments_allowed   0
# set language_allow_spaces_in_include_directive 0


  
############################################################
# Section 2: Default include paths
############################################################

### The system_include_path variable is a list of directories
### that will be searched in for system headers. Parser warnings
### are suppressed in these directories. These will come
### after any directories specified with -I on the command line.
  
# lappend system_include_path {/usr/include}
# lappend system_include_path {/usr/vacpp/include}

### Maybe your include paths are part of the environment
  
# if { [::info exists ::env(MY_INCLUDE_PATH)] } {
#   set system_include_path [concat $system_include_path \
#                             [split $::env(MY_INCLUDE_PATH) ":"]]
# }
  
lappend system_include_path {/usr/lib/gcc-lib/i586-suse-linux/3.3.5/include}
lappend system_include_path {/usr/include}

  
############################################################
# Section 3: Target machine configuration
############################################################

### These variables control the target machine and
### a few individual language options.
###
### Note: These examples do not cover all of the available
### options. For a complete list, refer to the BEAM documentation.
###
### Examples appear below the auto-configured ones.
  
set target_alignof_double                         {4}
set target_alignof_float                          {4}
set target_alignof_int                            {4}
set target_alignof_long                           {4}
set target_alignof_long_double                    {4}
set target_alignof_long_long                      {4}
set target_alignof_pointer                        {4}
set target_alignof_short                          {2}
set target_char_bit                               {8}
set target_dbl_max_exp                            {1024}
set target_dbl_min_exp                            {-1021}
set target_enum_bit_fields_are_always_unsigned    {0}
set target_flt_max_exp                            {128}
set target_flt_min_exp                            {-125}
set target_ldbl_max_exp                           {16384}
set target_ldbl_min_exp                           {-16381}
set target_little_endian                          {1}
set target_plain_char_is_unsigned                 {0}
set target_plain_int_bit_field_is_unsigned        {0}
set target_size_t_int_kind                        {unsigned int}
set target_sizeof_double                          {8}
set target_sizeof_float                           {4}
set target_sizeof_int                             {4}
set target_sizeof_long                            {4}
set target_sizeof_long_double                     {12}
set target_sizeof_long_long                       {8}
set target_sizeof_pointer                         {4}
set target_sizeof_short                           {2}
set target_sizeof_size_t                          {4}
set target_sizeof_wchar_t                         {4}
set target_wchar_t_int_kind                       {long int}
set target_wchar_t_is_unsigned                    {0}

  
### Examples ###

### The number of bits in a char

# set target_char_bit 8
    
### Default signedness options

# set target_plain_char_is_unsigned              0
# set target_plain_char_is_unsigned              1
#
# set target_plain_int_bit_field_is_unsigned     0
# set target_plain_int_bit_field_is_unsigned     1
#
# set target_enum_bit_fields_are_always_unsigned 0
# set target_enum_bit_fields_are_always_unsigned 1

### Endianness of target machine

# set target_little_endian 0    
# set target_little_endian 1

### Sizes of basic types in multiples of char. Since
### a char is defined to have size 1, it is not a
### configuration option.

# set target_sizeof_short 2
# set target_sizeof_int 4
# set target_sizeof_long 4
# set target_sizeof_long_long 8    
# set target_sizeof_float 4
# set target_sizeof_double 8
# set target_sizeof_long_double 12
# set target_sizeof_pointer 4

### Alignments of basic types in multiples of char. Since
### a char is defined to have alignment 1, it is not a
### configuration option.

# set target_alignof_short 2
# set target_alignof_int 4
# set target_alignof_long 4
# set target_alignof_long_long 4
# set target_alignof_float 4
# set target_alignof_double 4
# set target_alignof_long_double 4
# set target_alignof_pointer 4
    
### Special types

# set target_sizeof_size_t     4
# set target_size_t_int_kind   {unsigned int}
#
# set target_sizeof_wchar_t      4
# set target_wchar_t_int_kind    {int}
# set target_wchar_t_is_unsigned 0

### Floating-point characteristics. The default
### values for these variables depend on the sizes
### set for the types. The examples shown here
### are appropriate if float is size 4, double is
### size 8, and long double is size 12.
###
### Note that these values do not have to be exact
### because BEAM currently has limited floating-point
### support.

# set target_flt_max_exp 128
# set target_flt_min_exp -125
# set target_dbl_max_exp 1024
# set target_dbl_min_exp -1021
# set target_ldbl_max_exp 16384
# set target_ldbl_min_exp -16381

### Other miscellaneous options. The values
### shown here are the default values.

# set target_bit_field_container_size -1
# set target_zero_width_bit_field_alignment -1
# set target_zero_width_bit_field_affects_struct_alignment 0
# set target_unnamed_bit_field_affects_struct_alignment 0
  
############################################################
# Section 4: Predefined macros
############################################################

### The predefined_macro variable is an associated array that
### maps the name of a macro to the value. Be sure that the
### value contains quotes inside the curly braces if the
### expansion should also contain quotes.
###
### Curly braces are allowed in the expansion text as long
### as they are properly balanced.
###
### There is no limit to the number of predefined macros that
### you can define.

# set predefined_macro(identifier1)      {some_literal_value}
# set predefined_macro(identifier2)      {"some string value with quotes"}
# set predefined_macro(identifier3(x,y)) { do { code; } while((x) && (y)) }

set predefined_macro(__BEAM__) {1}

set predefined_macro(__CHAR_BIT__)                {8}
set predefined_macro(__DBL_DIG__)                 {15}
set predefined_macro(__DBL_EPSILON__)             {2.2204460492503131e-16}
set predefined_macro(__DBL_MANT_DIG__)            {53}
set predefined_macro(__DBL_MAX_10_EXP__)          {308}
set predefined_macro(__DBL_MAX_EXP__)             {1024}
set predefined_macro(__DBL_MAX__)                 {1.7976931348623157e+308}
set predefined_macro(__DBL_MIN_10_EXP__)          {(-307)}
set predefined_macro(__DBL_MIN_EXP__)             {(-1021)}
set predefined_macro(__DBL_MIN__)                 {2.2250738585072014e-308}
set predefined_macro(__DECIMAL_DIG__)             {21}
set predefined_macro(__FLT_DIG__)                 {6}
set predefined_macro(__FLT_EPSILON__)             {1.19209290e-7F}
set predefined_macro(__FLT_EVAL_METHOD__)         {2}
set predefined_macro(__FLT_MANT_DIG__)            {24}
set predefined_macro(__FLT_MAX_10_EXP__)          {38}
set predefined_macro(__FLT_MAX_EXP__)             {128}
set predefined_macro(__FLT_MAX__)                 {3.40282347e+38F}
set predefined_macro(__FLT_MIN_10_EXP__)          {(-37)}
set predefined_macro(__FLT_MIN_EXP__)             {(-125)}
set predefined_macro(__FLT_MIN__)                 {1.17549435e-38F}
set predefined_macro(__FLT_RADIX__)               {2}
set predefined_macro(__GNUC_MINOR__)              {3}
set predefined_macro(__GNUC_PATCHLEVEL__)         {5}
set predefined_macro(__GNUC__)                    {3}
set predefined_macro(__INT_MAX__)                 {2147483647}
set predefined_macro(__LDBL_DIG__)                {18}
set predefined_macro(__LDBL_EPSILON__)            {1.08420217248550443401e-19L}
set predefined_macro(__LDBL_MANT_DIG__)           {64}
set predefined_macro(__LDBL_MAX_10_EXP__)         {4932}
set predefined_macro(__LDBL_MAX_EXP__)            {16384}
set predefined_macro(__LDBL_MAX__)                {1.18973149535723176502e+4932L}
set predefined_macro(__LDBL_MIN_10_EXP__)         {(-4931)}
set predefined_macro(__LDBL_MIN_EXP__)            {(-16381)}
set predefined_macro(__LDBL_MIN__)                {3.36210314311209350626e-4932L}
set predefined_macro(__LONG_LONG_MAX__)           {9223372036854775807LL}
set predefined_macro(__LONG_MAX__)                {2147483647L}
set predefined_macro(__NO_INLINE__)               {1}
set predefined_macro(__PTRDIFF_TYPE__)            {int}
set predefined_macro(__SCHAR_MAX__)               {127}
set predefined_macro(__SHRT_MAX__)                {32767}
set predefined_macro(__SIZE_TYPE__)               {unsigned int}
set predefined_macro(__WCHAR_TYPE__)              {long int}
set predefined_macro(__WINT_TYPE__)               {unsigned int}
set predefined_macro(__i386)                      {1}
set predefined_macro(__i386__)                    {1}
set predefined_macro(__linux__)                   {1}
set predefined_macro(__unix)                      {1}
set predefined_macro(__unix__)                    {1}
set predefined_macro(i386)                        {1}
set predefined_macro(linux)                       {1}
set predefined_macro(unix)                        {1}


### You can also suppress the standard EDG predefined macros
### like __STDC__ if you set this pattern. By default,
### the pattern is "*", which allows all EDG predefined
### macros to get defined. Setting this to something
### like "* - __STDC__" would suppress the __STDC__
### macro from being defined by default. This does
### not affect any predefined macros set up in this
### file; it only affects the basic EDG predefined macros.
  
# set standard_predefined_macros "*"
  
############################################################
# Section 5: Miscellaneous options
############################################################

### The extern variable is an associated array that maps
### unknown extern "string" values to known ones. For example,
### to force BEAM to treat
###
###   extern "builtin" void func();
###
### as
###
###   extern "C" void func();
###
### you should set this option:
###
###   set extern(builtin) "C"
###
### There is no limit to the number of strings that you can
### map to the built-in strings of "C" or "C++".
  


### Some compilers define macro-like symbols that are being replaced
### with the name of the function they appear in. Below are the symbols
### EDG recognizes. Set to 1, if the symbol is replaced with a character
### string (as opposed to a variable). If in doubt define it as "1"
### which is more flexible.
###
### set function_name_is_string_literal(__PRETTY_FUNCTION__) 1
### set function_name_is_string_literal(__FUNCTION__) 1
### set function_name_is_string_literal(__FUNCDNAME__) 1
### set function_name_is_string_literal(__func__)     1

set function_name_is_string_literal(__func__) 0
set function_name_is_string_literal(__FUNCTION__) 1
set function_name_is_string_literal(__PRETTY_FUNCTION__) 1


############################################################
# End of the file and end of the namespace
############################################################

}

