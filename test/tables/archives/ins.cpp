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

BOOST_AUTO_TEST_SUITE(ins_tests)

using namespace system;
const table::ins::record in1{ {}, 0x01020304_u32, 0x0505050505_u64, 0x0a0b0c0d_u32 };
const table::ins::record in2{ {}, 0xffffffff_u32, 0x0000000042_u64, 0x00000001_u32 };
const auto expected_head = base16_chunk
(
    "00000000"
);
const auto closed_head = base16_chunk
(
    "02000000"
);
const auto expected_body = base16_chunk
(
    "04030201"   // sequence1
    "0505050505" // input_fk1
    "0d0c0b0a"   // parent_fk1
    "ffffffff"   // sequence2
    "4200000000" // input_fk2
    "01000000"   // parent_fk2
);

BOOST_AUTO_TEST_CASE(ins__put__two__expected)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::ins instance{ head_store, body_store };
    BOOST_REQUIRE(instance.create());

    table::ins::link link1{};
    table::ins::link link2{};
    BOOST_REQUIRE(instance.put_link(link1, in1));
    BOOST_REQUIRE(instance.put_link(link2, in2));
    BOOST_REQUIRE_EQUAL(link1, 0u);
    BOOST_REQUIRE_EQUAL(link2, 1u);
    BOOST_REQUIRE_EQUAL(head_store.buffer(), expected_head);
    BOOST_REQUIRE_EQUAL(body_store.buffer(), expected_body);
    BOOST_REQUIRE(instance.close());
    BOOST_REQUIRE_EQUAL(head_store.buffer(), closed_head);
}

BOOST_AUTO_TEST_CASE(ins__get__two__expected)
{
    auto head = expected_head;
    auto body = expected_body;
    test::chunk_storage head_store{ head, test::terminal_fill };
    test::chunk_storage body_store{ body };
    table::ins instance{ head_store, body_store };

    table::ins::record out{};
    BOOST_REQUIRE(instance.get(0u, out));
    BOOST_REQUIRE(out == in1);
    BOOST_REQUIRE(instance.get(1u, out));
    BOOST_REQUIRE(out == in2);

    table::ins::get_parent parent{};
    BOOST_REQUIRE(instance.get(0u, parent));
    BOOST_REQUIRE_EQUAL(parent.parent_fk, 0x0a0b0c0d_u32);

    table::ins::get_input input{};
    BOOST_REQUIRE(instance.get(1u, input));
    BOOST_REQUIRE_EQUAL(input.sequence, 0xffffffff_u32);
    BOOST_REQUIRE_EQUAL(input.input_fk, 0x0000000042_u64);
}

BOOST_AUTO_TEST_SUITE_END()
