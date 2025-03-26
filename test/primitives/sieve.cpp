/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(sieve_tests)

using namespace system;

BOOST_AUTO_TEST_CASE(sieve__screen__zeroed__always_screened)
{
    // Ensure default/nop behavior.
    static_assert(sieve<0, 0>{}.screened(42));
    static_assert(sieve<1, 0>{}.screened(42));
    static_assert(!sieve<1, 1>{}.screened(42));

    sieve<0, 0> sieve0{};
    BOOST_REQUIRE(sieve0.screened(42));
    BOOST_REQUIRE(!sieve0.screen(42));

    sieve<1, 0> sieve1{};
    BOOST_REQUIRE(sieve1.screened(42));
    BOOST_REQUIRE(!sieve1.screen(42));

    sieve<1, 1> sieve2{};
    BOOST_REQUIRE(!sieve2.screened(42));
    BOOST_REQUIRE(sieve2.screen(42));
}

#if defined(HAVE_SLOW_TESTS)

BOOST_AUTO_TEST_CASE(sieve__screen__4_bits_forward__16_screens)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);

    sieve<sieve_bits, selector_bits> sieve{};
    size_t count{};

    // This exhausts the full uint32_t domain and returns exactly selector_max values.
    // The count < selector_max condition just speeds up the test, still slow though.
    for (uint32_t value{}; count < selector_max; ++value)
    {
        if (sieve.screen(value))
        {
            ////std::cout << "0b" << binary{ sieve_bits, to_big(value) }
            ////    << "_u32, // " << count << std::endl;
            count++;
        }

        if (value == max_uint32)
            break;
    }

    BOOST_CHECK_EQUAL(count, selector_max);
}

BOOST_AUTO_TEST_CASE(sieve__screen__4_bits_reverse__16_screens)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);
    sieve<sieve_bits, selector_bits> sieve{};
    size_t count{};

    // This exhausts the full uint32_t domain and returns exactly selector_max values.
    // The count < selector_max condition just speeds up the test, still slow though.
    for (auto value = max_uint32; count < selector_max; --value)
    {
        if (sieve.screen(value))
        {
            ////std::cout << "0b" << binary{ sieve_bits, to_big(value) }
            ////    << "_u32, // " << count << std::endl;
            count++;
        }

        if (is_zero(value))
            break;
    }

    BOOST_CHECK_EQUAL(count, selector_max);
}

#endif // HAVE_SLOW_TESTS

BOOST_AUTO_TEST_CASE(sieve__screen__full_unsaturated__fully_screened)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);

    // This generation is based on the manually-generated matrix.
    constexpr std_array<uint32_t, selector_max> forward
    {
        0b00000000000000000000000000000000_u32, // 0
        0b00000000000000000000000000000001_u32, // 1
        0b00000000000000000100000000000000_u32, // 2
        0b00000000000001000000000000100000_u32, // 3
        0b00000000001000000000000010000000_u32, // 4
        0b00000000010000000000000100000000_u32, // 5
        0b00000000100000000000001000000000_u32, // 6
        0b00000001000000000000010000000000_u32, // 7
        0b00000001000000000000010000000001_u32, // 8
        0b00000001000000001000100000000001_u32, // 9
        0b00000010000000001000100000000001_u32, // 10
        0b00000010000000001000100000000010_u32, // 11
        0b00000010000000001000100000000110_u32, // 12
        0b00000010000010010000100000000110_u32, // 13
        0b00000100000010010001000000000110_u32, // 14
        0b00000100000010010001100000000110_u32  // 15
    };

    sieve<sieve_bits, selector_bits> sieve{};
    BOOST_CHECK(sieve.screen(forward[0]));
    BOOST_CHECK(sieve.screen(forward[1]));
    BOOST_CHECK(sieve.screen(forward[2]));
    BOOST_CHECK(sieve.screen(forward[3]));
    BOOST_CHECK(sieve.screen(forward[4]));
    BOOST_CHECK(sieve.screen(forward[5]));
    BOOST_CHECK(sieve.screen(forward[6]));
    BOOST_CHECK(sieve.screen(forward[7]));
    BOOST_CHECK(sieve.screen(forward[8]));
    BOOST_CHECK(sieve.screen(forward[9]));
    BOOST_CHECK(sieve.screen(forward[10]));
    BOOST_CHECK(sieve.screen(forward[11]));
    BOOST_CHECK(sieve.screen(forward[12]));
    BOOST_CHECK(sieve.screen(forward[13]));
    BOOST_CHECK(sieve.screen(forward[14]));
    BOOST_CHECK(sieve.screen(forward[15]));

    // All values must be screened once saturated.
    BOOST_CHECK(sieve.screened(42));

    // Add after already full saturates the sieve.
    BOOST_CHECK(!sieve.screen(0b00001111111111111111111111111111_u32));

#if !defined(HAVE_SLOW_TESTS)
    // All values must be screened once saturated.
    for (uint32_t value{}; value < max_uint32; ++value)
    {
        if (!sieve.screened(value))
        {
            BOOST_CHECK(false);
        }
    }
#endif // HAVE_SLOW_TESTS
}

BOOST_AUTO_TEST_CASE(sieve__screen__full_saturated__fully_screened)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);

    // This generation is based on the manually-generated matrix.
    constexpr std_array<uint32_t, selector_max> forward
    {
        0b00000000000000000000000000000000_u32, // 0
        0b00000000000000000000000000000001_u32, // 1
        0b00000000000000000100000000000000_u32, // 2
        0b00000000000001000000000000100000_u32, // 3
        0b00000000001000000000000010000000_u32, // 4
        0b00000000010000000000000100000000_u32, // 5
        0b00000000100000000000001000000000_u32, // 6
        0b00000001000000000000010000000000_u32, // 7
        0b00000001000000000000010000000001_u32, // 8
        0b00000001000000001000100000000001_u32, // 9
        0b00000010000000001000100000000001_u32, // 10
        0b00000010000000001000100000000010_u32, // 11
        0b00000010000000001000100000000110_u32, // 12
        0b00000010000010010000100000000110_u32, // 13
        0b00000100000010010001000000000110_u32, // 14
        0b00000100000010010001100000000110_u32  // 15
    };

    sieve<sieve_bits, selector_bits> sieve{};
    BOOST_CHECK(sieve.screen(forward[0]));
    BOOST_CHECK(sieve.screen(forward[1]));
    BOOST_CHECK(sieve.screen(forward[2]));
    BOOST_CHECK(sieve.screen(forward[3]));
    BOOST_CHECK(sieve.screen(forward[4]));
    BOOST_CHECK(sieve.screen(forward[5]));
    BOOST_CHECK(sieve.screen(forward[6]));
    BOOST_CHECK(sieve.screen(forward[7]));
    BOOST_CHECK(sieve.screen(forward[8]));
    BOOST_CHECK(sieve.screen(forward[9]));
    BOOST_CHECK(sieve.screen(forward[10]));
    BOOST_CHECK(sieve.screen(forward[11]));
    BOOST_CHECK(sieve.screen(forward[12]));
    BOOST_CHECK(sieve.screen(forward[13]));
    BOOST_CHECK(sieve.screen(forward[14]));
    BOOST_CHECK(sieve.screen(forward[15]));

    // Add after already full saturates the sieve.
    BOOST_CHECK(!sieve.screen(0b00001111111111111111111111111111_u32));

    // All values must be screened once saturated.
    BOOST_CHECK(sieve.screened(42));

#if defined(HAVE_SLOW_TESTS)
    // All values must be screened once saturated.
    for (uint32_t value{}; value < max_uint32; ++value)
    {
        if (!sieve.screened(value))
        {
            BOOST_CHECK(false);
        }
    }
#endif // HAVE_SLOW_TESTS
}

BOOST_AUTO_TEST_CASE(sieve__screened__forward__expected)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);

    // This generation is based on the manually-generated matrix.
    constexpr std_array<uint32_t, selector_max> forward
    {
        0b00000000000000000000000000000000_u32, // 0
        0b00000000000000000000000000000001_u32, // 1
        0b00000000000000000100000000000000_u32, // 2
        0b00000000000001000000000000100000_u32, // 3
        0b00000000001000000000000010000000_u32, // 4
        0b00000000010000000000000100000000_u32, // 5
        0b00000000100000000000001000000000_u32, // 6
        0b00000001000000000000010000000000_u32, // 7
        0b00000001000000000000010000000001_u32, // 8
        0b00000001000000001000100000000001_u32, // 9
        0b00000010000000001000100000000001_u32, // 10
        0b00000010000000001000100000000010_u32, // 11
        0b00000010000000001000100000000110_u32, // 12
        0b00000010000010010000100000000110_u32, // 13
        0b00000100000010010001000000000110_u32, // 14
        0b00000100000010010001100000000110_u32  // 15
    };

    sieve<sieve_bits, selector_bits> sieve{};

    // none screened
    BOOST_CHECK(!sieve.screened(forward[0]));
    BOOST_CHECK(!sieve.screened(forward[1]));
    BOOST_CHECK(!sieve.screened(forward[2]));
    BOOST_CHECK(!sieve.screened(forward[3]));
    BOOST_CHECK(!sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 0 screened
    BOOST_CHECK( sieve.screen(forward[0]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK(!sieve.screened(forward[1]));
    BOOST_CHECK(!sieve.screened(forward[2]));
    BOOST_CHECK(!sieve.screened(forward[3]));
    BOOST_CHECK(!sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 1 screened
    BOOST_CHECK( sieve.screen(forward[1]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK(!sieve.screened(forward[2]));
    BOOST_CHECK(!sieve.screened(forward[3]));
    BOOST_CHECK(!sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 2 screened
    BOOST_CHECK( sieve.screen(forward[2]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK(!sieve.screened(forward[3]));
    BOOST_CHECK(!sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 3 screened
    BOOST_CHECK( sieve.screen(forward[3]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK(!sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 4 screened
    BOOST_CHECK( sieve.screen(forward[4]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK(!sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 5 screened
    BOOST_CHECK( sieve.screen(forward[5]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK(!sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 6 screened
    BOOST_CHECK( sieve.screen(forward[6]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK(!sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 7 screened
    BOOST_CHECK( sieve.screen(forward[7]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK(!sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 8 screened
    BOOST_CHECK( sieve.screen(forward[8]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK(!sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 9 screened
    BOOST_CHECK( sieve.screen(forward[9]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK(!sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 10 screened
    BOOST_CHECK( sieve.screen(forward[10]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK(!sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 11 screened
    BOOST_CHECK( sieve.screen(forward[11]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK( sieve.screened(forward[11]));
    BOOST_CHECK(!sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 12 screened
    BOOST_CHECK( sieve.screen(forward[12]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK( sieve.screened(forward[11]));
    BOOST_CHECK( sieve.screened(forward[12]));
    BOOST_CHECK(!sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 13 screened
    BOOST_CHECK( sieve.screen(forward[13]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK( sieve.screened(forward[11]));
    BOOST_CHECK( sieve.screened(forward[12]));
    BOOST_CHECK( sieve.screened(forward[13]));
    BOOST_CHECK(!sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 14 screened
    BOOST_CHECK( sieve.screen(forward[14]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK( sieve.screened(forward[11]));
    BOOST_CHECK( sieve.screened(forward[12]));
    BOOST_CHECK( sieve.screened(forward[13]));
    BOOST_CHECK( sieve.screened(forward[14]));
    BOOST_CHECK(!sieve.screened(forward[15]));

    // 15 screened
    BOOST_CHECK( sieve.screen(forward[15]));
    BOOST_CHECK( sieve.screened(forward[0]));
    BOOST_CHECK( sieve.screened(forward[1]));
    BOOST_CHECK( sieve.screened(forward[2]));
    BOOST_CHECK( sieve.screened(forward[3]));
    BOOST_CHECK( sieve.screened(forward[4]));
    BOOST_CHECK( sieve.screened(forward[5]));
    BOOST_CHECK( sieve.screened(forward[6]));
    BOOST_CHECK( sieve.screened(forward[7]));
    BOOST_CHECK( sieve.screened(forward[8]));
    BOOST_CHECK( sieve.screened(forward[9]));
    BOOST_CHECK( sieve.screened(forward[10]));
    BOOST_CHECK( sieve.screened(forward[11]));
    BOOST_CHECK( sieve.screened(forward[12]));
    BOOST_CHECK( sieve.screened(forward[13]));
    BOOST_CHECK( sieve.screened(forward[14]));
    BOOST_CHECK( sieve.screened(forward[15]));
}

BOOST_AUTO_TEST_CASE(sieve__screened__reverse__expected)
{
    constexpr size_t sieve_bits = 32;
    constexpr size_t selector_bits = 4;
    constexpr size_t selector_max = power2(selector_bits);

    // This generation is based on the manually-generated matrix.
    constexpr std_array<uint32_t, selector_max> reverse
    {
        0b11111111111111111111111111111111_u32, // 0
        0b11111111111111111111111111111110_u32, // 1
        0b11111111111111111011111111111111_u32, // 2
        0b11111111111110111111111111011111_u32, // 3
        0b11111111110111111111111101111111_u32, // 4
        0b11111111101111111111111011111111_u32, // 5
        0b11111111011111111111110111111111_u32, // 6
        0b11111110111111111111101111111111_u32, // 7
        0b11111110111111111111101111111110_u32, // 8
        0b11111110111111110111011111111110_u32, // 9
        0b11111101111111110111011111111110_u32, // 10
        0b11111101111111110111011111111101_u32, // 11
        0b11111101111111110111011111111001_u32, // 12
        0b11111101111101101111011111111001_u32, // 13
        0b11111011111101101110111111111001_u32, // 14
        0b11111011111101101110011111111001_u32  // 15
    };

    sieve<sieve_bits, selector_bits> sieve{};

    // none screened
    BOOST_CHECK(!sieve.screened(reverse[0]));
    BOOST_CHECK(!sieve.screened(reverse[1]));
    BOOST_CHECK(!sieve.screened(reverse[2]));
    BOOST_CHECK(!sieve.screened(reverse[3]));
    BOOST_CHECK(!sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 0 screened
    BOOST_CHECK( sieve.screen(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK(!sieve.screened(reverse[1]));
    BOOST_CHECK(!sieve.screened(reverse[2]));
    BOOST_CHECK(!sieve.screened(reverse[3]));
    BOOST_CHECK(!sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 1 screened
    BOOST_CHECK( sieve.screen(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK(!sieve.screened(reverse[2]));
    BOOST_CHECK(!sieve.screened(reverse[3]));
    BOOST_CHECK(!sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 2 screened
    BOOST_CHECK( sieve.screen(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK(!sieve.screened(reverse[3]));
    BOOST_CHECK(!sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 3 screened
    BOOST_CHECK( sieve.screen(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK(!sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 4 screened
    BOOST_CHECK( sieve.screen(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK(!sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 5 screened
    BOOST_CHECK( sieve.screen(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK(!sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 6 screened
    BOOST_CHECK( sieve.screen(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK(!sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 7 screened
    BOOST_CHECK( sieve.screen(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK(!sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 8 screened
    BOOST_CHECK( sieve.screen(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK(!sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 9 screened
    BOOST_CHECK( sieve.screen(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK(!sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 10 screened
    BOOST_CHECK( sieve.screen(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK(!sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 11 screened
    BOOST_CHECK( sieve.screen(reverse[11]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[11]));
    BOOST_CHECK(!sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 12 screened
    BOOST_CHECK( sieve.screen(reverse[12]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[11]));
    BOOST_CHECK( sieve.screened(reverse[12]));
    BOOST_CHECK(!sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 13 screened
    BOOST_CHECK( sieve.screen(reverse[13]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[11]));
    BOOST_CHECK( sieve.screened(reverse[12]));
    BOOST_CHECK( sieve.screened(reverse[13]));
    BOOST_CHECK(!sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 14 screened
    BOOST_CHECK( sieve.screen(reverse[14]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[11]));
    BOOST_CHECK( sieve.screened(reverse[12]));
    BOOST_CHECK( sieve.screened(reverse[13]));
    BOOST_CHECK( sieve.screened(reverse[14]));
    BOOST_CHECK(!sieve.screened(reverse[15]));

    // 15 screened
    BOOST_CHECK( sieve.screen(reverse[15]));
    BOOST_CHECK( sieve.screened(reverse[0]));
    BOOST_CHECK( sieve.screened(reverse[1]));
    BOOST_CHECK( sieve.screened(reverse[2]));
    BOOST_CHECK( sieve.screened(reverse[3]));
    BOOST_CHECK( sieve.screened(reverse[4]));
    BOOST_CHECK( sieve.screened(reverse[5]));
    BOOST_CHECK( sieve.screened(reverse[6]));
    BOOST_CHECK( sieve.screened(reverse[7]));
    BOOST_CHECK( sieve.screened(reverse[8]));
    BOOST_CHECK( sieve.screened(reverse[9]));
    BOOST_CHECK( sieve.screened(reverse[10]));
    BOOST_CHECK( sieve.screened(reverse[11]));
    BOOST_CHECK( sieve.screened(reverse[12]));
    BOOST_CHECK( sieve.screened(reverse[13]));
    BOOST_CHECK( sieve.screened(reverse[14]));
    BOOST_CHECK( sieve.screened(reverse[15]));
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(DISABLED)

#include <bitset>
void print_table() const NOEXCEPT
{
    for (size_t r = 0; r < screens; ++r)
    {
        std::cout << "Row " << (r + 1) << ":\n";
        for (size_t c = 0; c < screens; ++c)
        {
            if (c <= r)
            {
                const auto output = std::bitset<screen_bits>(masks(r, c));

                std::cout << "  Col " << c << ": " << output << " ("
                    << std::popcount(masks(r, c)) << " bits)\n";
            }
        }
    }
}

static constexpr masks_t masks1_
{
    // 1
    0b0000'1111111111111111111111111111,

    // 2
    0b0000'1111111111111100000000000000,
    0b0000'0000000000000011111111111111,

    // 3
    0b0000'1111111111000000000000000000,
    0b0000'0000000000000011111111100000,
    0b0000'0000000000111100000000011111,

    // 4
    0b0000'1111111000000000000000000000,
    0b0000'0000000000000011111110000000,
    0b0000'0000000000111100000000011100,
    0b0000'0000000111000000000001100011,

    // 5
    0b0000'1111110000000000000000000000,
    0b0000'0000000000000011111100000000,
    0b0000'0000000000111100000000011000,
    0b0000'0000000111000000000001100000,
    0b0000'0000001000000000000010000111,

    // 6
    0b0000'1111100000000000000000000000,
    0b0000'0000000000000011111000000000,
    0b0000'0000000000111100000000010000,
    0b0000'0000000111000000000001100000,
    0b0000'0000001000000000000010000110,
    0b0000'0000010000000000000100001001,

    // 7
    0b0000'1111000000000000000000000000,
    0b0000'0000000000000011110000000000,
    0b0000'0000000000111100000000000000,
    0b0000'0000000111000000000001000000,
    0b0000'0000001000000000000010000110,
    0b0000'0000010000000000000100001001,
    0b0000'0000100000000000001000110000,

    // 8
    0b0000'1111000000000000000000000000,
    0b0000'0000000000000011110000000000,
    0b0000'0000000000111100000000000000,
    0b0000'0000000111000000000001000000,
    0b0000'0000001000000000000010000100,
    0b0000'0000010000000000000100001000,
    0b0000'0000100000000000001000100000,
    0b0000'0000000000000000000000010011,

    // 9
    0b0000'1111000000000000000000000000,
    0b0000'0000000000000011100000000000,
    0b0000'0000000000111000000000000000,
    0b0000'0000000111000000000000000000,
    0b0000'0000001000000000000010000100,
    0b0000'0000010000000000000100001000,
    0b0000'0000100000000000001000100000,
    0b0000'0000000000000000000000010011,
    0b0000'0000000000000100010001000000,

    // 10
    0b0000'1110000000000000000000000000,
    0b0000'0000000000000011100000000000,
    0b0000'0000000000111000000000000000,
    0b0000'0000000111000000000000000000,
    0b0000'0000001000000000000010000100,
    0b0000'0000010000000000000100001000,
    0b0000'0000100000000000001000100000,
    0b0000'0000000000000000000000010011,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,

    // 11
    0b0000'1110000000000000000000000000,
    0b0000'0000000000000011100000000000,
    0b0000'0000000000111000000000000000,
    0b0000'0000000111000000000000000000,
    0b0000'0000001000000000000010000100,
    0b0000'0000010000000000000100001000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,

    // 12
    0b0000'1110000000000000000000000000,
    0b0000'0000000000000011100000000000,
    0b0000'0000000000111000000000000000,
    0b0000'0000000111000000000000000000,
    0b0000'0000001000000000000010000000,
    0b0000'0000010000000000000100000000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,
    0b0000'0000000000000000000000001100,

    // 13
    0b0000'1110000000000000000000000000,
    0b0000'0000000000000011100000000000,
    0b0000'0000000000110000000000000000,
    0b0000'0000000110000000000000000000,
    0b0000'0000001000000000000010000000,
    0b0000'0000010000000000000100000000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,
    0b0000'0000000000000000000000001100,
    0b0000'0000000001001000000000000000,

    // 14
    0b0000'1100000000000000000000000000,
    0b0000'0000000000000011000000000000,
    0b0000'0000000000110000000000000000,
    0b0000'0000000110000000000000000000,
    0b0000'0000001000000000000010000000,
    0b0000'0000010000000000000100000000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,
    0b0000'0000000000000000000000001100,
    0b0000'0000000001001000000000000000,
    0b0000'0010000000000000100000000000,

    // 15
    0b0000'1100000000000000000000000000,
    0b0000'0000000000000011000000000000,
    0b0000'0000000000110000000000000000,
    0b0000'0000000110000000000000000000,
    0b0000'0000001000000000000010000000,
    0b0000'0000010000000000000100000000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,
    0b0000'0000000000000000000000001100,
    0b0000'0000000001001000000000000000,
    0b0000'0010000000000000000000000000,
    0b0000'0000000000000000100000000000,

    // 16
    // first row first bit sacrificed for sentinel.
    0b0000'0'100000000000000000000000000,
    0b0000'0000000000000011000000000000,
    0b0000'0000000000110000000000000000,
    0b0000'0000000110000000000000000000,
    0b0000'0000001000000000000010000000,
    0b0000'0000010000000000000100000000,
    0b0000'0000100000000000001000000000,
    0b0000'0000000000000000000000010010,
    0b0000'0000000000000100010000000000,
    0b0000'0001000000000000000001000000,
    0b0000'0000000000000000000000100001,
    0b0000'0000000000000000000000001100,
    0b0000'0000000001000000000000000000,
    0b0000'0010000000000000000000000000,
    0b0000'0000000000000000100000000000,
    0b0000'0000000000001000000000000000
};
#endif // DISABLED
