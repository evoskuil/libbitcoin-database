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

BOOST_FIXTURE_TEST_SUITE(query_bindings_tests, test::directory_setup_fixture)

using namespace system::chain;
using context = database::context;

static transaction make_coinbase() NOEXCEPT
{
    const script nops{ { { opcode::nop }, { opcode::nop } } };
    const inputs ins{ input{ point{}, nops, witness{}, max_uint32 } };
    return { 1, ins, outputs{ output{ 1, script{} } }, 0 };
}

// bind (test helper)
// ----------------------------------------------------------------------------

static code bind(test::query_accessor& query,
    const header_link& link) NOEXCEPT
{
    auto view = query.get_block_view(link, true);
    if (const auto ec = view.populate(system::chain::context{}))
        return ec;

    const auto populated = query.populate_with_metadata(link, view);
    const auto bound = query.bind(link, view);
    return populated ? populated : bound;
}

// spend
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(query_bindings__spend__unspent_output__bound)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));

    const auto parent = query.to_tx(test::block1a.transactions_ptr()->front()->hash(false));
    const auto spender = query.to_tx(test::block_spend_1a.transactions_ptr()->back()->hash(false));
    const auto spent = query.to_output(parent, 0);
    const auto in = query.to_point(spender, 0);
    BOOST_REQUIRE(!parent.is_terminal());
    BOOST_REQUIRE(!spender.is_terminal());
    BOOST_REQUIRE(!spent.is_terminal());
    BOOST_REQUIRE(!in.is_terminal());

    // Unbound (the point resolves by hash, but nothing spends the output).
    BOOST_REQUIRE(!query.has_spender(spent));
    BOOST_REQUIRE(!query.is_spent(spent));
    BOOST_REQUIRE(query.to_spender_txs(spent).empty());
    BOOST_REQUIRE(query.to_spenders(spent).empty());
    BOOST_REQUIRE_EQUAL(query.to_spent(in), spent);

    BOOST_REQUIRE(query.spend(in, spent));
    BOOST_REQUIRE(query.has_spender(spent));
    BOOST_REQUIRE(!query.is_contested(spent));
    BOOST_REQUIRE(query.is_spent(spent));
    BOOST_REQUIRE_EQUAL(query.to_spent(in), spent);
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(spent), tx_links{ spender });
    BOOST_REQUIRE_EQUAL(query.to_spenders(spent), ins_links{ in });
    BOOST_REQUIRE_EQUAL(query.to_spending_input(spender, spent), in);
    BOOST_REQUIRE_EQUAL(query.spender_records(), one);
}

BOOST_AUTO_TEST_CASE(query_bindings__spend__rebind__unchanged)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));

    const auto parent = query.to_tx(test::block1a.transactions_ptr()->front()->hash(false));
    const auto spender = query.to_tx(test::block_spend_1a.transactions_ptr()->back()->hash(false));
    const auto spent = query.to_output(parent, 0);
    const auto in = query.to_point(spender, 0);
    BOOST_REQUIRE(query.spend(in, spent));
    BOOST_REQUIRE(query.spend(in, spent));
    BOOST_REQUIRE(query.has_spender(spent));
    BOOST_REQUIRE(!query.is_contested(spent));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(spent), tx_links{ spender });
    BOOST_REQUIRE_EQUAL(query.spender_records(), one);
}

BOOST_AUTO_TEST_CASE(query_bindings__spend__second_spender__contested)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));

    // tx5 also spends the first block1a output (bound at archive).
    BOOST_REQUIRE(query.set(test::tx5));

    const auto parent = query.to_tx(test::block1a.transactions_ptr()->front()->hash(false));
    const auto spender1 = query.to_tx(test::block_spend_1a.transactions_ptr()->back()->hash(false));
    const auto spender2 = query.to_tx(test::tx5.hash(false));
    const auto spent = query.to_output(parent, 0);
    const auto in1 = query.to_point(spender1, 0);
    const auto in2 = query.to_point(spender2, 0);
    BOOST_REQUIRE(query.spend(in1, spent));

    // Each spender is a row, the second binding contests the output.
    BOOST_REQUIRE(query.has_spender(spent));
    BOOST_REQUIRE(query.is_contested(spent));
    BOOST_REQUIRE_EQUAL(query.spender_records(), two);
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(spent), (tx_links{ spender1, spender2 }));
    BOOST_REQUIRE_EQUAL(query.to_spenders(spent), (ins_links{ in2, in1 }));
    BOOST_REQUIRE_EQUAL(query.to_spending_input(spender1, spent), in1);

    // Rebinding does not duplicate a row.
    BOOST_REQUIRE(query.spend(in1, spent));
    BOOST_REQUIRE_EQUAL(query.spender_records(), two);
}

// bind
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(query_bindings__bind__view_spending_block__bound)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));

    const block spending{ header{ 1, test::block1a.hash(), hash_digest{ 0x43 }, 0, 0, 0 }, transactions{ make_coinbase(), test::tx4 } };
    BOOST_REQUIRE(query.set(spending, context{ 0, 2, 0 }, {}, false, false));

    // tx4 spends outputs 0 and 1 of the block1a transaction.
    const auto& outs = *test::block1a.transactions_ptr()->front()->outputs_ptr();
    const auto parent = query.to_tx(test::block1a.transactions_ptr()->front()->hash(false));
    const auto spender = query.to_tx(test::tx4.hash(false));

    auto view = query.get_block_view(2, true);
    BOOST_REQUIRE(!view.populate(system::chain::context{}));
    BOOST_REQUIRE(!query.populate_with_metadata(2, view));

    const auto in0 = view.views().back().inputs_begin();
    const auto in1 = std::next(in0);
    BOOST_REQUIRE_EQUAL(in0->prevout->value(), outs.at(0)->value());
    BOOST_REQUIRE_EQUAL(in1->prevout->value(), outs.at(1)->value());
    BOOST_REQUIRE_EQUAL(in0->metadata.input_link, query.to_point(spender, 0).value);
    BOOST_REQUIRE_EQUAL(in1->metadata.input_link, query.to_point(spender, 1).value);
    BOOST_REQUIRE_EQUAL(in0->metadata.output_link, query.to_output(parent, 0).value);
    BOOST_REQUIRE_EQUAL(in1->metadata.output_link, query.to_output(parent, 1).value);
    BOOST_REQUIRE(!query.bind(header_link{ 2 }, view));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(spender, 0)), query.to_output(parent, 0));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(spender, 1)), query.to_output(parent, 1));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(parent, 0)), tx_links{ spender });
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(parent, 1)), tx_links{ spender });
    BOOST_REQUIRE(query.is_spent(query.to_output(parent, 0)));
    BOOST_REQUIRE(query.is_spent(query.to_output(parent, 1)));
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__view_internal_spend__bound_to_block_instance)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto& genesis_coinbase = *test::genesis.transactions_ptr()->front();
    const transaction parent{ 1, inputs{ input{ point{ genesis_coinbase.hash(false), 0 }, script{}, witness{}, 42 } }, outputs{ output{ 2, script{} } }, 0 };
    const transaction child{ 1, inputs{ input{ point{ parent.hash(false), 0 }, script{}, witness{}, 24 } }, outputs{ output{ 1, script{} } }, 0 };
    const block spending{ header{ 1, test::block0_hash, hash_digest{ 0x44 }, 0, 0, 0 }, transactions{ make_coinbase(), parent, child } };
    BOOST_REQUIRE(query.set(spending, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(!bind(query, 1));

    const auto parent_fk = query.to_tx(parent.hash(false));
    const auto child_fk = query.to_tx(child.hash(false));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(parent_fk, 0)), query.to_output(0, 0));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(child_fk, 0)), query.to_output(parent_fk, 0));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(0, 0)), tx_links{ parent_fk });
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(parent_fk, 0)), tx_links{ child_fk });
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__view_missing_prevout__missing_previous_output)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const transaction spend{ 1, inputs{ input{ point{ system::one_hash, 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const block missing{ header{ 1, test::block0_hash, hash_digest{ 0x42 }, 0, 0, 0 }, transactions{ make_coinbase(), spend } };
    BOOST_REQUIRE(query.set(missing, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE_EQUAL(bind(query, 1), system::error::missing_previous_output);
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__store_pruned_spending_block__bound)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));

    const block spending{ header{ 1, test::block1a.hash(), hash_digest{ 0x47 }, 0, 0, 0 }, transactions{ make_coinbase(), test::tx4 } };
    BOOST_REQUIRE(query.set(spending.header(), context{ 0, 2, 0 }, {}, false));
    const auto link = query.to_header(spending.hash());
    const view::block view{ spending.to_data(true), true };
    BOOST_REQUIRE_EQUAL(query.set_code(view, link, false, true, 2, true), error::success);
    BOOST_REQUIRE(!query.bind(link));

    const auto parent = query.to_tx(test::block1a.transactions_ptr()->front()->hash(false));
    const auto spender = query.to_tx(test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(spender, 0)), query.to_output(parent, 0));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(spender, 1)), query.to_output(parent, 1));
    BOOST_REQUIRE(query.is_spent(query.to_output(parent, 0)));
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__store_missing_prevout__missing_previous_output)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const transaction spend{ 1, inputs{ input{ point{ system::one_hash, 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const block missing{ header{ 1, test::block0_hash, hash_digest{ 0x48 }, 0, 0, 0 }, transactions{ make_coinbase(), spend } };
    BOOST_REQUIRE(query.set(missing, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE_EQUAL(query.bind(header_link{ 1 }), system::error::missing_previous_output);
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__view_missing_then_found__found_bound)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto& genesis_coinbase = *test::genesis.transactions_ptr()->front();
    const transaction missing{ 1, inputs{ input{ point{ system::one_hash, 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const transaction found{ 1, inputs{ input{ point{ genesis_coinbase.hash(false), 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const block mixed{ header{ 1, test::block0_hash, hash_digest{ 0x45 }, 0, 0, 0 }, transactions{ make_coinbase(), missing, found } };
    BOOST_REQUIRE(query.set(mixed, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE_EQUAL(bind(query, 1), system::error::missing_previous_output);

    const auto missing_fk = query.to_tx(missing.hash(false));
    const auto found_fk = query.to_tx(found.hash(false));
    BOOST_REQUIRE(query.to_spent(query.to_point(missing_fk, 0)).is_terminal());
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(found_fk, 0)), query.to_output(0, 0));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(0, 0)), tx_links{ found_fk });
}

BOOST_AUTO_TEST_CASE(query_bindings__bind__store_blocks__result_per_block)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto& genesis_coinbase = *test::genesis.transactions_ptr()->front();
    const transaction found{ 1, inputs{ input{ point{ genesis_coinbase.hash(false), 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const transaction missing{ 1, inputs{ input{ point{ system::one_hash, 0 }, script{}, witness{}, max_uint32 } }, outputs{ output{ 1, script{} } }, 0 };
    const block first{ header{ 1, test::block0_hash, hash_digest{ 0x49 }, 0, 0, 0 }, transactions{ make_coinbase(), found } };
    const block second{ header{ 1, first.hash(), hash_digest{ 0x4a }, 0, 0, 0 }, transactions{ make_coinbase(), missing } };
    BOOST_REQUIRE(query.set(first, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set(second, context{ 0, 2, 0 }, {}, false, false));

    std::vector<code> out{};
    BOOST_REQUIRE(!query.bind(out, header_links{ 1, 2 }));
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    BOOST_REQUIRE(!out.front());
    BOOST_REQUIRE_EQUAL(out.back(), system::error::missing_previous_output);

    const auto found_fk = query.to_tx(found.hash(false));
    BOOST_REQUIRE_EQUAL(query.to_spent(query.to_point(found_fk, 0)), query.to_output(0, 0));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(query.to_output(0, 0)), tx_links{ found_fk });
}

BOOST_AUTO_TEST_SUITE_END()
