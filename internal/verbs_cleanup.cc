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

#include "internal/verbs_cleanup.h"

#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "infiniband/verbs.h"

namespace rdma_unit_test {

void VerbsCleanup::ContextDeleter(ibv_context* context) {
  int result = ibv_close_device(context);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::AhDeleter(ibv_ah* ah) {
  int result = ibv_destroy_ah(ah);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::PdDeleter(ibv_pd* pd) {
  int result = ibv_dealloc_pd(pd);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::ChannelDeleter(ibv_comp_channel* channel) {
  int result = ibv_destroy_comp_channel(channel);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::CqDeleter(ibv_cq* cq) {
  int result = ibv_destroy_cq(cq);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::CqExDeleter(ibv_cq_ex* cq) {
  int result = ibv_destroy_cq(ibv_cq_ex_to_cq(cq));
  EXPECT_EQ(0, result);
}

void VerbsCleanup::SrqDeleter(ibv_srq* srq) {
  int result = ibv_destroy_srq(srq);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::QpDeleter(ibv_qp* qp) {
  int result = ibv_destroy_qp(qp);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::MrDeleter(ibv_mr* mr) {
  int result = ibv_dereg_mr(mr);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::MwDeleter(ibv_mw* mw) {
  int result = ibv_dealloc_mw(mw);
  EXPECT_EQ(0, result);
}

void VerbsCleanup::AddCleanup(ibv_context* context) {
  absl::MutexLock guard(&mtx_contexts_);
  contexts_.emplace(context, &ContextDeleter);
}

void VerbsCleanup::AddCleanup(ibv_comp_channel* channel) {
  absl::MutexLock guard(&mtx_channels_);
  channels_.emplace(channel, &ChannelDeleter);
}

void VerbsCleanup::AddCleanup(ibv_cq* cq) {
  absl::MutexLock guard(&mtx_cqs_);
  cqs_.emplace(cq, &CqDeleter);
}

void VerbsCleanup::AddCleanup(ibv_cq_ex* cq) {
  absl::MutexLock guard(&mtx_cqs_ex_);
  cqs_ex_.emplace(cq, &CqExDeleter);
}

void VerbsCleanup::AddCleanup(ibv_pd* pd) {
  absl::MutexLock guard(&mtx_pds_);
  pds_.emplace(pd, &PdDeleter);
}

void VerbsCleanup::AddCleanup(ibv_ah* ah) {
  absl::MutexLock guard(&mtx_ahs_);
  ahs_.emplace(ah, &AhDeleter);
}

void VerbsCleanup::AddCleanup(ibv_srq* srq) {
  absl::MutexLock guard(&mtx_srqs_);
  srqs_.emplace(srq, &SrqDeleter);
}

void VerbsCleanup::AddCleanup(ibv_qp* qp) {
  absl::MutexLock guard(&mtx_qps_);
  qps_.emplace(qp, &QpDeleter);
}

void VerbsCleanup::AddCleanup(ibv_mr* mr) {
  absl::MutexLock guard(&mtx_mrs_);
  mrs_.emplace(mr, &MrDeleter);
}

void VerbsCleanup::AddCleanup(ibv_mw* mw) {
  absl::MutexLock guard(&mtx_mws_);
  mws_.emplace(mw, &MwDeleter);
}

void VerbsCleanup::ReleaseCleanup(ibv_context* context) {
  absl::MutexLock guard(&mtx_contexts_);
  auto node = contexts_.extract(context);
  ASSERT_TRUE(!node.empty());
  ibv_context* found = node.value().release();
  EXPECT_EQ(found, context);
}

void VerbsCleanup::ReleaseCleanup(ibv_comp_channel* channel) {
  absl::MutexLock guard(&mtx_channels_);
  auto node = channels_.extract(channel);
  ASSERT_TRUE(!node.empty());
  ibv_comp_channel* found = node.value().release();
  EXPECT_EQ(found, channel);
}

void VerbsCleanup::ReleaseCleanup(ibv_cq* cq) {
  absl::MutexLock guard(&mtx_cqs_);
  auto node = cqs_.extract(cq);
  ASSERT_TRUE(!node.empty());
  ibv_cq* found = node.value().release();
  EXPECT_EQ(found, cq);
}

void VerbsCleanup::ReleaseCleanup(ibv_cq_ex* cq) {
  absl::MutexLock guard(&mtx_cqs_ex_);
  auto node = cqs_ex_.extract(cq);
  ASSERT_TRUE(!node.empty());
  ibv_cq_ex* found = node.value().release();
  EXPECT_EQ(found, cq);
}

void VerbsCleanup::ReleaseCleanup(ibv_pd* pd) {
  absl::MutexLock guard(&mtx_pds_);
  auto node = pds_.extract(pd);
  ASSERT_TRUE(!node.empty());
  ibv_pd* found = node.value().release();
  EXPECT_EQ(found, pd);
}

void VerbsCleanup::ReleaseCleanup(ibv_ah* ah) {
  absl::MutexLock guard(&mtx_ahs_);
  auto node = ahs_.extract(ah);
  ASSERT_TRUE(!node.empty());
  ibv_ah* found = node.value().release();
  EXPECT_EQ(found, ah);
}

void VerbsCleanup::ReleaseCleanup(ibv_srq* srq) {
  absl::MutexLock guard(&mtx_srqs_);
  auto node = srqs_.extract(srq);
  ASSERT_TRUE(!node.empty());
  ibv_srq* found = node.value().release();
  EXPECT_EQ(found, srq);
}

void VerbsCleanup::ReleaseCleanup(ibv_qp* qp) {
  absl::MutexLock guard(&mtx_qps_);
  auto node = qps_.extract(qp);
  ASSERT_TRUE(!node.empty());
  ibv_qp* found = node.value().release();
  EXPECT_EQ(found, qp);
}

void VerbsCleanup::ReleaseCleanup(ibv_mr* mr) {
  absl::MutexLock guard(&mtx_mrs_);
  auto node = mrs_.extract(mr);
  ASSERT_TRUE(!node.empty());
  ibv_mr* found = node.value().release();
  EXPECT_EQ(found, mr);
}

void VerbsCleanup::ReleaseCleanup(ibv_mw* mw) {
  absl::MutexLock guard(&mtx_mws_);
  auto node = mws_.extract(mw);
  ASSERT_TRUE(!node.empty());
  ibv_mw* found = node.value().release();
  EXPECT_EQ(found, mw);
}

}  // namespace rdma_unit_test
