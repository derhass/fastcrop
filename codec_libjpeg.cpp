#include "codec_libjpeg.h"

#include "util.h"

#include <string.h>

static bool supportsName(const char *filename, const CCodecSettings& cfg)
{
	(void)cfg;
	const char *ext = util::getExt(filename);
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

CCodecDesc codecLibjpeg(supportsName,supportsFormat,NULL,NULL);

