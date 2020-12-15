// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/glue/status/posix_error_space.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include <string>

#include "stratum/glue/logging.h"

// Fake missing error codes on MacOS. Note, that they still have to be unique
// and must not overlap with existing ones.
#ifdef __APPLE__
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
#endif  // __APPLE__

using google::StrError;

namespace util {

namespace internal {

class PosixErrorSpace : public util::ErrorSpace {
 public:
  PosixErrorSpace();
  virtual ~PosixErrorSpace();

  // Returns the message associated with the given code in this error
  // space. This is basically a call to strerror_r.
  std::string String(int code) const final;

  ::util::error::Code CanonicalCode(const ::util::Status& status) const final;

  // PosixErrorSpace is neither copyable nor movable.
  PosixErrorSpace(const PosixErrorSpace&) = delete;
  PosixErrorSpace& operator=(const PosixErrorSpace&) = delete;
};

PosixErrorSpace::PosixErrorSpace() : ErrorSpace("util::PosixErrorSpace") {}

PosixErrorSpace::~PosixErrorSpace() {}

// TODO(unknown): Move to glog/StrError().
std::string PosixErrorSpace::String(int code) const { return StrError(code); }

::util::error::Code PosixErrorSpace::CanonicalCode(
    const ::util::Status& status) const {
  ::util::error::Code code;

  switch (status.error_code()) {
    case 0:
      code = ::util::error::OK;
      break;
    case EINVAL:        // Invalid argument
    case ENAMETOOLONG:  // Filename too long
    case E2BIG:         // Argument list too long
    case EDESTADDRREQ:  // Destination address required
    case EDOM:          // Mathematics argument out of domain of function
    case EFAULT:        // Bad address
    case EILSEQ:        // Illegal byte sequence
    case ENOPROTOOPT:   // Protocol not available
    case ENOSTR:        // Not a STREAM
    case ENOTSOCK:      // Not a socket
    case ENOTTY:        // Inappropriate I/O control operation
    case EPROTOTYPE:    // Protocol wrong type for socket
    case ESPIPE:        // Invalid seek
      code = ::util::error::INVALID_ARGUMENT;
      break;
    case ETIMEDOUT:  // Connection timed out
    case ETIME:      // Timer expired
      code = ::util::error::DEADLINE_EXCEEDED;
      break;
    case ENODEV:     // No such device
    case ENOENT:     // No such file or directory
    case ENOMEDIUM:  // No medium found
    case ENXIO:      // No such device or address
    case ESRCH:      // No such process
      code = ::util::error::NOT_FOUND;
      break;
    case EEXIST:         // File exists
    case EADDRNOTAVAIL:  // Address not available
    case EALREADY:       // Connection already in progress
    case ENOTUNIQ:       // Name not unique on network
      code = ::util::error::ALREADY_EXISTS;
      break;
    case EPERM:   // Operation not permitted
    case EACCES:  // Permission denied
    case ENOKEY:  // Required key not available
    case EROFS:   // Read only file system
      code = ::util::error::PERMISSION_DENIED;
      break;
    case ENOTEMPTY:   // Directory not empty
    case EISDIR:      // Is a directory
    case ENOTDIR:     // Not a directory
    case EADDRINUSE:  // Address already in use
    case EBADF:       // Invalid file descriptor
    case EBADFD:      // File descriptor in bad state
    case EBUSY:       // Device or resource busy
    case ECHILD:      // No child processes
    case EISCONN:     // Socket is connected
    case EISNAM:      // Is a named type file
    case ENOTBLK:     // Block device required
    case ENOTCONN:    // The socket is not connected
    case EPIPE:       // Broken pipe
    case ESHUTDOWN:   // Cannot send after transport endpoint shutdown
    case ETXTBSY:     // Text file busy
    case EUNATCH:     // Protocol driver not attached
      code = ::util::error::FAILED_PRECONDITION;
      break;
    case ENOSPC:   // No space left on device
    case EDQUOT:   // Disk quota exceeded
    case EMFILE:   // Too many open files
    case EMLINK:   // Too many links
    case ENFILE:   // Too many open files in system
    case ENOBUFS:  // No buffer space available
    case ENODATA:  // No message is available on the STREAM read queue
    case ENOMEM:   // Not enough space
    case ENOSR:    // No STREAM resources
    case EUSERS:   // Too many users
      code = ::util::error::RESOURCE_EXHAUSTED;
      break;
    case ECHRNG:     // Channel number out of range
    case EFBIG:      // File too large
    case EOVERFLOW:  // Value too large to be stored in data type
    case ERANGE:     // Result too large
      code = ::util::error::OUT_OF_RANGE;
      break;
    case ENOPKG:           // Package not installed
    case ENOSYS:           // Function not implemented
    case ENOTSUP:          // Operation not supported
    case EAFNOSUPPORT:     // Address family not supported
    case EPFNOSUPPORT:     // Protocol family not supported
    case EPROTONOSUPPORT:  // Protocol not supported
    case ESOCKTNOSUPPORT:  // Socket type not supported
    case EXDEV:            // Improper link
      code = ::util::error::UNIMPLEMENTED;
      break;
    case EAGAIN:        // Resource temporarily unavailable
    case ECOMM:         // Communication error on send
    case ECONNREFUSED:  // Connection refused
    case ECONNABORTED:  // Connection aborted
    case ECONNRESET:    // Connection reset
    case EINTR:         // Interrupted function call
    case EHOSTDOWN:     // Host is down
    case EHOSTUNREACH:  // Host is unreachable
    case ENETDOWN:      // Network is down
    case ENETRESET:     // Connection aborted by network
    case ENETUNREACH:   // Network unreachable
    case ENOLCK:        // No locks available
    case ENOLINK:       // Link has been severed
    case ENONET:        // Machine is not on the network
      code = ::util::error::UNAVAILABLE;
      break;
    case EDEADLK:  // Resource deadlock avoided
    case ESTALE:   // Stale file handle
      code = ::util::error::ABORTED;
      break;
    case ECANCELED:  // Operation cancelled
      code = ::util::error::CANCELLED;
      break;
    // NOTE: If you get any of the following (especially in a
    // reproducable way) and can propose a better mapping,
    // please email the owners about updating this mapping.
    case EBADE:         // Bad exchange
    case EBADMSG:       // Bad message
    case EBADR:         // Invalid request descriptor
    case EBADRQC:       // Invalid request code
    case EBADSLT:       // Invalid slot
    case EIDRM:         // Identifier removed
    case EINPROGRESS:   // Operation in progress
    case EIO:           // I/O error
    case EKEYEXPIRED:   // Key has expired
    case EKEYREJECTED:  // Key was rejected by service
    case EKEYREVOKED:   // Key has been revoked
    case EL2HLT:        // Level 2 halted
    case EL2NSYNC:      // Level 2 not synchronised
    case EL3HLT:        // Level 3 halted
    case EL3RST:        // Level 3 reset
    case ELIBACC:       // Cannot access a needed shared library
    case ELIBBAD:       // Acccessing a corrupted shared library
    case ELIBMAX:       // Attempting to link in too many shared libraries
    case ELIBSCN:       // lib section in a.out corrupted
    case ELIBEXEC:      // Cannot exec a shared library directly
    case ELOOP:         // Too many levels of symbolic links
    case EMEDIUMTYPE:   // Wrong medium type
    case ENOEXEC:       // Exec format error
    case ENOMSG:        // No message of the desired type
    case EPROTO:        // Protocol error
    case EREMOTE:       // Object is remote
    case EREMOTEIO:     // Remote I/O error
    case ERESTART:      // Interrupted system call should be restarted
    case ESTRPIPE:      // Streams pipe error
    case EUCLEAN:       // Structure needs cleaning
    case EXFULL:        // Exchange full
      code = ::util::error::UNKNOWN;
      break;
    default: {
      code = ::util::error::UNKNOWN;
      break;
    }
  }
  return code;
}

}  // namespace internal

const ErrorSpace* PosixErrorSpace() {
  static const ErrorSpace* space = new internal::PosixErrorSpace();
  return space;
}

// Force accessor to run at load time, which registers the errorspace.
static const ErrorSpace* dummy __attribute__((unused)) =
    util::PosixErrorSpace();

}  // namespace util
