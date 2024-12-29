#include "codec.h"
#include "util.h"

#include <stdlib.h>

void CCodecs::registerCodec(const CCodecDesc& desc)
{
	codecs.push_back(desc);
}

CCodecDesc* CCodecs::findCodecByName(const char *filename, const CCodecSettings& cfg)
{
	CCodecDesc *found = NULL;
	for (size_t i=0; i<codecs.size() && !found; i++) {
		try {
			CCodecDesc& desc = codecs[i];
			if (desc.supportsName) {
				if (desc.supportsName(filename, cfg)) {
					found = &desc;
				}
			}
		} catch(...) {}
	}

	return found;
}

CCodecDesc* CCodecs::findCodecByData(const char *filename, const CCodecSettings& cfg)
{
	CCodecDesc *found = NULL;
	if (cfg.scanHeaderSize > 0) {
		void *buf = malloc(cfg.scanHeaderSize);
		if (buf) {
			FILE *f = util::fopen_wrapper(filename, "rb");
			if (f) {
				size_t got = fread(buf, 1, cfg.scanHeaderSize, f);
				fclose(f);

				if (got > 0) {
					
					for (size_t i=0; i<codecs.size() && !found; i++) {
						try {
							CCodecDesc& desc = codecs[i];
							if (desc.supportsFormat) {
								if (desc.supportsFormat(buf, got, cfg)) {
									found = &desc;
								}
							}
						} catch(...) {}
					}
				}
			}
			free(buf);
		}
	}

	return found;
}

CCodecDesc* CCodecs::findCodec(const char *filename, const CCodecSettings& cfg)
{
	CCodecDesc *found = findCodecByData(filename, cfg);
	if (!found) {
		found = findCodecByName(filename, cfg);
	}
	return found;
}

bool CCodecs::decode(const char *filename, CImage& img, const CCodecSettings& cfg)
{
	CCodecDesc *c = findCodec(filename, cfg);
	if (c && c->decode) {
		return c->decode(filename, img, cfg);
	}
	return false;
}

bool CCodecs::encode(const char *filename, const CImage& img, const CCodecSettings& cfg)
{
	CCodecDesc *c = findCodecByName(filename, cfg);
	if (c && c->encode) {
		return c->encode(filename, img, cfg);
	}
	return false;
}

