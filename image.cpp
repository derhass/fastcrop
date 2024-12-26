#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <utility>

CImage::CImage() noexcept :
	data(NULL)
{
	reset();
}

CImage::CImage(const CImage& other) noexcept :
	data(NULL)
{
	*this = other;
}

CImage::CImage(CImage&& other) noexcept :
	data(NULL)
{
	*this = std::move(other);
}

CImage& CImage::operator=(const CImage& other) noexcept
{
	if (this == &other) {
		return *this;
	}

	allocate(other.width, other.height, other.channels, other.bytesPerChannel);
	if (data && other.data) {
		memcpy(data, other.data, getDataSize());
	}
	return *this;
}

CImage& CImage::operator=(CImage&& other) noexcept
{
	if (this == &other) {
		return *this;
	}

	allocate(other.width, other.height, other.channels, other.bytesPerChannel);
	if (data && other.data) {
		data = other.data;
		other.data = NULL;
	}
	return *this;
}

CImage::~CImage() noexcept
{
	dropData();
}

void CImage::dropData() noexcept
{
	if (data) {
		free(data);
		data = NULL;
	}
}

void CImage::setFormat(unsigned w, unsigned h, unsigned c, unsigned bpc) noexcept
{
	dropData();
	width = w;
	height = h;
	channels = c;
	bytesPerChannel = bpc;
}

bool CImage::allocate(unsigned w, unsigned h, unsigned c, unsigned bpc) noexcept
{
	setFormat(w,h,c,bpc);
	size_t s = getDataSize();
	if (s > 0) {
		data = calloc(s, 1);
	}
	return (data != NULL);
}

void CImage::reset() noexcept
{
	dropData();

	width = 0;
	height = 0;
	channels = 0;
	bytesPerChannel = 0;
}

bool CImage::isValidDims() const noexcept
{
	if (!width || !height || !channels || !bytesPerChannel || channels > 4 || bytesPerChannel > 4 || bytesPerChannel == 3) {
		return false;
	}

	return true;
}

size_t CImage::getDataSize(size_t maxSize) const noexcept
{
	if (!isValidDims()) {
		return 0;
	}

	size_t s = maxSize;
	s /= bytesPerChannel;
	s /= channels;
	s /= height;
	if (s < width) {
		return 0;
	}
	return s * height * channels * bytesPerChannel;
}

const void* CImage::getData(unsigned&w, unsigned& h, unsigned& c, unsigned& bpc) const noexcept
{
	if (!hasData()) {
		w = 0;
		h = 0;
		c = 0;
		bpc = 0;
		return NULL;
	}

	w = width;
	h = height;
	c = channels;
	bpc = bytesPerChannel;
	return data;
}

bool CImage::hasData() const noexcept
{
	return (data && isValidDims());
}

bool CImage::makeChecker(unsigned int w, unsigned int h, unsigned int c) noexcept
{
	if (allocate(w,h,c,1)) {
		for (unsigned int y=0; y<h; y++) {
			uint8_t *line = (uint8_t*)data + (w * c * y);
			for (unsigned int x=0; x<w; x++) {
				uint8_t val = ((x + y) & 1) ? 255 : 0;
				for (unsigned int z=0; z<c; z++) {
					line[x*c + z] = val;
				}
			}
		}
		return true;
	}
	return false;
}
