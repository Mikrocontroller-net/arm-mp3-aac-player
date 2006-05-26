#include "systime.h"

#ifndef _PROFILE_H_
#define _PROFILE_H_

long profile_time;
const char *profile_name;

#define PROFILE_START(name)			\
do {								\
  profile_name = name;				\
  profile_time = systime_get();		\
} while(0)

#define PROFILE_END()				\
do {								\
  iprintf("%s: %li ms\n", profile_name, systime_get() - profile_time);	\
} while(0)

#endif /* _PROFILE_H_ */
