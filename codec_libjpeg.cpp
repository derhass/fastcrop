#include "codec_libjpeg.h"

#include "util.h"

#include <string.h>

static bool supportsName(const char *filename, const char *ext, const CCodecSettings& cfg)
{
	(void)cfg;
	if (!ext) {
		ext = filename;
		if (!ext) {
			return false;
		}
	}
	
	if (!strcasecmp(ext,"jpg") || !strcasecmp(ext,"jpeg")) {
		return true;
	}
	return false;
}

static bool supportsFormat(const void *header, size_t size, const CCodecSettings& cfg)
{
	if (size >= 2) {
		const unsigned char *d = (const unsigned char*)header;
		if (d[0] == 0xff && d[1] == 0xe8) {
			return true;
		}
	}
	return false;
}

CCodecDesc codecLibjpeg("libjpeg", supportsName,supportsFormat,NULL,NULL);

