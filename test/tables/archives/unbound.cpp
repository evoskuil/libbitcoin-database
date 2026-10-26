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

BOOST_AUTO_TEST_SUITE(unbound_tests)

using namespace system;
const table::unbound::record unbound1{ {}, chain::point{ one_hash, 0x00123456_u32 } };
const table::unbound::record unbound2{ {}, chain::point{ null_hash, chain::point::null_index } };
const auto key1 = table::unbound::to_key(0x00000042_u32);
const auto key2 = table::unbound::to_key(0x00000043_u32);

BOOST_AUTO_TEST_CASE(unbound__put__two__found)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::unbound instance{ head_store, body_store, 8 };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.first(key1).is_terminal());
    BOOST_REQUIRE(instance.put(key1, unbound1));
    BOOST_REQUIRE(instance.put(key2, unbound2));
    BOOST_REQUIRE_EQUAL(instance.count(), 2u);

    table::unbound::record out{};
    BOOST_REQUIRE(instance.get(instance.first(key1), out));
    BOOST_REQUIRE(out == unbound1);
    BOOST_REQUIRE(instance.get(instance.first(key2), out));
    BOOST_REQUIRE(out == unbound2);
    BOOST_REQUIRE(out.key.is_null());
}

BOOST_AUTO_TEST_CASE(unbound__first__absent__terminal)
{
    test::chunk_storage head_store{ test::terminal_fill };
    test::chunk_storage body_store{};
    table::unbound instance{ head_store, body_store, 8 };
    BOOST_REQUIRE(instance.create());
    BOOST_REQUIRE(instance.put(key1, unbound1));
    BOOST_REQUIRE(instance.first(key2).is_terminal());
}

BOOST_AUTO_TEST_SUITE_END()
