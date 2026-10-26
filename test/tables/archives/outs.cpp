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

BOOST_AUTO_TEST_SUITE(outs_tests)

using namespace system;
const table::outs::record out1{ {}, 0x7890abcdef_u64, 0x01020304_u32, false, true };
const table::outs::record out2{ {}, 0x1234567890_u64, 0x0a0b0c0d_u32, true, false };
const auto expected_body = base16_chunk
(
    "efcdab9078" // out_fk1
    "04030281"   // parent_fk1 | coinbase
    "9078563492" // out_fk2 | excluded
    "0d0c0b0a"   // parent_fk2
);

BOOST_AUTO_TEST_CASE(outs__put__two__expected)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::outs instance{ head_store, body_store };
    BOOST_REQUIRE(instance.create());

    table::outs::link link1{};
    table::outs::link link2{};
    BOOST_REQUIRE(instance.put_link(link1, out1));
    BOOST_REQUIRE(instance.put_link(link2, out2));
    BOOST_REQUIRE_EQUAL(link1, 0u);
    BOOST_REQUIRE_EQUAL(link2, 1u);
    BOOST_REQUIRE_EQUAL(body_store.buffer(), expected_body);
}

BOOST_AUTO_TEST_CASE(outs__get__two__expected)
{
    auto head = base16_chunk("02000000");
    auto body = expected_body;
    test::chunk_storage head_store{ head, test::terminal_fill };
    test::chunk_storage body_store{ body };
    table::outs instance{ head_store, body_store };

    table::outs::record out{};
    BOOST_REQUIRE(instance.get(0u, out));
    BOOST_REQUIRE(out == out1);
    BOOST_REQUIRE(instance.get(1u, out));
    BOOST_REQUIRE(out == out2);

    table::outs::get_output output{};
    BOOST_REQUIRE(instance.get(1u, output));
    BOOST_REQUIRE_EQUAL(output.out_fk, 0x1234567890_u64);

    table::outs::get_parent parent{};
    BOOST_REQUIRE(instance.get(0u, parent));
    BOOST_REQUIRE_EQUAL(parent.parent_fk, 0x01020304_u32);
    BOOST_REQUIRE(parent.coinbase);
    BOOST_REQUIRE(instance.get(1u, parent));
    BOOST_REQUIRE_EQUAL(parent.parent_fk, 0x0a0b0c0d_u32);
    BOOST_REQUIRE(!parent.coinbase);
}

BOOST_AUTO_TEST_CASE(outs__get_excluded__two__second_only)
{
    auto head = base16_chunk("02000000");
    auto body = expected_body;
    test::chunk_storage head_store{ head, test::terminal_fill };
    test::chunk_storage body_store{ body };
    table::outs instance{ head_store, body_store };

    table::outs::get_excluded excluded{ {}, 2u };
    BOOST_REQUIRE(instance.get(0u, excluded));
    BOOST_REQUIRE_EQUAL(excluded.excluded.size(), 1u);
    BOOST_REQUIRE_EQUAL(excluded.excluded.front().first, 1u);
    BOOST_REQUIRE_EQUAL(excluded.excluded.front().second, 0x1234567890_u64);
}

BOOST_AUTO_TEST_SUITE_END()
