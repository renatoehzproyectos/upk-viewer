#ifndef __UNTEXTURENVTT_H__
#define __UNTEXTURENVTT_H__

// NVTT (nvimage) is not available in the Emscripten/WASM build.
// Provide minimal stubs so UnTexture.cpp still compiles; DXT paths
// that relied on NVTT will fall back / no-op.

namespace nv {
struct DDSHeader {
	void setFourCC(unsigned, unsigned, unsigned, unsigned) {}
	void setWidth(int) {}
	void setHeight(int) {}
	void setNormalFlag(bool) {}
};
struct Image {
	unsigned char* pixels() { return nullptr; }
};
}

inline void DecodeDDS(const unsigned char* /*Data*/, int /*USize*/, int /*VSize*/,
                      nv::DDSHeader& /*header*/, nv::Image& /*image*/) {}
inline void WriteDDSHeader(unsigned char* /*Data*/, nv::DDSHeader& /*header*/) {}

#endif // __UNTEXTURENVTT_H__
