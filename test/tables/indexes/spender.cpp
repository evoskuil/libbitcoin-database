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
constexpr auto buckets = 8u;
const table::spender::record spender1{ {}, 0x11111111_u32 };
const table::spender::record spender2{ {}, 0x22222222_u32 };
const auto outs_fk = table::spender::outs{ 0x00000007_u32 };

BOOST_AUTO_TEST_CASE(spender__put__two_same_key__both_found)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::spender instance{ head_store, body_store, buckets };
    BOOST_REQUIRE(instance.create());

    const auto key = table::spender::to_key(outs_fk);
    BOOST_REQUIRE(instance.put(key, spender1));
    BOOST_REQUIRE(instance.put(key, spender2));
    BOOST_REQUIRE_EQUAL(instance.count(), 2u);

    std::vector<table::spender::record> found{};
    for (auto it = instance.it(key); it; ++it)
    {
        table::spender::record out{};
        BOOST_REQUIRE(instance.get(*it, out));
        found.push_back(out);
    }

    BOOST_REQUIRE_EQUAL(found.size(), 2u);
    BOOST_REQUIRE(found.at(0) == spender2);
    BOOST_REQUIRE(found.at(1) == spender1);
}

BOOST_AUTO_TEST_CASE(spender__it__other_key__none)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::spender instance{ head_store, body_store, buckets };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.put(table::spender::to_key(outs_fk), spender1));

    const auto other = table::spender::to_key(table::spender::outs{ 0x00000008_u32 });
    BOOST_REQUIRE(!instance.exists(other));
    BOOST_REQUIRE(instance.exists(table::spender::to_key(outs_fk)));
}

BOOST_AUTO_TEST_SUITE_END()
