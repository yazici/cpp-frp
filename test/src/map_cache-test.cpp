/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <array_util.h>
#include <frp/static/push/map_cache.h>
#include <frp/static/push/sink.h>
#include <frp/static/push/source.h>
#include <frp/static/push/transform.h>
#include <future>
#include <gtest/gtest.h>
#include <string>
#include <test_types.h>
#include <vector>

TEST(map_cache, test1) {
	auto source(frp::stat::push::source(make_array(1, 2, 3, 4)));
	auto map(frp::stat::push::map_cache([](auto i) { return std::to_string(i); }, std::ref(source)));
	auto sink(frp::stat::push::sink(std::ref(map)));
	auto reference(*sink);
	auto values(*reference);
	ASSERT_EQ(values[0], "1");
	ASSERT_EQ(values[1], "2");
	ASSERT_EQ(values[2], "3");
	ASSERT_EQ(values[3], "4");
}

TEST(map_cache, empty_collection) {
	auto map(frp::stat::push::map_cache([](auto i) { return std::to_string(i); },
		frp::stat::push::transform([]() { return make_array<int>(); })));
	auto sink(frp::stat::push::sink(std::ref(map)));
	auto reference(*sink);
	auto values(*reference);
	ASSERT_TRUE(values.empty());
}

TEST(map_cache, test_caching) {
	auto source(frp::stat::push::source(make_array(1, 2, 3, 4)));
	std::unordered_map<int, std::size_t> counter;
	auto map(frp::stat::push::map_cache([&](auto i) {
			++counter[i];
			return std::to_string(i);
		},
		std::ref(source)));
	source = make_array(3, 4, 5, 6);
	auto sink(frp::stat::push::sink(std::ref(map)));
	auto reference(*sink);
	auto values(*reference);
	ASSERT_EQ(values[0], "3");
	ASSERT_EQ(values[1], "4");
	ASSERT_EQ(values[2], "5");
	ASSERT_EQ(values[3], "6");
	ASSERT_EQ(counter[1], 1);
	ASSERT_EQ(counter[2], 1);
	ASSERT_EQ(counter[3], 1);
	ASSERT_EQ(counter[4], 1);
	ASSERT_EQ(counter[5], 1);
	ASSERT_EQ(counter[6], 1);
}

TEST(map_cache, custom_comparator) {
	auto source(frp::stat::push::source(make_array(1, 3, 5)));
	auto map(frp::stat::push::map_cache<odd_comparator_type, std::hash<int>>(
		[](auto c) { return c; }, std::ref(source)));
	auto sink(frp::stat::push::sink(std::ref(map)));
	auto reference1(*sink);
	auto value1(*reference1);
	ASSERT_EQ(value1[0], 1);
	ASSERT_EQ(value1[1], 3);
	ASSERT_EQ(value1[2], 5);
	source = { 5, 7, 9 };
	auto reference2(*sink);
	auto value2(*reference2);
	ASSERT_EQ(value2[0], 1);
	ASSERT_EQ(value2[1], 3);
	ASSERT_EQ(value2[2], 5);
	source = { 1, 2, 3 };
	auto reference3(*sink);
	auto value3(*reference3);
	ASSERT_EQ(value3[0], 1);
	ASSERT_EQ(value3[1], 2);
	ASSERT_EQ(value3[2], 3);
}

TEST(map_cache, references) {
	auto f([](auto source) {
		return source * source;
	});
	auto source(frp::stat::push::transform([]() { return make_array(1, 3, 5, 7); }));
	auto map(frp::stat::push::map_cache(std::cref(f), std::cref(source)));
}

TEST(map_cache, indexed_expand) {
	auto sink(frp::stat::push::sink(frp::stat::push::map_cache<1>(
		[](auto i, auto j, auto k) {
			return i + j + k;
		},
		frp::stat::push::source(1), frp::stat::push::source(make_array(0, 1, 2, 3)),
		frp::stat::push::source(3))));
	auto value(**sink);
	ASSERT_TRUE(std::equal(std::begin(value), std::end(value),
		std::begin(make_array(4, 5, 6, 7))));
}

TEST(map_cache, indexed_expand_update_dependency) {
	auto source1(frp::stat::push::source(1));
	auto source2(frp::stat::push::source(make_array(0, 1, 2, 3)));
	auto sink(frp::stat::push::sink(frp::stat::push::map_cache<1>(
		[](auto i, auto j, auto k) {
			return i + j + k;
		},
		std::ref(source1), std::ref(source2), frp::stat::push::source(3))));
	auto value1(**sink);
	ASSERT_TRUE(std::equal(std::begin(value1), std::end(value1),
		std::begin(make_array(4, 5, 6, 7))));
	source1 = 2;
	auto value2(**sink);
	ASSERT_TRUE(std::equal(std::begin(value2), std::end(value2),
		std::begin(make_array(5, 6, 7, 8))));
}

TEST(map_cache, indexed_expand_update_dependency_and_invalidate_cache) {
	auto source1(frp::stat::push::source(1));
	auto source2(frp::stat::push::source(make_array(0, 1, 2, 3)));
	auto sink(frp::stat::push::sink(
		frp::stat::push::map_cache<1, odd_comparator_type, std::hash<int>>(
			[](auto i, auto j, auto k) {
				return i + j + k;
			},
			std::ref(source1), std::ref(source2), frp::stat::push::source(3))));
	{
		auto value(**sink);
		ASSERT_TRUE(std::equal(std::begin(value), std::end(value),
			std::begin(make_array(4, 5, 6, 7))));
	}
	source2 = make_array(10, 11, 12, 13);
	{
		auto value(**sink);
		ASSERT_TRUE(std::equal(std::begin(value), std::end(value),
			std::begin(make_array(4, 5, 6, 7))));
	}
	source1 = 2;
	{
		auto value(**sink);
		ASSERT_TRUE(std::equal(std::begin(value), std::end(value),
			std::begin(make_array(15, 16, 17, 18))));
	}
}