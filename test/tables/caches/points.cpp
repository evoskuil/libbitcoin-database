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

BOOST_AUTO_TEST_SUITE(points_tests)

using namespace system;
constexpr auto hash = base16_array
(
    "110102030405060708090a0b0c0d0e0f220102030405060708090a0b0c0d0e0f"
);
const table::points::record null_point{ {}, null_hash, chain::point::null_index };
const table::points::record point{ {}, hash, 0x00000042_u32 };
const auto expected_body = base16_chunk
(
    "0000000000000000000000000000000000000000000000000000000000000000" "ffffff"
    "110102030405060708090a0b0c0d0e0f220102030405060708090a0b0c0d0e0f" "420000"
);

BOOST_AUTO_TEST_CASE(points__put__two__expected)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::points instance{ head_store, body_store, 8u };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.put(3u, null_point));
    BOOST_REQUIRE(instance.put(5u, point));
    BOOST_REQUIRE_EQUAL(instance.at(3u), 0u);
    BOOST_REQUIRE_EQUAL(instance.at(5u), 1u);
    BOOST_REQUIRE(instance.at(4u).is_terminal());
    BOOST_REQUIRE_EQUAL(body_store.buffer(), expected_body);
}

BOOST_AUTO_TEST_CASE(points__at__two__expected)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::points instance{ head_store, body_store, 8u };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.put(3u, null_point));
    BOOST_REQUIRE(instance.put(5u, point));

    table::points::record out{};
    BOOST_REQUIRE(instance.at(3u, out));
    BOOST_REQUIRE(out == null_point);
    BOOST_REQUIRE(out.is_null());
    BOOST_REQUIRE(instance.at(5u, out));
    BOOST_REQUIRE(out == point);
    BOOST_REQUIRE(!out.is_null());
    BOOST_REQUIRE(!instance.at(4u, out));

    table::points::get_composed composed{};
    BOOST_REQUIRE(instance.at(5u, composed));
    BOOST_REQUIRE_EQUAL(composed.key.hash(), hash);
    BOOST_REQUIRE_EQUAL(composed.key.index(), 0x00000042_u32);
}

BOOST_AUTO_TEST_CASE(points__drop__base__keys_from_base)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::points instance{ head_store, body_store, 8u };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE_EQUAL(instance.base(), 0u);
    BOOST_REQUIRE(instance.put(1u, point));
    BOOST_REQUIRE(instance.drop(5u));
    BOOST_REQUIRE_EQUAL(instance.base(), 5u);
    BOOST_REQUIRE_EQUAL(instance.count(), 0u);
    BOOST_REQUIRE_EQUAL(instance.body_size(), zero);

    table::points::record out{};
    BOOST_REQUIRE(!instance.at(1u, out));
    BOOST_REQUIRE(!instance.put(4u, point));
    BOOST_REQUIRE(instance.put(6u, point));
    BOOST_REQUIRE(instance.at(6u, out));
    BOOST_REQUIRE(out == point);
    BOOST_REQUIRE_EQUAL(instance.body_size(), schema::points::minrow);
}

BOOST_AUTO_TEST_SUITE_END()
