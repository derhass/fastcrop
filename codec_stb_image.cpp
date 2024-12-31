#include "codec_stb_image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb/stb_image.h"

#include "image.h"
#include "util.h"

#include <string.h>

static bool supportsName(const char *filename, const char *ext, const CCodecSettings& cfg)
{
	const char *exts[] = {
		"jpg",
		"jpeg",
		"png",
		"tga",
		"bmp",
		"psd",
		"gif",
		"hdr",
		"pic",
		"ppm",
		"pgm",
		"pnm",
		NULL // end marker...
	};
	(void)cfg;
	if (!ext) {
		ext = filename;
		if (!ext) {
			return false;
		}
	}
	for (size_t i=0; exts[i]; i++) {
		if (!strcasecmp(ext,exts[i])) {
			return true;
		}
	}
	return false;
}

static bool decode(const char *filename, CImage& img, const CCodecSettings& cfg)
{
	(void)cfg;
	if (!filename) {
		return false;
	}
	int w=0, h=0, c=0;
	unsigned char *data = stbi_load(filename, &w, &h, &c, 0);
	if (!data) {
		return false;
	}
	if (img.adopt(TImageInfo((size_t)w,(size_t)h,(size_t)c,1), data)) {
		data = NULL;
	}
	if (data) {
		STBI_FREE(data);
		return false;
	}
	return true;
}

CCodecDesc codecSTBImageLoad("stb_image", supportsName,NULL,decode,NULL);

