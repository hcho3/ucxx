/**
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * See file LICENSE for terms.
 */
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <ucxx/log.h>

namespace ucxx {

class DelayedNotificationRequest;

typedef std::shared_ptr<DelayedNotificationRequest> DelayedNotificationRequestCallbackDataType;
typedef std::function<void(DelayedNotificationRequestCallbackDataType)>
  DelayedNotificationRequestCallbackType;

class DelayedNotificationRequest {
 public:
  ucp_worker_h _worker{nullptr};
  ucp_ep_h _ep{nullptr};
  std::shared_ptr<ucxx_request_t> _request{nullptr};
  bool _send{false};
  void* _buffer{nullptr};
  size_t _length{0};
  ucp_tag_t _tag{0};

  DelayedNotificationRequest() = delete;

  DelayedNotificationRequest(ucp_worker_h worker,
                             ucp_ep_h ep,
                             std::shared_ptr<ucxx_request_t> request,
                             const bool send,
                             void* buffer,
                             const size_t length,
                             const ucp_tag_t tag = 0)
    : _worker(worker),
      _ep(ep),
      _request(request),
      _send(send),
      _buffer(buffer),
      _length(length),
      _tag(tag)
  {
  }
};

class DelayedNotificationRequestCallback {
 private:
  DelayedNotificationRequestCallbackType _callback{nullptr};
  DelayedNotificationRequestCallbackDataType _callbackData{nullptr};

 public:
  DelayedNotificationRequestCallback(DelayedNotificationRequestCallbackType callback,
                                     DelayedNotificationRequestCallbackDataType callbackData)
    : _callback(callback), _callbackData(callbackData)
  {
  }

  std::pair<DelayedNotificationRequestCallbackType, DelayedNotificationRequestCallbackDataType>
  get()
  {
    return std::pair(_callback, _callbackData);
  }
};

typedef std::shared_ptr<DelayedNotificationRequestCallback>
  DelayedNotificationRequestCallbackPtrType;

class DelayedNotificationRequestCollection {
 private:
  std::vector<DelayedNotificationRequestCallbackPtrType> _collection{};
  std::mutex _mutex{};

 public:
  DelayedNotificationRequestCollection() = default;

  DelayedNotificationRequestCollection(const DelayedNotificationRequestCollection&) = delete;
  DelayedNotificationRequestCollection& operator=(DelayedNotificationRequestCollection const&) =
    delete;

  DelayedNotificationRequestCollection(DelayedNotificationRequestCollection&& o) = delete;
  DelayedNotificationRequestCollection& operator=(DelayedNotificationRequestCollection&& o) =
    delete;

  void process()
  {
    if (_collection.size() > 0) {
      ucxx_trace_req("Submitting %lu requests", _collection.size());

      // Move _collection to a local copy in order to to hold the lock for as
      // short as possible
      decltype(_collection) toProcess;
      {
        std::lock_guard<std::mutex> lock(_mutex);
        std::swap(_collection, toProcess);
      }

      for (auto& dnr : toProcess) {
        auto callbackPair = dnr->get();
        auto callback     = callbackPair.first;
        auto callbackData = callbackPair.second;

        ucxx_trace_req("Submitting request: %p %p",
                       callback.target<void (*)(std::shared_ptr<void>)>(),
                       callbackData.get());

        if (callback) callback(callbackData);
      }
    }
  }

  void registerRequest(DelayedNotificationRequestCallbackType callback,
                       DelayedNotificationRequestCallbackDataType callbackData)
  {
    auto r = std::make_shared<DelayedNotificationRequestCallback>(callback, callbackData);

    {
      std::lock_guard<std::mutex> lock(_mutex);
      _collection.push_back(r);
    }
    ucxx_trace_req("Registered submit request: %p %p",
                   callback.target<void (*)(std::shared_ptr<void>)>(),
                   callbackData.get());
  }
};

}  // namespace ucxx
