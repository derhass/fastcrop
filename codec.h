#ifndef FASTCROP_CODEC_H
#define FASTCROP_CODEC_H

#include <unistd.h>
#include <vector>

class CImage; // forward image.h

struct CCodecSettings {
	float quality;
	size_t scanHeaderSize;

	CCodecSettings() :
		quality(0.75),
		scanHeaderSize(1024)
	{}
};

typedef bool (*TPtrSupportsName)(const char *filename, const CCodecSettings& cfg);
typedef bool (*TPtrSupportsFormat)(const void *header, size_t size, const CCodecSettings& cfg);
typedef bool (*TPtrDecode)(const char *filename, CImage& img, const CCodecSettings& cfg);
typedef bool (*TPtrEncode)(const char *filename, const CImage& img, const CCodecSettings& cfg);

struct CCodecDesc {
	TPtrSupportsName supportsName;
	TPtrSupportsFormat supportsFormat;
	TPtrDecode decode;
	TPtrEncode encode;

	CCodecDesc(TPtrSupportsName n, TPtrSupportsFormat f, TPtrDecode d, TPtrEncode e) :
		supportsName(n),
		supportsFormat(f),
		decode(d),
		encode(e)
	{
	}
};

class CCodecs {
	private:
		std::vector<CCodecDesc> codecs;
	
	public:
		void registerCodec(const CCodecDesc& desc);
		CCodecDesc* findCodecByName(const char *filename, const CCodecSettings& cfg);
		CCodecDesc* findCodecByData(const char *filename, const CCodecSettings& cfg);
		CCodecDesc* findCodec(const char *filename, const CCodecSettings& cfg);
		bool decode(const char *filename, CImage& img, const CCodecSettings& cfg);
		bool encode(const char *filename, const CImage& img, const CCodecSettings& cfg);
};

#endif /* !FASTCROP_CODEC_H */
