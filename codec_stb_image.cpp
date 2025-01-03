#include "codec_stb_image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb/stb_image_write.h"

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

static bool supportsNameEncode(const char *filename, const char *ext, const CCodecSettings& cfg)
{
	const char *exts[] = {
		"jpg",
		"jpeg",
		"png",
		"tga",
		"bmp",
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

static bool encode(const char *filename, const CImage& img, const CCodecSettings& cfg)
{
	const char *ext = (cfg.forceExt)? cfg.forceExt : (util::getExt(filename));
	if (!ext) {
		ext = filename;
	}
	if (!ext || !ext[0]) {
		return false;
	}
	if (!img.hasData()) {
		return false;
	}
	const TImageInfo& info = img.getInfo();
	if (info.bytesPerChannel != 1) {
		return false;
	}

	bool success;
	const void *data = img.getData();

	if (!strcasecmp(ext, "jpg") || !strcasecmp(ext, "jpeg")) {
		int q = (int)(cfg.quality * 100.0f);
		if (q < 0) {
			q = 0;
		} else if (q > 100) { 
			q = 100;
		}
		success = (stbi_write_jpg(filename, (int)info.width, (int)info.height, (int)info.channels, data, q) != 0);
	} else if (!strcasecmp(ext, "png")) {
		success = (stbi_write_png(filename, (int)info.width, (int)info.height, (int)info.channels, data, (int)(info.width * info.channels * info.bytesPerChannel)) != 0);
	} else if (!strcasecmp(ext, "tga")) {
		success = (stbi_write_tga(filename, (int)info.width, (int)info.height, (int)info.channels, data) != 0);
	} else if (!strcasecmp(ext, "bmp")) {
		success = (stbi_write_bmp(filename, (int)info.width, (int)info.height, (int)info.channels, data) != 0);
	} else {
		success = false;
	}
	return success;
}

CCodecDesc codecSTBImageLoad("stb_image", supportsName,NULL,decode,NULL);
CCodecDesc codecSTBImageWrite("stb_image_write", supportsNameEncode, NULL, NULL, encode);

