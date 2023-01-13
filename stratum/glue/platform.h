// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_PLATFORM_H_
#define STRATUM_GLUE_PLATFORM_H_

#include <signal.h>

#ifdef __APPLE__

// Fake missing error codes on MacOS. Note, that they still have to be unique
// and must not overlap with existing ones.
#ifndef ENOMEDIUM
#define ENOMEDIUM (ELAST + __COUNTER__)
#endif

#ifndef ENOTUNIQ
#define ENOTUNIQ (ELAST + __COUNTER__)
#endif

#ifndef ENOKEY
#define ENOKEY (ELAST + __COUNTER__)
#endif

#ifndef EBADFD
#define EBADFD (ELAST + __COUNTER__)
#endif

#ifndef EISNAM
#define EISNAM (ELAST + __COUNTER__)
#endif

#ifndef EUNATCH
#define EUNATCH (ELAST + __COUNTER__)
#endif

#ifndef ECHRNG
#define ECHRNG (ELAST + __COUNTER__)
#endif

#ifndef ENOPKG
#define ENOPKG (ELAST + __COUNTER__)
#endif

#ifndef ECOMM
#define ECOMM (ELAST + __COUNTER__)
#endif

#ifndef ENONET
#define ENONET (ELAST + __COUNTER__)
#endif

#ifndef EBADE
#define EBADE (ELAST + __COUNTER__)
#endif

#ifndef EBADR
#define EBADR (ELAST + __COUNTER__)
#endif

#ifndef EBADRQC
#define EBADRQC (ELAST + __COUNTER__)
#endif

#ifndef EBADSLT
#define EBADSLT (ELAST + __COUNTER__)
#endif

#ifndef EKEYEXPIRED
#define EKEYEXPIRED (ELAST + __COUNTER__)
#endif

#ifndef EKEYREJECTED
#define EKEYREJECTED (ELAST + __COUNTER__)
#endif

#ifndef EKEYREVOKED
#define EKEYREVOKED (ELAST + __COUNTER__)
#endif

#ifndef EL2HLT
#define EL2HLT (ELAST + __COUNTER__)
#endif

#ifndef EL2NSYNC
#define EL2NSYNC (ELAST + __COUNTER__)
#endif

#ifndef EL3HLT
#define EL3HLT (ELAST + __COUNTER__)
#endif

#ifndef EL3RST
#define EL3RST (ELAST + __COUNTER__)
#endif

#ifndef ELIBACC
#define ELIBACC (ELAST + __COUNTER__)
#endif

#ifndef ELIBBAD
#define ELIBBAD (ELAST + __COUNTER__)
#endif

#ifndef ELIBMAX
#define ELIBMAX (ELAST + __COUNTER__)
#endif

#ifndef ELIBSCN
#define ELIBSCN (ELAST + __COUNTER__)
#endif

#ifndef ELIBEXEC
#define ELIBEXEC (ELAST + __COUNTER__)
#endif

#ifndef EMEDIUMTYPE
#define EMEDIUMTYPE (ELAST + __COUNTER__)
#endif

#ifndef EREMOTEIO
#define EREMOTEIO (ELAST + __COUNTER__)
#endif

#ifndef ERESTART
#define ERESTART (ELAST + __COUNTER__)
#endif

#ifndef ESTRPIPE
#define ESTRPIPE (ELAST + __COUNTER__)
#endif

#ifndef EUCLEAN
#define EUCLEAN (ELAST + __COUNTER__)
#endif

#ifndef EXFULL
#define EXFULL (ELAST + __COUNTER__)
#endif

// Typedef aliases for members in struct in6_addr.
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32

// Typedef for compatability with Linux's sighandler_t.
typedef sig_t sighandler_t;

#endif  // __APPLE__

#endif  // STRATUM_GLUE_PLATFORM_H_
