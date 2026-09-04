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
#include <bitcoin/database/memory/utilities.hpp>

#if defined(HAVE_MSC)
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#if defined(HAVE_APPLE)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif
#if defined(HAVE_LINUX)
    #include <algorithm>
    #include <cinttypes>
    #include <cstdio>
#endif
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

using namespace system;

#if defined(HAVE_MSC)

size_t page_size() NOEXCEPT
{
    SYSTEM_INFO info{};
    ::GetSystemInfo(&info);

    BC_ASSERT(!is_limited<size_t>(info.dwPageSize));
    return info.dwPageSize;
}

uint64_t system_memory() NOEXCEPT
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return is_zero(::GlobalMemoryStatusEx(&status)) ? zero :
        status.ullTotalPhys;
}

uint64_t system_free() NOEXCEPT
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return is_zero(::GlobalMemoryStatusEx(&status)) ? zero :
        status.ullAvailPhys;
}

uint64_t system_available() NOEXCEPT
{
    // Windows available physical memory is the correct semantic directly.
    return system_free();
}

size_t system_pressure() NOEXCEPT
{
    // The kernel low memory resource signal (no configurable threshold).
    BOOL low{ FALSE };
    const auto handle = ::CreateMemoryResourceNotification(
        ::LowMemoryResourceNotification);

    if (handle == NULL)
        return zero;

    const auto success = !is_zero(::QueryMemoryResourceNotification(handle,
        &low));
    ::CloseHandle(handle);

    return success ? (low == FALSE ? 1_size : 4_size) : zero;
}

uint64_t system_compressed() NOEXCEPT
{
    // No cheap source (performance counters only).
    return zero;
}

#else // HAVE_MSC

size_t page_size() NOEXCEPT
{
    errno = 0;
    const auto size = sysconf(_SC_PAGESIZE);
    if (is_zero(errno) && !is_negative(size))
    {
        BC_ASSERT(!is_limited<size_t>(size));
        return possible_narrow_sign_cast<size_t>(size);
    }

    return zero;
}

uint64_t system_memory() NOEXCEPT
{
    errno = 0;
    const int64_t pages = sysconf(_SC_PHYS_PAGES);
    if (is_zero(errno) && !is_negative(pages))
    {
        // Failed page_size also results in zero return.
        const auto size = possible_wide_cast<uint64_t>(page_size());
        return ceilinged_multiply(to_unsigned(pages), size);
    }

    return zero;
}

uint64_t system_free() NOEXCEPT
{
#if defined(HAVE_APPLE)
    auto count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t statistics{};
    if (::host_statistics64(::mach_host_self(), HOST_VM_INFO64,
        pointer_cast<integer_t>(&statistics), &count) == KERN_SUCCESS)
    {
        return ceilinged_multiply<uint64_t>(statistics.free_count, page_size());
    }
#else
    errno = 0;
    const int64_t pages = sysconf(_SC_AVPHYS_PAGES);
    if (is_zero(errno) && !is_negative(pages))
    {
        // Failed page_size() also results in zero return.
        const auto size = possible_wide_cast<uint64_t>(page_size());
        return ceilinged_multiply(to_unsigned(pages), size);
    }
#endif

    // Failed or no platform source.
    return zero;
}

uint64_t system_available() NOEXCEPT
{
#if defined(HAVE_APPLE)
    // Free plus purgeable plus file-backed (external) pages are available to
    // allocation without compression or swap.
    auto count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t statistics{};
    if (::host_statistics64(::mach_host_self(), HOST_VM_INFO64,
        pointer_cast<integer_t>(&statistics), &count) == KERN_SUCCESS)
    {
        const auto pages = statistics.free_count + statistics.purgeable_count +
            statistics.external_page_count;
        return ceilinged_multiply<uint64_t>(pages, page_size());
    }
#elif defined(HAVE_LINUX)
    // MemAvailable is the kernel estimate of memory available to allocation
    // without swapping (includes reclaimable file cache).
    if (const auto file = std::fopen("/proc/meminfo", "r"))
    {
        char line[128];
        uint64_t kilobytes{};
        while (!is_null(std::fgets(line, sizeof(line), file)))
        {
            if (std::sscanf(line, "MemAvailable: %" SCNu64 " kB",
                &kilobytes) == 1)
            {
                std::fclose(file);
                return ceilinged_multiply<uint64_t>(kilobytes, 1024u);
            }
        }

        std::fclose(file);
    }
#endif

    // Failed or no platform source (free approximates on other platforms).
    return system_free();
}

size_t system_pressure() NOEXCEPT
{
#if defined(HAVE_APPLE)
    int level{};
    auto size = sizeof(level);
    if (is_zero(::sysctlbyname("kern.memorystatus_vm_pressure_level", &level,
        &size, nullptr, 0)))
    {
        return possible_narrow_sign_cast<size_t>(level);
    }
#elif defined(HAVE_LINUX)
    // PSI (requires CONFIG_PSI): fraction of recent wall time that tasks
    // stalled on memory reclaim. A tenth of time stalled is treated as genuine
    // pressure, partial (some) as warning and total (full) as critical,
    // mapping to the macos memorystatus level semantics.
    if (const auto file = std::fopen("/proc/pressure/memory", "r"))
    {
        char line[128];
        auto level = one;
        double average10{};
        while (!is_null(std::fgets(line, sizeof(line), file)))
        {
            if ((std::sscanf(line, "some average10=%lf", &average10) == 1) &&
                (average10 >= 10.0))
                level = std::max(level, 2_size);

            if ((std::sscanf(line, "full average10=%lf", &average10) == 1) &&
                (average10 >= 10.0))
                level = std::max(level, 4_size);
        }

        std::fclose(file);
        return level;
    }
#endif

    // Failed or no platform source.
    return zero;
}

uint64_t system_compressed() NOEXCEPT
{
#if defined(HAVE_APPLE)
    auto count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t statistics{};
    if (::host_statistics64(::mach_host_self(), HOST_VM_INFO64,
        pointer_cast<integer_t>(&statistics), &count) == KERN_SUCCESS)
    {
        const auto pages = statistics.compressor_page_count;
        return ceilinged_multiply<uint64_t>(pages, page_size());
    }
#endif

    // Failed or no platform source adopted. Linux zswap/zram accounting is
    // configuration dependent, PSI carries the pressure signal there.
    return zero;
}

#endif // HAVE_MSC

constexpr auto gigabyte = power2<uint64_t>(30u);
constexpr auto megabyte = power2<uint64_t>(20u);
constexpr auto uncontested = power2<uint64_t>(35u);

#if defined(HAVE_APPLE)
constexpr auto contested = 10u * gigabyte;
#else
constexpr auto contested = 8u * gigabyte;
#endif

uint32_t derive_buckets(uint64_t expected, uint32_t low,
    uint32_t high) NOEXCEPT
{
    if (is_zero(expected) || is_zero(low) || is_zero(high))
        return {};

    const auto scaled = ceilinged_multiply<uint64_t>(expected, 10);
    const auto floored = scaled / low;
    const auto ceiled = scaled / high;
    const auto memory = system_memory();

    if (memory <= contested)
        return possible_narrow_cast<uint32_t>(floored);

    if (memory >= uncontested)
        return possible_narrow_cast<uint32_t>(ceiled);

    // Megabytes, as the byte product overflows the word.
    const auto over = (memory - contested) / megabyte;
    const auto rise = floored_subtract(ceiled, floored);
    constexpr auto span = (uncontested - contested) / megabyte;
    return limit<uint32_t>(floored + (ceilinged_multiply(rise, over) / span));
}

} // namespace database
} // namespace libbitcoin
