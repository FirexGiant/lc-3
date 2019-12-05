#pragma once

#include <climits>

namespace rks {

template <typename T, typename O>
// requires UnsignedInteger(T) &&
//      Writable(O) && Iterator(O) && ValueType(O) == unsigned char
O store_little_endian(const T& x, O f_o)
{
    for (size_t i(0); i < sizeof(T); ++i)
        *f_o++ = (x >> (i * CHAR_BIT)) & UCHAR_MAX;
    return f_o;
}

template <typename T, typename I>
// requires UnsignedInteger(T) &&
//      Readable(I) && Iterator(I) && ValueType(I) == unsigned char
I load_little_endian(T& x, I f_i)
{
    x = T(0);
    for (size_t i(0); i < sizeof(T); ++i)
        x = x | (*f_i++ << (i * CHAR_BIT));
    return f_i;
}

template <typename T, typename O>
// requires UnsignedInteger(T) &&
//      Writable(O) && Iterator(O) && ValueType(O) == unsigned char
O store_big_endian(const T& x, O f_o)
{
    for (size_t i(0); i < sizeof(T); ++i)
        *f_o++ = (x >> (sizeof(T) * CHAR_BIT - ((i + 1) * CHAR_BIT))) & UCHAR_MAX;
    return f_o;
}

template <typename T, typename I>
// requires UnsignedInteger(T) &&
//      Readable(I) && Iterator(I) && ValueType(I) == unsigned char
I load_big_endian(T& x, I f_i)
{
    x = T(0);
    for (size_t i(0); i < sizeof(T); ++i)
        x = x | (*f_i++ << (sizeof(T) * CHAR_BIT - ((i + 1) * CHAR_BIT)));
    return f_i;
}

} // namespace rks
