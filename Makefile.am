###############################################################################
#  Copyright (c) 2014-2025 libbitcoin-database developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the m4 subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4


# Distribute, make and install products.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-database.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# src/libbitcoin-database.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-database.la
src_libbitcoin_database_la_CPPFLAGS = -I${srcdir}/include ${bitcoin_system_BUILD_CPPFLAGS}
src_libbitcoin_database_la_LIBADD = ${bitcoin_system_LIBS}
src_libbitcoin_database_la_SOURCES = \
    src/error.cpp \
    src/settings.cpp \
    src/file/rotator.cpp \
    src/file/utilities.cpp \
    src/locks/file_lock.cpp \
    src/locks/flush_lock.cpp \
    src/locks/interprocess_lock.cpp \
    src/memory/map.cpp \
    src/memory/utilities.cpp \
    src/memory/mman-win32/mman.cpp \
    src/memory/mman-win32/mman.hpp

# local: test/libbitcoin-database-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-database-test_runner.sh

check_PROGRAMS = test/libbitcoin-database-test
test_libbitcoin_database_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_system_BUILD_CPPFLAGS}
test_libbitcoin_database_test_LDADD = src/libbitcoin-database.la ${boost_unit_test_framework_LIBS} ${bitcoin_system_LIBS}
test_libbitcoin_database_test_SOURCES = \
    test/error.cpp \
    test/main.cpp \
    test/settings.cpp \
    test/store.cpp \
    test/test.cpp \
    test/test.hpp \
    test/file/rotator.cpp \
    test/file/utilities.cpp \
    test/locks/file_lock.cpp \
    test/locks/flush_lock.cpp \
    test/locks/interprocess_lock.cpp \
    test/memory/accessor.cpp \
    test/memory/map.cpp \
    test/memory/utilities.cpp \
    test/mocks/blocks.cpp \
    test/mocks/blocks.hpp \
    test/mocks/chunk_storage.cpp \
    test/mocks/chunk_storage.hpp \
    test/mocks/chunk_store.hpp \
    test/mocks/map_store.hpp \
    test/primitives/arrayhead.cpp \
    test/primitives/arraymap.cpp \
    test/primitives/hashhead.cpp \
    test/primitives/hashmap.cpp \
    test/primitives/iterator.cpp \
    test/primitives/keys.cpp \
    test/primitives/linkage.cpp \
    test/primitives/manager.cpp \
    test/primitives/nomap.cpp \
    test/query/archive_read.cpp \
    test/query/archive_write.cpp \
    test/query/confirm.cpp \
    test/query/consensus.cpp \
    test/query/context.cpp \
    test/query/extent.cpp \
    test/query/height.cpp \
    test/query/initialize.cpp \
    test/query/network.cpp \
    test/query/objects.cpp \
    test/query/optional.cpp \
    test/query/translate.cpp \
    test/query/validate.cpp \
    test/tables/archives/header.cpp \
    test/tables/archives/input.cpp \
    test/tables/archives/output.cpp \
    test/tables/archives/point.cpp \
    test/tables/archives/puts.cpp \
    test/tables/archives/transaction.cpp \
    test/tables/archives/txs.cpp \
    test/tables/caches/duplicate.cpp \
    test/tables/caches/prevout.cpp \
    test/tables/caches/validated_bk.cpp \
    test/tables/caches/validated_tx.cpp \
    test/tables/indexes/height.cpp \
    test/tables/indexes/strong_tx.cpp \
    test/tables/optional/address.cpp \
    test/tables/optional/filter_bk.cpp \
    test/tables/optional/filter_tx.cpp

endif WITH_TESTS

# local: tools/initchain/initchain
#------------------------------------------------------------------------------
if WITH_TOOLS

noinst_PROGRAMS = tools/initchain/initchain
tools_initchain_initchain_CPPFLAGS = -I${srcdir}/include ${bitcoin_system_BUILD_CPPFLAGS}
tools_initchain_initchain_LDADD = src/libbitcoin-database.la ${bitcoin_system_LIBS}
tools_initchain_initchain_SOURCES = \
    tools/initchain/initchain.cpp

endif WITH_TOOLS

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/database.hpp

include_bitcoin_databasedir = ${includedir}/bitcoin/database
include_bitcoin_database_HEADERS = \
    include/bitcoin/database/boost.hpp \
    include/bitcoin/database/define.hpp \
    include/bitcoin/database/error.hpp \
    include/bitcoin/database/query.hpp \
    include/bitcoin/database/settings.hpp \
    include/bitcoin/database/store.hpp \
    include/bitcoin/database/version.hpp

include_bitcoin_database_filedir = ${includedir}/bitcoin/database/file
include_bitcoin_database_file_HEADERS = \
    include/bitcoin/database/file/file.hpp \
    include/bitcoin/database/file/rotator.hpp \
    include/bitcoin/database/file/utilities.hpp

include_bitcoin_database_impldir = ${includedir}/bitcoin/database/impl
include_bitcoin_database_impl_HEADERS = \
    include/bitcoin/database/impl/store.ipp

include_bitcoin_database_impl_memorydir = ${includedir}/bitcoin/database/impl/memory
include_bitcoin_database_impl_memory_HEADERS = \
    include/bitcoin/database/impl/memory/accessor.ipp

include_bitcoin_database_impl_primitivesdir = ${includedir}/bitcoin/database/impl/primitives
include_bitcoin_database_impl_primitives_HEADERS = \
    include/bitcoin/database/impl/primitives/arrayhead.ipp \
    include/bitcoin/database/impl/primitives/arraymap.ipp \
    include/bitcoin/database/impl/primitives/hashhead.ipp \
    include/bitcoin/database/impl/primitives/hashmap.ipp \
    include/bitcoin/database/impl/primitives/iterator.ipp \
    include/bitcoin/database/impl/primitives/keys.ipp \
    include/bitcoin/database/impl/primitives/linkage.ipp \
    include/bitcoin/database/impl/primitives/manager.ipp \
    include/bitcoin/database/impl/primitives/nomap.ipp

include_bitcoin_database_impl_querydir = ${includedir}/bitcoin/database/impl/query
include_bitcoin_database_impl_query_HEADERS = \
    include/bitcoin/database/impl/query/archive_read.ipp \
    include/bitcoin/database/impl/query/archive_write.ipp \
    include/bitcoin/database/impl/query/confirm.ipp \
    include/bitcoin/database/impl/query/consensus.ipp \
    include/bitcoin/database/impl/query/context.ipp \
    include/bitcoin/database/impl/query/extent.ipp \
    include/bitcoin/database/impl/query/height.ipp \
    include/bitcoin/database/impl/query/initialize.ipp \
    include/bitcoin/database/impl/query/network.ipp \
    include/bitcoin/database/impl/query/objects.ipp \
    include/bitcoin/database/impl/query/optional.ipp \
    include/bitcoin/database/impl/query/query.ipp \
    include/bitcoin/database/impl/query/translate.ipp \
    include/bitcoin/database/impl/query/validate.ipp

include_bitcoin_database_locksdir = ${includedir}/bitcoin/database/locks
include_bitcoin_database_locks_HEADERS = \
    include/bitcoin/database/locks/file_lock.hpp \
    include/bitcoin/database/locks/flush_lock.hpp \
    include/bitcoin/database/locks/interprocess_lock.hpp \
    include/bitcoin/database/locks/locks.hpp

include_bitcoin_database_memorydir = ${includedir}/bitcoin/database/memory
include_bitcoin_database_memory_HEADERS = \
    include/bitcoin/database/memory/accessor.hpp \
    include/bitcoin/database/memory/finalizer.hpp \
    include/bitcoin/database/memory/map.hpp \
    include/bitcoin/database/memory/memory.hpp \
    include/bitcoin/database/memory/reader.hpp \
    include/bitcoin/database/memory/streamers.hpp \
    include/bitcoin/database/memory/utilities.hpp

include_bitcoin_database_memory_interfacesdir = ${includedir}/bitcoin/database/memory/interfaces
include_bitcoin_database_memory_interfaces_HEADERS = \
    include/bitcoin/database/memory/interfaces/memory.hpp \
    include/bitcoin/database/memory/interfaces/storage.hpp

include_bitcoin_database_primitivesdir = ${includedir}/bitcoin/database/primitives
include_bitcoin_database_primitives_HEADERS = \
    include/bitcoin/database/primitives/arrayhead.hpp \
    include/bitcoin/database/primitives/arraymap.hpp \
    include/bitcoin/database/primitives/hashhead.hpp \
    include/bitcoin/database/primitives/hashmap.hpp \
    include/bitcoin/database/primitives/iterator.hpp \
    include/bitcoin/database/primitives/keys.hpp \
    include/bitcoin/database/primitives/linkage.hpp \
    include/bitcoin/database/primitives/manager.hpp \
    include/bitcoin/database/primitives/nomap.hpp \
    include/bitcoin/database/primitives/primitives.hpp

include_bitcoin_database_tablesdir = ${includedir}/bitcoin/database/tables
include_bitcoin_database_tables_HEADERS = \
    include/bitcoin/database/tables/association.hpp \
    include/bitcoin/database/tables/associations.hpp \
    include/bitcoin/database/tables/context.hpp \
    include/bitcoin/database/tables/event.hpp \
    include/bitcoin/database/tables/names.hpp \
    include/bitcoin/database/tables/point_set.hpp \
    include/bitcoin/database/tables/schema.hpp \
    include/bitcoin/database/tables/states.hpp \
    include/bitcoin/database/tables/table.hpp \
    include/bitcoin/database/tables/tables.hpp

include_bitcoin_database_tables_archivesdir = ${includedir}/bitcoin/database/tables/archives
include_bitcoin_database_tables_archives_HEADERS = \
    include/bitcoin/database/tables/archives/header.hpp \
    include/bitcoin/database/tables/archives/input.hpp \
    include/bitcoin/database/tables/archives/ins.hpp \
    include/bitcoin/database/tables/archives/output.hpp \
    include/bitcoin/database/tables/archives/outs.hpp \
    include/bitcoin/database/tables/archives/point.hpp \
    include/bitcoin/database/tables/archives/transaction.hpp \
    include/bitcoin/database/tables/archives/txs.hpp

include_bitcoin_database_tables_cachesdir = ${includedir}/bitcoin/database/tables/caches
include_bitcoin_database_tables_caches_HEADERS = \
    include/bitcoin/database/tables/caches/duplicate.hpp \
    include/bitcoin/database/tables/caches/prevout.hpp \
    include/bitcoin/database/tables/caches/validated_bk.hpp \
    include/bitcoin/database/tables/caches/validated_tx.hpp

include_bitcoin_database_tables_indexesdir = ${includedir}/bitcoin/database/tables/indexes
include_bitcoin_database_tables_indexes_HEADERS = \
    include/bitcoin/database/tables/indexes/height.hpp \
    include/bitcoin/database/tables/indexes/strong_tx.hpp

include_bitcoin_database_tables_optionalsdir = ${includedir}/bitcoin/database/tables/optionals
include_bitcoin_database_tables_optionals_HEADERS = \
    include/bitcoin/database/tables/optionals/address.hpp \
    include/bitcoin/database/tables/optionals/filter_bk.hpp \
    include/bitcoin/database/tables/optionals/filter_tx.hpp


# Custom make targets.
#==============================================================================
# make target: tools
#------------------------------------------------------------------------------
target_tools = \
    tools/initchain/initchain

tools: ${target_tools}

