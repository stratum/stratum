// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_UTILS_H_
#define STRATUM_LIB_UTILS_H_

#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <chrono>  // NOLINT
#include <functional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"

namespace stratum {

// This is a simple stopwatch/timer class. The implementation is not threadsafe.
// TODO(unknown): Use internal Google3 timers when moved there.
class Timer {
 public:
  Timer() : started_(false) {}
  ~Timer() {}

  // Starts the stopwatch.
  void Start() {
    if (started_) return;
    t1_ = std::chrono::high_resolution_clock::now();
    t2_ = t1_;
    started_ = true;
  }

  // Stops the stopwatch.
  void Stop() {
    if (!started_) return;
    t2_ = std::chrono::high_resolution_clock::now();
    started_ = false;
  }

  // Return the duration between stop and start in milliseconds
  double Get() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_)
        .count();
  }

  // Timer is neither copyable nor movable.
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

 private:
  std::chrono::high_resolution_clock::time_point t1_;
  std::chrono::high_resolution_clock::time_point t2_;
  bool started_;
};

// Pretty prints an array of type T. T must have operator << overloaded. The
// separator between the elements in the returned string is given by 'sep'.
template <class T>
inline std::string PrintArray(const T* array, int count,
                              const std::string& sep) {
  std::stringstream buffer;
  std::string s = "";
  buffer << "(";
  for (int i = 0; i < count; ++i) {
    buffer << s << array[i];
    s = sep;
  }
  buffer << ")";

  return buffer.str();
}

// Pretty prints an vector of type T. T must have operator << overloaded. The
// separator between the elements in the returned string is given by 'sep'.
template <class T>
inline std::string PrintVector(const std::vector<T>& vec,
                               const std::string& sep) {
  return PrintArray<T>(vec.data(), vec.size(), sep);
}

template <typename T>
inline std::string PrintIterable(const T& iterable, const std::string& sep) {
  std::stringstream buffer;
  std::string s = "";
  buffer << "(";
  for (auto const& elem : iterable) {
    buffer << s << elem;
    s = sep;
  }
  buffer << ")";

  return buffer.str();
}

// Writes a proto message in binary format to the given file path.
::util::Status WriteProtoToBinFile(const ::google::protobuf::Message& message,
                                   const std::string& filename);

// Reads proto from a file containing the proto message in binary format.
::util::Status ReadProtoFromBinFile(const std::string& filename,
                                    ::google::protobuf::Message* message);

// Writes a proto message in text format to the given file path.
::util::Status WriteProtoToTextFile(const ::google::protobuf::Message& message,
                                    const std::string& filename);

// Reads proto from a text file containing the proto message in text format.
::util::Status ReadProtoFromTextFile(const std::string& filename,
                                     ::google::protobuf::Message* message);

// Serializes proto to a string. Wrapper around TextFormat::PrintToString().
::util::Status PrintProtoToString(const ::google::protobuf::Message& message,
                                  std::string* text);

// Parses a proto from a string. Wrapper around TextFormat::ParseFromString().
::util::Status ParseProtoFromString(const std::string& text,
                                    ::google::protobuf::Message* message);

// Writes a string buffer to a text file. 'append' (default false) specifies
// whether the string need to appended to the end of the file as opposed to
// truncating the file contents. The default is false.
::util::Status WriteStringToFile(const std::string& buffer,
                                 const std::string& filename,
                                 bool append = false);

// Reads the contents of a file to a string buffer.
::util::Status ReadFileToString(const std::string& filename,
                                std::string* buffer);

// A helper to convert a string containing binary data to a ASCII string
// presenting the data in hex format.
std::string StringToHex(const std::string& str);

// Creates the given dir and do to do that creates all the parent dirs first.
::util::Status RecursivelyCreateDir(const std::string& dir);

// Removes a file from the given path. Returns error if the file does not exist
// or the path is a dir.
::util::Status RemoveFile(const std::string& path);

// Checks to see if a path exists.
inline bool PathExists(const std::string& path) {
  struct stat stbuf;
  return (stat(path.c_str(), &stbuf) >= 0);
}

// Checks to see if a path is a dir.
inline bool IsDir(const std::string& path) {
  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) < 0) {
    return false;
  }
  return S_ISDIR(stbuf.st_mode);
}

// Breaks a path string into directory and filename components and returns the
// directory.
inline std::string DirName(const std::string& path) {
  char* path_str = strdup(path.c_str());
  std::string dir = dirname(path_str);
  free(path_str);
  return dir;
}

// Breaks a path string into directory and filename components and returns the
// filename (aka basename).
inline std::string BaseName(const std::string& path) {
  char* path_str = strdup(path.c_str());
  std::string base = basename(path_str);
  free(path_str);
  return base;
}

// Serializes the proto into a string in a deterministic way, i.e., ensures
// that for any two proto messages m1 and m2, if m1 == m2 the corresponding
// serialized strings are always the same. Note that m1 == m2 means m1 and m2
// are equal (not equivalent). Note that for equality the order of repeated
// fields are important.
inline std::string ProtoSerialize(const google::protobuf::Message& m) {
  const size_t size = m.ByteSizeLong();
  std::string out;
  out.resize(size);
  char* out_buf = out.empty() ? nullptr : &(*out.begin());
  ::google::protobuf::io::ArrayOutputStream array_out_stream(
      out_buf, static_cast<int>(size));
  ::google::protobuf::io::CodedOutputStream coded_out_stream(&array_out_stream);
  coded_out_stream.SetSerializationDeterministic(true);
  m.SerializeWithCachedSizes(&coded_out_stream);
  return out;
}

// Compares two protos m1 and m2 and returns true if m1 == m2, but ignores the
// order of repeated fields. In other words checks for equality (not
// equivalence) of the protos assuming that the order of the repeated fields
// are not important.
inline bool ProtoEqual(const google::protobuf::Message& m1,
                       const google::protobuf::Message& m2) {
  ::google::protobuf::util::MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);
  return differencer.Compare(m1, m2);
}

// Hash functor used in hash maps or hash sets with enums used as key.
template <typename T>
struct EnumHash {
  size_t operator()(const T& x) const { return static_cast<size_t>(x); }
};

// Helper for converting an int error code to a GRPC canonical error code.
constexpr ::grpc::StatusCode kGrpcCodeMin = ::grpc::StatusCode::OK;
constexpr ::grpc::StatusCode kGrpcCodeMax = ::grpc::StatusCode::UNAUTHENTICATED;
inline ::grpc::StatusCode ToGrpcCode(int from) {
  ::grpc::StatusCode code = ::grpc::StatusCode::UNKNOWN;
  if (from >= kGrpcCodeMin && from <= kGrpcCodeMax) {
    code = static_cast<::grpc::StatusCode>(from);
  }
  return code;
}

// Helper for converting an int error code to a Google RPC canonical error code.
constexpr ::google::rpc::Code kGoogleRpcCodeMin = ::google::rpc::OK;
constexpr ::google::rpc::Code kGoogleRpcCodeMax =
    ::google::rpc::UNAUTHENTICATED;
inline ::google::rpc::Code ToGoogleRpcCode(int from) {
  ::google::rpc::Code code = ::google::rpc::UNKNOWN;
  if (from >= kGoogleRpcCodeMin && from <= kGoogleRpcCodeMax) {
    code = static_cast<::google::rpc::Code>(from);
  }
  return code;
}

// This function takes an unsigned integer encoded as string data and
// converts it to the desired unsigned type.  The bytes in the string are
// assumed to be in network byte order.  The conversion is truncated if the
// number of input bytes is too large for the output.  The typename U must
// be at least 16 bits wide.
template <typename U>
inline U ByteStreamToUint(const std::string& bytes) {
  U val = 0;
  for (size_t i = 0; i < bytes.size() && i < sizeof(U); ++i) {
    val <<= 8;
    val += static_cast<uint8>(bytes[i]);
  }
  return val;
}

// Demangles a symbol name, if possible. If it fails, the mangled name is
// returned instead.
// Not async-safe, do not use this function inside a signal handler!
std::string Demangle(const char* mangled);

}  // namespace stratum

#endif  // STRATUM_LIB_UTILS_H_
