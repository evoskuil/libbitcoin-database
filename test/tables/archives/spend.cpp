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

BOOST_AUTO_TEST_SUITE(spend_tests)

using namespace system;
constexpr auto terminal = table::spend::outs::terminal;
constexpr auto first = 0x12345678_u32;
constexpr auto second = 0xabcdef12_u32;
const auto expected_head = base16_chunk
(
    "ffffffff" // bucket[0] (fill)
    "ffffffff" // bucket[1] (fill)
    "78563412" // bucket[2] first
    "12efcdab" // bucket[3] second
);

BOOST_AUTO_TEST_CASE(spend__bind__unbound__expected)
{
    data_chunk head_file{};
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spend instance{ head_store };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.set(2u, table::spend::bind{ {}, first }));
    BOOST_REQUIRE(instance.set(3u, table::spend::bind{ {}, second }));
    BOOST_REQUIRE_EQUAL(instance.count(), 4u);
    BOOST_REQUIRE_EQUAL(head_file, expected_head);
}

BOOST_AUTO_TEST_CASE(spend__bind__bound__rebound)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spend instance{ head_store };
    BOOST_REQUIRE(instance.verify());
    BOOST_REQUIRE(instance.set(2u, table::spend::bind{ {}, first }));
    BOOST_REQUIRE_EQUAL(head_file, expected_head);
}

BOOST_AUTO_TEST_CASE(spend__at__populated__expected)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spend instance{ head_store };
    BOOST_REQUIRE(instance.verify());
    BOOST_REQUIRE_EQUAL(instance.at(1u).value, terminal);
    BOOST_REQUIRE_EQUAL(instance.at(2u).value, first);
    BOOST_REQUIRE_EQUAL(instance.at(3u).value, second);
    BOOST_REQUIRE_EQUAL(instance.at(4u).value, terminal);
    BOOST_REQUIRE(!table::spend::is_spent(instance.at(1u).value));
    BOOST_REQUIRE(table::spend::is_spent(instance.at(2u).value));
}

BOOST_AUTO_TEST_CASE(spend__get_run__populated__expected)
{
    auto head_file = expected_head;
    test::chunk_storage head_store{ head_file, test::terminal_fill };
    table::spend instance{ head_store };
    BOOST_REQUIRE(instance.verify());

    std::vector<table::spend::outs::integer> run(4);
    BOOST_REQUIRE(instance.get_run(run, 1u));
    BOOST_REQUIRE_EQUAL(run.at(0), terminal);
    BOOST_REQUIRE_EQUAL(run.at(1), first);
    BOOST_REQUIRE_EQUAL(run.at(2), second);
    BOOST_REQUIRE_EQUAL(run.at(3), terminal);
}

BOOST_AUTO_TEST_SUITE_END()
