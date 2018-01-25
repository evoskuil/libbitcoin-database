/////**
//// * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
//// *
//// * This file is part of libbitcoin.
//// *
//// * This program is free software: you can redistribute it and/or modify
//// * it under the terms of the GNU Affero General Public License as published by
//// * the Free Software Foundation, either version 3 of the License, or
//// * (at your option) any later version.
//// *
//// * This program is distributed in the hope that it will be useful,
//// * but WITHOUT ANY WARRANTY; without even the implied warranty of
//// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// * GNU Affero General Public License for more details.
//// *
//// * You should have received a copy of the GNU Affero General Public License
//// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//// */
////#include <boost/test/unit_test.hpp>
////
////#include <bitcoin/database.hpp>
////#include "../utility/storage.hpp"
////#include "../utility/utility.hpp"
////
////using namespace bc;
////using namespace bc::database;
////
////BOOST_AUTO_TEST_SUITE(hash_table_tests)
////
////BOOST_AUTO_TEST_CASE(hash_table__store__one_slab__expected)
////{
////    typedef test::tiny_hash key_type;
////    typedef hash_table<key_type, uint32_t, uint64_t> slab_map;
////
////    test::storage file;
////    BOOST_REQUIRE(file.open());
////
////    const auto buckets = 100u;
////    slab_map table(file, buckets);
////    BOOST_REQUIRE(table.create());
////
////    const key_type key1{ { 0xde, 0xad, 0xbe, 0xef } };
////
////    const auto writer = [](byte_serializer& serial)
////    {
////        serial.write_byte(110);
////        serial.write_byte(4);
////        serial.write_byte(99);
////    };
////
////    table.store(key1, writer, 3);
////    const auto memory = table.find(key1);
////    BOOST_REQUIRE(memory);
////
////    const auto slab = memory->buffer();
////    BOOST_REQUIRE(slab);
////    BOOST_REQUIRE_EQUAL(slab[0], 110u);
////    BOOST_REQUIRE_EQUAL(slab[1], 4u);
////    BOOST_REQUIRE_EQUAL(slab[2], 99u);
////}
////
////BOOST_AUTO_TEST_CASE(hash_table__find__overlapping_reads__expected)
////{
////    typedef test::tiny_hash key_type;
////    typedef hash_table<key_type, uint32_t, uint64_t> slab_map;
////
////    test::storage file;
////    BOOST_REQUIRE(file.open());
////
////    const auto buckets = 100u;
////    slab_map table(file, buckets);
////    BOOST_REQUIRE(table.create());
////
////    const key_type key1{ { 0xde, 0xad, 0xbe, 0xef } };
////    const key_type key2{ { 0xba, 0xad, 0xbe, 0xef } };
////
////    const auto writer1 = [](byte_serializer& serial)
////    {
////        serial.write_byte(42);
////        serial.write_byte(24);
////    };
////
////    const auto writer2 = [](byte_serializer& serial)
////    {
////        serial.write_byte(44);
////    };
////
////    table.store(key1, writer1, 2);
////    table.store(key2, writer2, 1);
////
////    const auto memory1 = table.find(key1);
////    const auto memory2 = table.find(key2);
////    BOOST_REQUIRE(memory1);
////    BOOST_REQUIRE(memory2);
////
////    const auto slab1 = memory1->buffer();
////    const auto slab2 = memory2->buffer();
////    BOOST_REQUIRE(slab1);
////    BOOST_REQUIRE(slab2);
////
////    BOOST_REQUIRE_EQUAL(slab1[0], 42u);
////    BOOST_REQUIRE_EQUAL(slab1[1], 24u);
////    BOOST_REQUIRE_EQUAL(slab2[0], 44u);
////}
////
////BOOST_AUTO_TEST_CASE(hash_table__unlink__first_stored__expected)
////{
////    typedef test::tiny_hash key_type;
////    typedef hash_table<key_type, uint32_t, uint64_t> slab_map;
////
////    test::storage file;
////    BOOST_REQUIRE(file.open());
////
////    const auto buckets = 100u;
////    slab_map table(file, buckets);
////    BOOST_REQUIRE(table.create());
////
////    const key_type key1{ { 0xde, 0xad, 0xbe, 0xef } };
////    const key_type key2{ { 0xba, 0xad, 0xbe, 0xef } };
////
////    const auto writer1 = [](byte_serializer& serial)
////    {
////        serial.write_byte(42);
////        serial.write_byte(24);
////    };
////
////    const auto writer2 = [](byte_serializer& serial)
////    {
////        serial.write_byte(44);
////    };
////
////    table.store(key1, writer1, 2);
////    table.store(key2, writer2, 1);
////    table.unlink(key1);
////
////    const auto memory1 = table.find(key1);
////    const auto memory2 = table.find(key2);
////    BOOST_REQUIRE(!memory1);
////    BOOST_REQUIRE(memory2);
////
////    const auto slab2 = memory2->buffer();
////    BOOST_REQUIRE(slab2);
////    BOOST_REQUIRE_EQUAL(slab2[0], 44u);
////}
////
////BOOST_AUTO_TEST_SUITE_END()