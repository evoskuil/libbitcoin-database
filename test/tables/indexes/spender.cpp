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

BOOST_AUTO_TEST_SUITE(spender_tests)

using namespace system;
constexpr auto terminal = table::spender::value::terminal;
constexpr auto first = 0x00112233_u32;
constexpr auto second = 0x00445566_u32;
const auto expected_head = base16_chunk
(
    "ffffffff" // bucket[0] (fill)
    "33221100" // bucket[1] first
);

BOOST_AUTO_TEST_CASE(spender__spend__unspent__spent_by_first)
{
    data_chunk head_file{};
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spender instance{ head_store };
    BOOST_REQUIRE(instance.create());

    table::spender::value::integer holder{};
    BOOST_REQUIRE(instance.set(1u, table::spender::spend{ {}, holder, first }));
    BOOST_REQUIRE_EQUAL(holder, terminal);
    BOOST_REQUIRE_EQUAL(instance.count(), 2u);
    BOOST_REQUIRE_EQUAL(head_file, expected_head);

    const auto cell = instance.at(1u).value;
    BOOST_REQUIRE(table::spender::is_spent(cell));
    BOOST_REQUIRE(!table::spender::is_contested(cell));
    BOOST_REQUIRE_EQUAL(table::spender::spender_fk(cell), first);
}

BOOST_AUTO_TEST_CASE(spender__spend__spent__holder_retained)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spender instance{ head_store };
    BOOST_REQUIRE(instance.verify());

    table::spender::value::integer holder{};
    BOOST_REQUIRE(instance.set(1u, table::spender::spend{ {}, holder, second }));
    BOOST_REQUIRE_EQUAL(holder, first);
    BOOST_REQUIRE_EQUAL(head_file, expected_head);
    BOOST_REQUIRE_EQUAL(table::spender::spender_fk(instance.at(1u).value), first);
}

BOOST_AUTO_TEST_CASE(spender__contest__spent__contested)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spender instance{ head_store };
    BOOST_REQUIRE(instance.verify());
    BOOST_REQUIRE(instance.set(1u, table::spender::contest{}));

    const auto cell = instance.at(1u).value;
    BOOST_REQUIRE(table::spender::is_spent(cell));
    BOOST_REQUIRE(table::spender::is_contested(cell));
    BOOST_REQUIRE_EQUAL(table::spender::spender_fk(cell), first);
    BOOST_REQUIRE_EQUAL(cell, table::spender::merge(true, first));
}

BOOST_AUTO_TEST_CASE(spender__at__unallocated__unspent)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spender instance{ head_store };
    BOOST_REQUIRE(instance.verify());
    BOOST_REQUIRE(!table::spender::is_spent(instance.at(0u).value));
    BOOST_REQUIRE(!table::spender::is_spent(instance.at(2u).value));
    BOOST_REQUIRE(instance.at(42u).is_terminal());
}

BOOST_AUTO_TEST_SUITE_END()
