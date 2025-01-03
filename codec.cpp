#include "codec.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

void CCodecs::registerCodec(const CCodecDesc& desc)
{
	codecs.push_back(desc);
}

bool CCodecs::decode(const char *filename, CImage& img, const CCodecSettings& cfg)
{
	void *buf = NULL;
	size_t size = 0;
	bool success = false;
	bool bufAllocTried = false;
	const char *ext = NULL;

	if (!filename || !filename[0]) {
		return false;
	}

	for (size_t i=0; i<codecs.size() && !success; i++) {
		bool tryThis = false;
		CCodecDesc& c = codecs[i];
		if (c.supportsFormat) {
			if (cfg.scanHeaderSize > 0 && !bufAllocTried) {
				bufAllocTried = true;
				buf = malloc(cfg.scanHeaderSize);
				if (buf) {
					FILE *f = util::fopen_wrapper(filename, "rb");
					if (f) {
						size = fread(buf, 1, cfg.scanHeaderSize, f);
						fclose(f);
					}
				}
			}
			if (bufAllocTried && buf && size > 0) {
				try {
					if (c.supportsFormat(buf, size, cfg)) {
						tryThis = true;
					}
				} catch(...) {}
			}
		}
	        if (!tryThis && c.supportsName) {
			if (!ext) {
				if (cfg.forceExt) {
					ext = cfg.forceExt;
				} else {
					ext = util::getExt(filename);
				}
			}
			try {
				if (c.supportsName(filename, ext, cfg)) {
					tryThis = true;
				}
			} catch(...) {}
		}
		if (c.decode && tryThis && cfg.forceCodecName) {
			if (strcmp(c.name, cfg.forceCodecName)) {
				tryThis = false;
			}
		}
		if (c.decode && tryThis) {
			try {
				success = c.decode(filename, img, cfg);
			} catch(...) {
				success = false;
			}
		}
	}

	if (buf) {
		free(buf);
	}

	return success;
}

bool CCodecs::encode(const char *filename, const CImage& img, const CCodecSettings& cfg)
{
	if (!filename || !filename[0]) {
		return false;
	}

	bool success = false;
	const char *ext = (cfg.forceExt)?cfg.forceExt : (util::getExt(filename));

	for (size_t i=0; i<codecs.size() && !success; i++) {
		bool tryThis = false;
		CCodecDesc& c = codecs[i];
	        if (c.supportsName) {
			try {
				if (c.supportsName(filename, ext, cfg)) {
					tryThis = true;
				}
			} catch(...) {}
		}
		if (c.encode && tryThis && cfg.forceCodecName) {
			if (strcmp(c.name, cfg.forceCodecName)) {
				tryThis = false;
			}
		}
		if (c.encode && tryThis) {
			try {
				success = c.encode(filename, img, cfg);
			} catch(...) {
				success = false;
			}
		}
	}

	return success;
}

