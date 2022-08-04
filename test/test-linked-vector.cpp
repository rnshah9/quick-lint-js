// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <boost/container/pmr/global_resource.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/container/linked-vector.h>
#include <quick-lint-js/util/narrow-cast.h>
#include <utility>
#include <vector>

using ::boost::container::pmr::new_delete_resource;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace quick_lint_js {
namespace {
template <class T>
std::vector<T> to_vector(const linked_vector<T>& v) {
  std::vector<T> result;
  v.for_each([&](auto& x) { result.push_back(x); });
  return result;
}

TEST(test_linked_vector, empty) {
  linked_vector<int> v(new_delete_resource());
  EXPECT_TRUE(v.empty());
  EXPECT_THAT(to_vector(v), IsEmpty());
}

TEST(test_linked_vector, emplace_back_one) {
  linked_vector<int> v(new_delete_resource());
  v.emplace_back(42);
  EXPECT_FALSE(v.empty());
  EXPECT_THAT(to_vector(v), ElementsAre(42));
}

TEST(test_linked_vector, emplace_back_full_chunk) {
  linked_vector<int> v(new_delete_resource());
  std::vector<int> expected_items;
  for (int i = 0; i < narrow_cast<int>(v.items_per_chunk); ++i) {
    v.emplace_back(i);
    expected_items.push_back(i);
  }
  EXPECT_THAT(to_vector(v), ContainerEq(expected_items));
}

TEST(test_linked_vector, emplace_back_full_chunk_and_one) {
  linked_vector<int> v(new_delete_resource());
  std::vector<int> expected_items;
  for (int i = 0; i < narrow_cast<int>(v.items_per_chunk) + 1; ++i) {
    v.emplace_back(i);
    expected_items.push_back(i);
  }
  EXPECT_THAT(to_vector(v), ContainerEq(expected_items));
}
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
