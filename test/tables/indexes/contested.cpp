/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../../test.hpp"
#include "../../mocks/chunk_storage.hpp"

BOOST_AUTO_TEST_SUITE(contested_tests)

using namespace system;

BOOST_AUTO_TEST_CASE(contested__to_cell__outs__eight_per_cell)
{
    static_assert(table::contested::to_cell(0) == 0u);
    static_assert(table::contested::to_cell(7) == 0u);
    static_assert(table::contested::to_cell(8) == 1u);
    static_assert(table::contested::to_mask(0) == 0x01u);
    static_assert(table::contested::to_mask(10) == 0x04u);
    static_assert(table::contested::to_mask(15) == 0x80u);
}

BOOST_AUTO_TEST_CASE(contested__set__contest__bits_merged)
{
    data_chunk head_file{};
    test::chunk_storage head_store{ head_file };
    table::contested instance{ head_store };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.set(table::contested::to_cell(10), table::contested::contest{ {}, table::contested::to_mask(10) }));
    BOOST_REQUIRE(instance.set(table::contested::to_cell(15), table::contested::contest{ {}, table::contested::to_mask(15) }));
    BOOST_REQUIRE(instance.set(table::contested::to_cell(10), table::contested::contest{ {}, table::contested::to_mask(10) }));
    BOOST_REQUIRE_EQUAL(instance.count(), 2u);
    BOOST_REQUIRE_EQUAL(head_file, base16_chunk("0084"));
    BOOST_REQUIRE_EQUAL(instance.at(0), 0x00u);
    BOOST_REQUIRE_EQUAL(instance.at(1), 0x84u);
}

BOOST_AUTO_TEST_CASE(contested__at__unallocated__clear)
{
    data_chunk head_file{};
    test::chunk_storage head_store{ head_file };
    table::contested instance{ head_store };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE_EQUAL(instance.at(42), 0x00u);
}

BOOST_AUTO_TEST_SUITE_END()
