/* --- ais.h
  Header file of SA Forum AIS APIs Version 1.0
  In order to compile, all opaque types which appear as <...> in 
  the spec have been defined as OPAQUE_TYPE (which is an integer).
*/
#ifndef _AIS_H_
#define _AIS_H_

#define AIS_VERSION_RELEASE_CODE	'A'
#define AIS_VERSION_MAJOR		0x01
#define AIS_VERSION_MINOR		0x01

#include "ais_base.h"
#include "ais_amf.h"
#include "ais_membership.h"
#include "ais_checkpoint.h"
#include "ais_event.h"
#include "ais_lock.h"
#include "ais_message.h"

#endif
