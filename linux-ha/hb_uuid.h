/*
 * uuid: wrapper declarations.
 *
 *	heartbeat originally used "uuid" functionality by calling directly,
 *	and only, onto the "e2fsprogs" implementation.
 *
 *	The run-time usages in the code have since been abstracted, funnelled
 *	through a thin, common interface layer: a Good Thing.
 *
 *	Similarly, the compile-time usages of "include <uuid/uuid.h>" are
 *	replaced, being funnelled through a reference to this header file.
 *
 *	This header file interfaces onto the actual underlying implementation.
 *	In the case of the "e2fsprogs" implementation, it is simply a stepping
 *	stone onto "<uuid/uuid.h>".  As other implementations are accommodated,
 *	so their header requirements can be accommodated here.
 *
 * Copyright (C) 2004 David Lee <t.d.lee@durham.ac.uk>
 */

#ifndef HB_UUID_H
#define HB_UUID_H

#if defined (HAVE_UUID_UUID_H)
/*
 * Almost certainly the "e2fsprogs" implementation.
 */

# include <uuid/uuid.h>

/* elif defined(...UUID_OTHER_1 e.g. OSSP ...) */

/* elif defined(...UUID_OTHER_2...) */

#else
/*
 * Local "replace" implementation.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

/* UUID Variant definitions */
#define UUID_VARIANT_NCS	0
#define UUID_VARIANT_DCE	1
#define UUID_VARIANT_MICROSOFT	2
#define UUID_VARIANT_OTHER	3

/* UUID Type definitions */  
#define UUID_TYPE_DCE_TIME	1
#define UUID_TYPE_DCE_RANDOM	4

typedef unsigned char uuid_t[16];

void uuid_clear(uuid_t uu);

int uuid_compare(const uuid_t uu1, const uuid_t uu2);

void uuid_copy(uuid_t dst, const uuid_t src);

void uuid_generate(uuid_t out);
void uuid_generate_random(uuid_t out);
void uuid_generate_time(uuid_t out);

int uuid_is_null(const uuid_t uu);

int uuid_parse(const char *in, uuid_t uu);

void uuid_unparse(const uuid_t uu, char *out);

time_t uuid_time(const uuid_t uu, struct timeval *ret_tv);
int uuid_type(const uuid_t uu);
int uuid_variant(const uuid_t uu);

#endif /* HAVE_UUID_UUID_H */

#endif /* HB_UUID_H */
