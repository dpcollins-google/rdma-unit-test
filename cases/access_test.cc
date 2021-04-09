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

#include <cstdint>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "infiniband/verbs.h"
#include "cases/basic_fixture.h"
#include "cases/status_matchers.h"
#include "public/rdma-memblock.h"
#include "public/util.h"

namespace rdma_unit_test {

class AccessTest : public BasicFixture,
                   public ::testing::WithParamInterface<ibv_mw_type> {
 public:
  void SetUp() override {
    if (GetParam() == IBV_MW_TYPE_1 && !Introspection().SupportsType1()) {
      LOG(INFO) << "Nic does not support Type1 MW";
      GTEST_SKIP();
    }
    if (GetParam() == IBV_MW_TYPE_2 && !Introspection().SupportsType2()) {
      LOG(INFO) << "Nic does not support Type2 MW";
      GTEST_SKIP();
    }
  }

 protected:
  struct BasicSetup {
    ibv_context* context;
    verbs_util::LocalVerbsAddress address;
    ibv_pd* pd;
    RdmaMemBlock src_buffer;
    RdmaMemBlock dst_buffer;
    ibv_cq* src_cq;
    ibv_cq* dst_cq;
  };

  absl::StatusOr<BasicSetup> CreateBasicSetup() {
    BasicSetup setup;
    setup.src_buffer = ibv_.AllocBuffer(/*pages=*/2);
    setup.dst_buffer = ibv_.AllocBuffer(/*pages=*/2);
    auto context_or = ibv_.OpenDevice();
    if (!context_or.ok()) {
      return context_or.status();
    }
    setup.context = context_or.value();
    setup.address = ibv_.GetContextAddressInfo(setup.context);
    setup.pd = ibv_.AllocPd(setup.context);
    if (!setup.pd) {
      return absl::InternalError("Failed to allcoate pd.");
    }
    setup.src_cq = ibv_.CreateCq(setup.context);
    if (!setup.src_cq) {
      return absl::InternalError("Failed to create source qp.");
    }
    setup.dst_cq = ibv_.CreateCq(setup.context);
    if (!setup.dst_cq) {
      return absl::InternalError("Failed to create destination qp.");
    }
    return setup;
  }

  void AttemptMrRead(BasicSetup setup, int src_mr_access, int dst_mr_access,
                     ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_wc_status actual =
        verbs_util::ReadSync(src_qp, setup.src_buffer.span(), src_mr,
                             setup.dst_buffer.data(), dst_mr->rkey);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMwRead(BasicSetup setup, int src_mr_access, int dst_mr_access,
                     int dst_mw_access, ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_mw* dst_mw =
        CreateAndBindMw(dst_qp, setup.dst_buffer, dst_mr, dst_mw_access);
    ASSERT_NE(nullptr, dst_mw);
    ibv_wc_status actual =
        verbs_util::ReadSync(src_qp, setup.src_buffer.span(), src_mr,
                             setup.dst_buffer.data(), dst_mw->rkey);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMrWrite(BasicSetup setup, int src_mr_access, int dst_mr_access,
                      ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_wc_status actual =
        verbs_util::WriteSync(src_qp, setup.src_buffer.span(), src_mr,
                              setup.dst_buffer.data(), dst_mr->rkey);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMwWrite(BasicSetup setup, int src_mr_access, int dst_mr_access,
                      int dst_mw_access, ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_mw* dst_mw =
        CreateAndBindMw(dst_qp, setup.dst_buffer, dst_mr, dst_mw_access);
    ASSERT_NE(nullptr, dst_mw);
    ibv_wc_status actual =
        verbs_util::WriteSync(src_qp, setup.src_buffer.span(), src_mr,
                              setup.dst_buffer.data(), dst_mw->rkey);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMrAtomic(BasicSetup setup, int src_mr_access, int dst_mr_access,
                       ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_wc_status actual = verbs_util::FetchAddSync(
        src_qp, setup.src_buffer.data(), src_mr, setup.dst_buffer.data(),
        dst_mr->rkey, /*comp_add=*/1);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMwAtomic(BasicSetup setup, int src_mr_access, int dst_mr_access,
                       int dst_mw_access, ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    ibv_mw* dst_mw =
        CreateAndBindMw(dst_qp, setup.dst_buffer, dst_mr, dst_mw_access);
    ASSERT_NE(nullptr, dst_mw);
    ibv_wc_status actual = verbs_util::FetchAddSync(
        src_qp, setup.src_buffer.data(), src_mr, setup.dst_buffer.data(),
        dst_mw->rkey, /*comp_add=*/1);
    EXPECT_EQ(actual, expected);
  }

  void AttemptMrSend(BasicSetup setup, int src_mr_access, int dst_mr_access,
                     ibv_wc_status expected) {
    ibv_mr* src_mr = ibv_.RegMr(setup.pd, setup.src_buffer, src_mr_access);
    ibv_mr* dst_mr = ibv_.RegMr(setup.pd, setup.dst_buffer, dst_mr_access);
    auto [src_qp, dst_qp] = CreateNewConnectedQpPair(setup);
    ASSERT_NE(nullptr, src_qp);
    ASSERT_NE(nullptr, dst_qp);
    auto [src_status, dst_status] =
        verbs_util::SendRecvSync(src_qp, dst_qp, setup.src_buffer.span(),
                                 src_mr, setup.dst_buffer.span(), dst_mr);
    ASSERT_EQ(src_status, expected);
  }

 private:
  std::pair<ibv_qp*, ibv_qp*> CreateNewConnectedQpPair(
      const BasicSetup& setup) {
    ibv_qp* src_qp = ibv_.CreateQp(setup.pd, setup.src_cq);
    ibv_qp* dst_qp = ibv_.CreateQp(setup.pd, setup.dst_cq);
    if (src_qp && dst_qp) {
      ibv_.SetUpLoopbackRcQps(src_qp, dst_qp, setup.address);
    }
    return {src_qp, dst_qp};
  }

  ibv_mw* CreateAndBindMw(ibv_qp* dst_qp, RdmaMemBlock dst_buffer,
                          ibv_mr* dst_mr, int access) {
    static int type2_rkey = 17;
    ibv_mw* mw = ibv_.AllocMw(dst_qp->pd, GetParam());
    if (!mw) {
      LOG(ERROR) << "Failed to allocate mw.";
      return nullptr;
    }

    switch (GetParam()) {
      case IBV_MW_TYPE_1: {
        ibv_wc_status status = verbs_util::BindType1MwSync(
            dst_qp, mw, dst_buffer.span(), dst_mr, access);
        if (status != IBV_WC_SUCCESS) {
          LOG(ERROR) << "Cannot bind mw.";
          return nullptr;
        }
      } break;
      case IBV_MW_TYPE_2: {
        ibv_wc_status status = verbs_util::BindType2MwSync(
            dst_qp, mw, dst_buffer.span(), ++type2_rkey, dst_mr, access);
        if (status != IBV_WC_SUCCESS) {
          LOG(ERROR) << "Cannot bind mw.";
          return nullptr;
        }
      } break;
      default:
        LOG(FATAL) << "Unknown param";
    }
    return mw;
  }
};

TEST_P(AccessTest, AllAccess) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingSrcLocalWrite) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_MW_BIND | IBV_ACCESS_REMOTE_READ;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_LOC_PROT_ERR);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_LOC_PROT_ERR);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_LOC_PROT_ERR);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_LOC_PROT_ERR);
}

TEST_P(AccessTest, MissingDstLocalWrite) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_MW_BIND | IBV_ACCESS_REMOTE_READ;
  const int kDstMwAccess = IBV_ACCESS_MW_BIND | IBV_ACCESS_REMOTE_READ;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_REM_ACCESS_ERR);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_REM_ACCESS_ERR);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_REM_ACCESS_ERR);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_REM_ACCESS_ERR);
}

TEST_P(AccessTest, MissingDstMrRemoteWrite) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_REM_ACCESS_ERR);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingDstMwRemoteWrite) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_REM_ACCESS_ERR);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingDstMrRemoteAtomic) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_REM_ACCESS_ERR);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingDstMwRemoteAtomic) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_REM_ACCESS_ERR);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingDstMrRemoteRead) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_REM_ACCESS_ERR);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_SUCCESS);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

TEST_P(AccessTest, MissingDstMwRemoteRead) {
  ASSERT_OK_AND_ASSIGN(BasicSetup setup, CreateBasicSetup());
  const int kSrcMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMrAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
  const int kDstMwAccess = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_MW_BIND |
                           IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE;
  AttemptMrRead(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwRead(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                IBV_WC_REM_ACCESS_ERR);
  AttemptMrWrite(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwWrite(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                 IBV_WC_SUCCESS);
  AttemptMrAtomic(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
  AttemptMwAtomic(setup, kSrcMrAccess, kDstMrAccess, kDstMwAccess,
                  IBV_WC_SUCCESS);
  AttemptMrSend(setup, kSrcMrAccess, kDstMrAccess, IBV_WC_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(AccessTestCase, AccessTest,
                         ::testing::Values(IBV_MW_TYPE_1, IBV_MW_TYPE_2));

}  // namespace rdma_unit_test
