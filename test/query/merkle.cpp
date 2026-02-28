/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

BOOST_FIXTURE_TEST_SUITE(query_merkle_tests, test::directory_setup_fixture)

// nop event handler.
const auto events_handler = [](auto, auto) {};

constexpr auto root01 = system::sha256::double_hash(test::block0_hash, test::block1_hash);
constexpr auto root23 = system::sha256::double_hash(test::block2_hash, test::block3_hash);
constexpr auto root03 = system::sha256::double_hash(root01, root23);
constexpr auto root45 = system::sha256::double_hash(test::block4_hash, test::block5_hash);
constexpr auto root67 = system::sha256::double_hash(test::block6_hash, test::block7_hash);
constexpr auto root47 = system::sha256::double_hash(root45, root67);
constexpr auto root07 = system::sha256::double_hash(root03, root47);

class merkle_accessor
  : public test::query_accessor
{
public:
    using base = test::query_accessor;
    using base::base;
    using base::interval_span;
    using base::create_interval;
    using base::get_confirmed_interval;
    using base::merge_merkle;
    using base::get_merkle_proof;
    using base::get_merkle_subroots;
    using base::get_merkle_root_and_proof;
};

// interval_span

BOOST_AUTO_TEST_CASE(query_merkle__interval_span__uninitialized__max_size_t)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(settings.interval_depth, max_uint8);
    BOOST_CHECK_EQUAL(query.interval_span(), max_size_t);
}

BOOST_AUTO_TEST_CASE(query_merkle__interval_span__11__2048)
{
    settings settings{};
    settings.interval_depth = 11;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK_EQUAL(query.interval_span(), 2048u);
}

BOOST_AUTO_TEST_CASE(query_merkle__interval_span__0__1)
{
    settings settings{};
    settings.interval_depth = 0;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK_EQUAL(query.interval_span(), 1u);
}

// create_interval

BOOST_AUTO_TEST_CASE(query_merkle__create_interval__depth_0__block_hash)
{
    settings settings{};
    settings.interval_depth = 0;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));

    const auto header0 = query.to_header(test::block0_hash);
    const auto header1 = query.to_header(test::block1_hash);
    const auto header2 = query.to_header(test::block2_hash);
    const auto header3 = query.to_header(test::block3_hash);
    BOOST_CHECK(!header0.is_terminal());
    BOOST_CHECK(!header1.is_terminal());
    BOOST_CHECK(!header2.is_terminal());
    BOOST_CHECK(!header3.is_terminal());
    BOOST_CHECK(query.create_interval(header0, 0).has_value());
    BOOST_CHECK(query.create_interval(header1, 1).has_value());
    BOOST_CHECK(query.create_interval(header2, 2).has_value());
    BOOST_CHECK(query.create_interval(header3, 3).has_value());
    BOOST_CHECK_EQUAL(query.create_interval(header0, 0).value(), test::block0_hash);
    BOOST_CHECK_EQUAL(query.create_interval(header1, 1).value(), test::block1_hash);
    BOOST_CHECK_EQUAL(query.create_interval(header2, 2).value(), test::block2_hash);
    BOOST_CHECK_EQUAL(query.create_interval(header3, 3).value(), test::block3_hash);
}

BOOST_AUTO_TEST_CASE(query_merkle__create_interval__depth_1__expected)
{
    settings settings{};
    settings.interval_depth = 1;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));

    const auto header0 = query.to_header(test::block0_hash);
    const auto header1 = query.to_header(test::block1_hash);
    const auto header2 = query.to_header(test::block2_hash);
    const auto header3 = query.to_header(test::block3_hash);
    BOOST_CHECK(!header0.is_terminal());
    BOOST_CHECK(!header1.is_terminal());
    BOOST_CHECK(!header2.is_terminal());
    BOOST_CHECK(!header3.is_terminal());
    BOOST_CHECK(!query.create_interval(header0, 0).has_value());
    BOOST_CHECK( query.create_interval(header1, 1).has_value());
    BOOST_CHECK(!query.create_interval(header2, 2).has_value());
    BOOST_CHECK( query.create_interval(header3, 3).has_value());
    BOOST_CHECK_EQUAL(query.create_interval(header1, 1).value(), root01);
    BOOST_CHECK_EQUAL(query.create_interval(header3, 3).value(), root23);
}

BOOST_AUTO_TEST_CASE(query_merkle__create_interval__depth_2__expected)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));

    const auto header3 = query.to_header(test::block3_hash);
    BOOST_CHECK(!header3.is_terminal());
    BOOST_CHECK(query.create_interval(header3, 3).has_value());
    BOOST_CHECK_EQUAL(query.create_interval(header3, 3).value(), root03);
}

// get_confirmed_interval

BOOST_AUTO_TEST_CASE(query_merkle__get_confirmed_interval__not_multiple__no_value)
{
    settings settings{};
    settings.interval_depth = 3;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(query.interval_span(), system::power2(settings.interval_depth));
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(!query.get_confirmed_interval(0).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(1).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(6).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(7).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(14).has_value());
}

// Interval is set by create_interval(), integral to set(block).
BOOST_AUTO_TEST_CASE(query_merkle__get_confirmed_interval__multiple__expected_value)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(query.interval_span(), system::power2(settings.interval_depth));
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));
    BOOST_CHECK(!query.get_confirmed_interval(0).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(1).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(2).has_value());
    BOOST_CHECK( query.get_confirmed_interval(3).has_value());
    BOOST_CHECK(!query.get_confirmed_interval(4).has_value());
}

// merge_merkle

BOOST_AUTO_TEST_CASE(query_merkle__merge_merkle__empty_from__expected)
{
    hashes to{};
    merkle_accessor::merge_merkle(to, {}, 0);
    BOOST_CHECK(to.empty());
}

BOOST_AUTO_TEST_CASE(query_merkle__merge_merkle__one_hash__empty)
{
    hashes to{};
    merkle_accessor::merge_merkle(to, { system::null_hash }, 0);
    BOOST_CHECK(to.empty());
}

BOOST_AUTO_TEST_CASE(query_merkle__push_merkle__two_leaves_target_zero__expected)
{
    hashes to{};
    hashes from
    {
        test::block0_hash,
        test::block1_hash
    };

    merkle_accessor::merge_merkle(to, std::move(from), 0);
    BOOST_CHECK_EQUAL(to.size(), 1u);
    BOOST_CHECK_EQUAL(to[0], test::block1_hash);
}

BOOST_AUTO_TEST_CASE(query_merkle__merge_merkle__three_leaves_target_two__expected)
{
    hashes to{};
    hashes from
    {
        test::block0_hash,
        test::block1_hash,
        test::block2_hash
    };

    merkle_accessor::merge_merkle(to, std::move(from), 2);
    BOOST_CHECK_EQUAL(to.size(), 2u);
    BOOST_CHECK_EQUAL(to[0], test::block2_hash);
    BOOST_CHECK_EQUAL(to[1], root01);
}

BOOST_AUTO_TEST_CASE(query_merkle__merge_merkle__four_leaves_target_three__expected)
{
    hashes to{};
    hashes from
    {
        test::block0_hash,
        test::block1_hash,
        test::block2_hash,
        test::block3_hash
    };

    merkle_accessor::merge_merkle(to, std::move(from), 3);
    BOOST_CHECK_EQUAL(to.size(), 2u);
    BOOST_CHECK_EQUAL(to[0], test::block2_hash);
    BOOST_CHECK_EQUAL(to[1], root01);
}

// get_merkle_proof

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_proof__no_confirmed_blocks__error_merkle_proof)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));

    hashes proof{};
    BOOST_CHECK_EQUAL(query.get_merkle_proof(proof, {}, 5, 10), error::merkle_proof);
    BOOST_CHECK(proof.empty());
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_proof__target_in_first_interval__expected)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));

    hashes proof{};
    BOOST_CHECK_EQUAL(query.get_merkle_proof(proof, {}, 3u, 3u), error::success);
    BOOST_CHECK_EQUAL(proof.size(), 2u);
    BOOST_CHECK_EQUAL(proof[0], test::block2_hash);
    BOOST_CHECK_EQUAL(proof[1], root01);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_proof__multiple_intervals__expected)
{
    settings settings{};
    settings.interval_depth = 1;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));

    hashes proof{};
    const hashes roots
    {
        root01,
        root23
    };
    BOOST_CHECK_EQUAL(query.get_merkle_proof(proof, roots, 3u, 3u), error::success);
    BOOST_CHECK_EQUAL(proof.size(), 2u);
    BOOST_CHECK_EQUAL(proof[0], test::block2_hash);
    BOOST_CHECK_EQUAL(proof[1], root01);
}

// get_merkle_subroots

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_subroots__waypoint_zero__genesis)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));

    hashes roots{};
    BOOST_CHECK_EQUAL(query.get_merkle_subroots(roots, 0), error::success);
    BOOST_CHECK_EQUAL(roots.size(), 1u);

    // At depth 2, 1 hash must be elevated twice, and paired twice.
    constexpr auto root00 = system::sha256::double_hash(test::block0_hash, test::block0_hash);
    constexpr auto root0000 = system::sha256::double_hash(root00, root00);
    BOOST_CHECK_EQUAL(roots[0], root0000);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_subroots__one_full_interval__expected_root)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));

    hashes roots{};
    BOOST_CHECK_EQUAL(query.get_merkle_subroots(roots, 3), error::success);
    BOOST_CHECK_EQUAL(roots.size(), 1u);

    // At depth 2, the 4th position (block 3) results in a simple root.
    BOOST_CHECK_EQUAL(roots[0], root03);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_subroots__full_and_partial_interval__expected_two_roots)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.set(test::block4, context{ 0, 4, 0 }, false, false));
    BOOST_CHECK(query.set(test::block5, context{ 0, 5, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block4_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block5_hash), false));

    hashes roots{};
    BOOST_CHECK_EQUAL(query.get_merkle_subroots(roots, 5), error::success);
    BOOST_CHECK_EQUAL(roots.size(), 2u);
    BOOST_CHECK_EQUAL(roots[0], root03);

    // At depth 2, the 6th position (block 5) results in one complete and one partial root.
    constexpr auto root45 = system::sha256::double_hash(test::block4_hash, test::block5_hash);
    constexpr auto root4545 = system::sha256::double_hash(root45, root45);
    BOOST_CHECK_EQUAL(roots[1], root4545);
}

// get_merkle_root_and_proof
// get_merkle_proof

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_root_and_proof__target_equals_waypoint__success)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));

    hashes proof{};
    hash_digest root{};
    BOOST_CHECK(!query.get_merkle_root_and_proof(root, proof, 3, 3));
    BOOST_CHECK_EQUAL(proof.size(), 2u);
    BOOST_CHECK_EQUAL(proof[0], test::block2_hash);
    BOOST_CHECK_EQUAL(proof[1], root01);
    BOOST_CHECK_EQUAL(root, query.get_merkle_root(3));
    BOOST_CHECK_EQUAL(root, root03);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_root_and_proof__target_less_than_waypoint__success)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));

    hashes proof{};
    hash_digest root{};
    BOOST_CHECK(!query.get_merkle_root_and_proof(root, proof, 1, 3));
    BOOST_CHECK_EQUAL(proof.size(), 2u);
    BOOST_CHECK_EQUAL(proof[0], test::block0_hash);
    BOOST_CHECK_EQUAL(proof[1], root23);
    BOOST_CHECK_EQUAL(root, query.get_merkle_root(3));
    BOOST_CHECK_EQUAL(root, root03);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_root_and_proof__electrumx_example__success)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, context{ 0, 1, 0 }, false, false));
    BOOST_CHECK(query.set(test::block2, context{ 0, 2, 0 }, false, false));
    BOOST_CHECK(query.set(test::block3, context{ 0, 3, 0 }, false, false));
    BOOST_CHECK(query.set(test::block4, context{ 0, 4, 0 }, false, false));
    BOOST_CHECK(query.set(test::block5, context{ 0, 5, 0 }, false, false));
    BOOST_CHECK(query.set(test::block6, context{ 0, 6, 0 }, false, false));
    BOOST_CHECK(query.set(test::block7, context{ 0, 7, 0 }, false, false));
    BOOST_CHECK(query.set(test::block8, context{ 0, 8, 0 }, false, false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block1_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block2_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block3_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block4_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block5_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block6_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block7_hash), false));
    BOOST_CHECK(query.push_confirmed(query.to_header(test::block8_hash), false));

    // Example vector from electrumx documentation.
    // electrumx.readthedocs.io/en/latest/protocol-methods.html#cp-height-example
    //{
    //  "branch":
    //  [
    //     "000000004ebadb55ee9096c9a2f8880e09da59c0d68b1c228da88e48844a1485",
    //     "96cbbc84783888e4cc971ae8acf86dd3c1a419370336bb3c634c97695a8c5ac9",
    //     "965ac94082cebbcffe458075651e9cc33ce703ab0115c72d9e8b1a9906b2b636",
    //     "89e5daa6950b895190716dd26054432b564ccdc2868188ba1da76de8e1dc7591"
    //  ],
    //  "root  ": "e347b1c43fd9b5415bf0d92708db8284b78daf4d0e24f9c3405f45feb85e25db"
    //}
    constexpr auto root82 = system::sha256::double_hash(test::block8_hash, test::block8_hash);
    constexpr auto root84 = system::sha256::double_hash(root82, root82);
    constexpr auto root88 = system::sha256::double_hash(root84, root84);
    constexpr auto root08 = system::sha256::double_hash(root07, root88);
    static_assert(root08 == system::base16_hash("e347b1c43fd9b5415bf0d92708db8284b78daf4d0e24f9c3405f45feb85e25db"));
    BOOST_CHECK_EQUAL(query.get_merkle_root(8), root08);

    hashes proof{};
    hash_digest root{};
    BOOST_CHECK(!query.get_merkle_root_and_proof(root, proof, 5, 8));
    BOOST_CHECK_EQUAL(root, root08);
    BOOST_CHECK_EQUAL(proof.size(), 4u);
    BOOST_CHECK_EQUAL(proof[0], system::base16_hash("000000004ebadb55ee9096c9a2f8880e09da59c0d68b1c228da88e48844a1485"));
    BOOST_CHECK_EQUAL(proof[1], system::base16_hash("96cbbc84783888e4cc971ae8acf86dd3c1a419370336bb3c634c97695a8c5ac9"));
    BOOST_CHECK_EQUAL(proof[2], system::base16_hash("965ac94082cebbcffe458075651e9cc33ce703ab0115c72d9e8b1a9906b2b636"));
    BOOST_CHECK_EQUAL(proof[3], system::base16_hash("89e5daa6950b895190716dd26054432b564ccdc2868188ba1da76de8e1dc7591"));
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_root_and_proof__target_greater_than_waypoint__error_invalid_argument)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));

    hashes proof{};
    hash_digest root{};
    BOOST_CHECK_EQUAL(query.get_merkle_root_and_proof(root, proof, 5, 3), error::invalid_argument);
    BOOST_CHECK_EQUAL(query.get_merkle_root(3), system::null_hash);
}

BOOST_AUTO_TEST_CASE(query_merkle__get_merkle_root_and_proof__waypoint_beyond_top__error_not_found)
{
    settings settings{};
    settings.interval_depth = 2;
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    merkle_accessor query{ store };
    BOOST_CHECK_EQUAL(store.create(events_handler), error::success);
    BOOST_CHECK(query.initialize(test::genesis));

    hashes proof{};
    hash_digest root{};
    BOOST_CHECK_EQUAL(query.get_merkle_root_and_proof(root, proof, 0, 100), error::not_found);
    BOOST_CHECK_EQUAL(query.get_merkle_root(100), system::null_hash);
}

BOOST_AUTO_TEST_SUITE_END()
