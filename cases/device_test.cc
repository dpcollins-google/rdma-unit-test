// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>

#include <thread>  // NOLINT
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "infiniband/verbs.h"
#include "cases/basic_fixture.h"
#include "cases/status_matchers.h"
#include "public/verbs_helper_suite.h"

namespace rdma_unit_test {

class DeviceTest : public BasicFixture {};

TEST_F(DeviceTest, GetDeviceList) {
  int num_devices = 0;
  ibv_device** devices = ibv_get_device_list(&num_devices);
  ASSERT_NE(nullptr, devices);
  ibv_free_device_list(devices);

  devices = ibv_get_device_list(nullptr);
  ASSERT_NE(nullptr, devices);
  ibv_free_device_list(devices);
}

TEST_F(DeviceTest, Open) {
  auto context = ibv_.OpenDevice();
  ASSERT_OK(context.status());
}

TEST_F(DeviceTest, OpenMany) {
  for (int i = 0; i < 100; ++i) {
    auto context = ibv_.OpenDevice();
    ASSERT_OK(context.status());
  }
}

TEST_F(DeviceTest, OpenInAnotherThread) {
  std::thread another_thread([this]() {
    auto context = ibv_.OpenDevice();
    EXPECT_OK(context.status());
  });
  another_thread.join();
}

TEST_F(DeviceTest, OpenInManyThreads) {
  std::vector<std::thread> threads;
  for (int i = 0; i < 100; i++) {
    threads.push_back(std::thread([this]() {
      auto context = ibv_.OpenDevice();
      EXPECT_OK(context.status());
    }));
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(DeviceTest, QueryDevice) {
  ibv_context* context = ibv_.OpenDevice().value();

  ibv_device_attr dev_attr = {};
  int query_result = ibv_query_device(context, &dev_attr);
  ASSERT_EQ(0, query_result);
  LOG(INFO) << "Device capabilities = " << std::hex
            << dev_attr.device_cap_flags;
}

TEST_F(DeviceTest, ContextTomfoolery) {
  ibv_context* context1 = ibv_.OpenDevice().value();
  ibv_context* context2 = ibv_.OpenDevice().value();
  auto* pd = ibv_alloc_pd(context1);
  ASSERT_NE(nullptr, pd);
  // Try to delete with the other context.
  pd->context = context2;
  ASSERT_EQ(ENOENT, ibv_dealloc_pd(pd));
  pd->context = context1;
  ASSERT_EQ(0, ibv_dealloc_pd(pd));
}

// TODO(author1): Create Max

}  // namespace rdma_unit_test