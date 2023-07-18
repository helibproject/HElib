/* Copyright (C) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STREAM_DISPENSER_H_
#define STREAM_DISPENSER_H_

#include<functional>
#include<tuple>
#include<memory>

// Do not use this class directly. Create instances using the
// make_stream_dispenser function
// eg. make_stream_dispenser<std::ifstream>("filename", std::ios::binary)
template<typename Stream, typename... Args>
class StreamDispenser {
 private:
  std::tuple<Args...> args;

 public:
  StreamDispenser(Args... args): args(std::make_tuple(args...)) {}

  std::unique_ptr<Stream> get() const {
    return std::apply([](const Args... args){
        return std::make_unique<Stream>(args...);
      }, args);
  }
};

template<typename Stream, typename... Args>
inline auto make_stream_dispenser(Args... args) {
  return StreamDispenser<Stream, Args...>(args...);
}

#endif // STREAM_DISPENSER_H_
