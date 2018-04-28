/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_LIB_UTILS_H_
#define STRATUM_LIB_UTILS_H_

#include <grpcpp/grpcpp.h>
#include <chrono>  // NOLINT
#include <functional>
#include <sstream>  // IWYU pragma: keep
#include <string>

#include "google/protobuf/message.h"
#include "third_party/absl/base/integral_types.h"
#include "third_party/stratum/glue/status/status.h"

namespace stratum {

// Port of boost::hash_combine for internal use (Hercules does not need boost).
template <typename T>
inline void HashCombine(size_t* seed, const T& v) {
  std::hash<T> hasher;
  *seed ^= hasher(v) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

// Custom hash function for enums that can be converted to size_t.
template <typename T>
struct EnumHash {
  size_t operator()(const T& x) const { return static_cast<size_t>(x); }
};

// Custom hash for a std::pair of two types with predifed hash.
template <typename T, typename U>
struct PairHash {
  size_t operator()(const std::pair<T, U>& p) const {
    size_t seed = 0;
    HashCombine<T>(&seed, p.first);
    HashCombine<U>(&seed, p.second);
    return seed;
  }
};

// This is a simple stopwatch/timer class. The implementation is not threadsafe.
// TODO: Use internal Google3 timers when moved there.
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
bool PathExists(const std::string& path);

// Checks to see if a path is a dir.
bool IsDir(const std::string& path);

// Break a path string into directory and filename components and return them.
std::string DirName(const std::string& path);
std::string BaseName(const std::string& path);

// Compares two protos m1 and m2 and returns true if m1 < m2. This method does
// a simple comparison on SerializeAsString output of the protos.
bool ProtoLess(const ::google::protobuf::Message& m1,
               const ::google::protobuf::Message& m2);

// Compares two protos m1 and m2 and returns true if m1 == m2. This method does
// a simple comparison on SerializeAsString output of the protos and cannot
// handle the case where the order of repeated fields are not important for
// example.
bool ProtoEqual(const ::google::protobuf::Message& m1,
                const ::google::protobuf::Message& m2);

// Custom hash function for proto messages.
size_t ProtoHash(const ::google::protobuf::Message& m);

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
U ByteStreamToUint(const std::string& bytes) {
  U val = 0;
  for (size_t i = 0; i < bytes.size() && i < sizeof(U); ++i) {
    val <<= 8;
    val += static_cast<uint8>(bytes[i]);
  }
  return val;
}

}  // namespace stratum

#endif  // STRATUM_LIB_UTILS_H_
