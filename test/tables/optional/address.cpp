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

BOOST_AUTO_TEST_SUITE(address_tests)

using namespace system;

// Both keys select bucket zero (of 8), forming one chain of two rows.
constexpr auto key1 = base16_array
(
    "1000000000000000aa000000000000000000000000000000000000000000000a"
);

constexpr auto key2 = base16_array
(
    "2000000000000000bb000000000000000000000000000000000000000000000b"
);

const auto expected_body = base16_chunk
(
    "ffffffff" // next->end
    "00000000" // next->0
);

BOOST_AUTO_TEST_CASE(address__commit__two__expected_body)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::address instance{ head_store, body_store, 8 };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.enabled());

    // Rows are aligned with outs by expansion, not allocation.
    BOOST_REQUIRE(instance.expand(2u));
    const auto ptr = instance.get_memory();
    BOOST_REQUIRE(instance.push(ptr, 0u, { key1 }));
    BOOST_REQUIRE(instance.push(ptr, 1u, { key2 }));
    BOOST_REQUIRE_EQUAL(body_store.buffer(), expected_body);
}

BOOST_AUTO_TEST_CASE(address__it__co_bucket__all_candidates)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::address instance{ head_store, body_store, 8 };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.expand(2u));
    const auto ptr = instance.get_memory();
    BOOST_REQUIRE(instance.push(ptr, 0u, { key1 }));
    BOOST_REQUIRE(instance.push(ptr, 1u, { key2 }));

    // Nothing is stored, so both rows are candidates for either key.
    std::vector<table::address::link::integer> links{};
    for (auto it = instance.it({ key1 }); it; ++it)
        links.push_back(*it);

    BOOST_REQUIRE_EQUAL(links.size(), 2u);
    BOOST_REQUIRE_EQUAL(links.at(0), 1u);
    BOOST_REQUIRE_EQUAL(links.at(1), 0u);
}

BOOST_AUTO_TEST_CASE(address__enabled__no_buckets__false)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::address instance{ head_store, body_store, 0 };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(!instance.enabled());
}

BOOST_AUTO_TEST_SUITE_END()
