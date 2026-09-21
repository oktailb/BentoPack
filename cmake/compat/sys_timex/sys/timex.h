#pragma once
/* Compatibility header for platforms without <sys/timex.h> (such as Haiku OS).
 * basis_universal includes <sys/timex.h> under __GNUC__ purely for gettimeofday(),
 * which is provided by <sys/time.h>.
 */
#include <sys/time.h>
