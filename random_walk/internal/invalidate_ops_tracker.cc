/*
 * Copyright 2021 Google LLC
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

#include "random_walk/internal/invalidate_ops_tracker.h"

#include <cstdint>

#include "public/map_util.h"
#include "random_walk/internal/types.h"

namespace rdma_unit_test {
namespace random_walk {

void InvalidateOpsTracker::PushInvalidate(uint64_t wr_id, uint32_t rkey,
                                          ClientId client_id) {
  InvalidateWr wr{.client_id = client_id, .rkey = rkey};
  map_util::InsertOrDie(invalidate_wrs_, wr_id, wr);
}

InvalidateOpsTracker::InvalidateWr InvalidateOpsTracker::ExtractInvalidateWr(
    uint64_t wr_id) {
  return map_util::FindOrDie(invalidate_wrs_, wr_id);
}

}  // namespace random_walk
}  // namespace rdma_unit_test
