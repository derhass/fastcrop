#ifndef FASTCROP_EXIF_H
#define FASTCROP_EXIF_H

#include <stdint.h>
#include <stdlib.h>

extern size_t TIFFFindBegin(const void* data, size_t maxSize);
extern size_t EXIFFindBegin(const void* data, size_t maxSize);

struct TExifData {
	bool     parsed;
	uint16_t orientation;

	TExifData() noexcept :
		parsed(false),
		orientation(0)
	{
	}
};

extern bool EXIFParse(TExifData& info, const void* data, size_t size);

#endif /* !FASTCROP_EXIF_H */
