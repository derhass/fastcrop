#ifndef FASTCROP_CODEC_H
#define FASTCROP_CODEC_H

//#include <unistd.h>
#include <vector>

class CImage; // forward image.h

typedef enum {
	JPEG_SUBSAMPLING_444 = 0,
	JPEG_SUBSAMPLING_422,
	JPEG_SUBSAMPLING_420,
} TJpegSubsamlpingMode;

struct CCodecSettings {
	float quality;
	int jpegSmooth;
	TJpegSubsamlpingMode jpegSubsamplingMode;
	size_t scanHeaderSize;

	bool  autoRotate;
	const char *forceCodecName;
	const char *forceExt;

	CCodecSettings() :
		quality(0.75),
		jpegSmooth(0),
		jpegSubsamplingMode(JPEG_SUBSAMPLING_420),
		scanHeaderSize(1024),
		autoRotate(true),
		forceCodecName(NULL),
		forceExt(NULL)
	{}
};

typedef bool (*TPtrSupportsName)(const char *filename, const char *ext, const CCodecSettings& cfg);
typedef bool (*TPtrSupportsFormat)(const void *header, size_t size, const CCodecSettings& cfg);
typedef bool (*TPtrDecode)(const char *filename, CImage& img, const CCodecSettings& cfg);
typedef bool (*TPtrEncode)(const char *filename, const CImage& img, const CCodecSettings& cfg);

struct CCodecDesc {
	const char *name;
	TPtrSupportsName supportsName;
	TPtrSupportsFormat supportsFormat;
	TPtrDecode decode;
	TPtrEncode encode;

	CCodecDesc(const char *na, TPtrSupportsName n, TPtrSupportsFormat f, TPtrDecode d, TPtrEncode e) :
		name(na),
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

		bool decode(const char *filename, CImage& img, const CCodecSettings& cfg);
		bool encode(const char *filename, const CImage& img, const CCodecSettings& cfg);
};

#endif /* !FASTCROP_CODEC_H */
