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
#include "../test.hpp"
#include "../mocks/blocks.hpp"
#include "../mocks/chunk_store.hpp"

BOOST_FIXTURE_TEST_SUITE(query_unspent_tests, test::directory_setup_fixture)

BOOST_AUTO_TEST_CASE(query_unspent__get_unspent_totals__empty_branch__success_zeros)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const stopper cancel{};
    unspent_totals totals{};
    BOOST_REQUIRE(!query.get_unspent_totals(cancel, totals, {}));
    BOOST_REQUIRE_EQUAL(totals.outputs, 0u);
    BOOST_REQUIRE_EQUAL(totals.transactions, 0u);
    BOOST_REQUIRE_EQUAL(totals.script_bytes, 0u);
    BOOST_REQUIRE_EQUAL(totals.value, 0u);
}

BOOST_AUTO_TEST_CASE(query_unspent__get_unspent_totals__two_block_branch__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_REQUIRE(query.set(test::block2, context{ 0, 2, 0 }, false, false));

    // Each block is one coinbase with one unspent p2pk output (67 bytes).
    const stopper cancel{};
    unspent_totals totals{};
    const header_links branch{ header_link{ 2 }, header_link{ 1 } };
    BOOST_REQUIRE(!query.get_unspent_totals(cancel, totals, branch));
    BOOST_REQUIRE_EQUAL(totals.outputs, 2u);
    BOOST_REQUIRE_EQUAL(totals.transactions, 2u);
    BOOST_REQUIRE_EQUAL(totals.script_bytes, 134u);
    BOOST_REQUIRE_EQUAL(totals.value, 10'000'000'000u);
}

BOOST_AUTO_TEST_CASE(query_unspent__get_unspent_totals__cancelled__query_canceled)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, context{ 0, 1, 0 }, false, false));

    const stopper cancel{ true };
    unspent_totals totals{};
    const header_links branch{ header_link{ 1 } };
    const auto ec = query.get_unspent_totals(cancel, totals, branch);
    BOOST_REQUIRE_EQUAL(ec, error::query_canceled);
}

BOOST_AUTO_TEST_SUITE_END()
