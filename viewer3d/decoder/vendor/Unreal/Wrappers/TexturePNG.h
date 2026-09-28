#ifndef __UNTEXTUREPNG_H__
#define __UNTEXTUREPNG_H__

// libpng not available under Emscripten — stub implementations.
#include "Core.h"

inline bool UncompressPNG(const unsigned char* /*CompressedData*/, int /*CompressedSize*/,
                          int /*Width*/, int /*Height*/, unsigned char* /*pic*/, bool /*bgra*/)
{
	return false;
}

inline void CompressPNG(const unsigned char* /*pic*/, int /*Width*/, int /*Height*/, TArray<byte>& /*CompressedData*/)
{
}

#endif // __UNTEXTUREPNG_H__
