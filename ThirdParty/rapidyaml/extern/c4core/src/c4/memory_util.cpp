// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/memory_util.hpp"
#include "c4/error.hpp"

C4_BEGIN_NAMESPACE(c4)

/** returns true if the memory overlaps */
bool mem_overlaps(void const* a, void const* b, size_t sza, size_t szb)
{
    if(a < b)
    {
        if(size_t(a) + sza > size_t(b)) return true;
    }
    else if(a > b)
    {
        if(size_t(b) + szb > size_t(a)) return true;
    }
    else if(a == b)
    {
        if(sza != 0 && szb != 0) return true;
    }
    return false;
}

/** Fills 'dest' with the first 'pattern_size' bytes at 'pattern', 'num_times'. */
void mem_repeat(void* dest, void const* pattern, size_t pattern_size, size_t num_times)
{
    if(C4_UNLIKELY(num_times == 0)) return;
    C4_ASSERT( ! mem_overlaps(dest, pattern, num_times*pattern_size, pattern_size));
    char *begin = (char*)dest;
    char *end   = begin + num_times * pattern_size;
    // copy the pattern once
    ::memcpy(begin, pattern, pattern_size);
    // now copy from dest to itself, doubling up every time
    size_t n = pattern_size;
    while(begin + 2*n < end)
    {
        ::memcpy(begin + n, begin, n);
        n <<= 1; // double n
    }
    // copy the missing part
    if(begin + n < end)
    {
        ::memcpy(begin + n, begin, end - (begin + n));
    }
}

C4_END_NAMESPACE(c4)