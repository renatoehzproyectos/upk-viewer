#ifndef __UNTEXTURENVTT_H__
#define __UNTEXTURENVTT_H__

// NVTT not available under Emscripten — stub so UnTexture.cpp compiles.
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
inline void DecodeDDS(const unsigned char*, int, int, nv::DDSHeader&, nv::Image&) {}
inline void WriteDDSHeader(unsigned char*, nv::DDSHeader&) {}

#endif
