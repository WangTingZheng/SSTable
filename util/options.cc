// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "options.h"

#include "comparator.h"
#include "env.h"

namespace leveldb {

Options::Options() : comparator(BytewiseComparator()), env(Env::Default()) {}

}  // namespace leveldb
