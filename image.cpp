#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <utility>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

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

	allocate(other.info);
	if (data && other.data) {
		memcpy(data, other.data, other.info.getDataSize());
	}
	return *this;
}

CImage& CImage::operator=(CImage&& other) noexcept
{
	if (this == &other) {
		return *this;
	}

	allocate(other.info);
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

void CImage::setFormat(const TImageInfo& newInfo) noexcept
{
	dropData();
	info = newInfo;
}

bool CImage::allocate(const TImageInfo& newInfo, bool clear) noexcept
{
	setFormat(newInfo);
	size_t s = info.getDataSize();
	if (s > 0) {
		if (clear) {
			data = calloc(s, 1);
		} else {
			data = malloc(s);
		}
	}
	return (data != NULL);
}

void CImage::reset() noexcept
{
	dropData();

	info.reset();
}

const void* CImage::getData() const noexcept
{
	if (!hasData()) {
		return NULL;
	}

	return data;
}

void* CImage::getData() noexcept
{
	if (!hasData()) {
		return NULL;
	}

	return data;
}

bool CImage::hasData() const noexcept
{
	return (data && info.isValid());
}

bool CImage::create(const TImageInfo& newInfo) noexcept
{
	return allocate(newInfo);
}

bool CImage::adopt(const TImageInfo& newInfo, void *dataPtr) noexcept
{
	setFormat(newInfo);

	if (!dataPtr) {
		return false;
	}

	if (info.getDataSize() < 1) {
		return false;
	}

	data = dataPtr;
	return true;
}

bool CImage::makeChecker(const TImageInfo& newInfo) noexcept
{
	if (allocate(newInfo)) {
		for (size_t y=0; y<info.height; y++) {
			uint8_t *line = (uint8_t*)data + (info.width * info.channels * y);
			for (size_t x=0; x<info.width; x++) {
				uint8_t val = ((x + y) & 1) ? 255 : 0;
				for (size_t z=0; z<info.channels; z++) {
					line[x*info.channels + z] = val;
				}
			}
		}
		return true;
	}
	return false;
}

bool CImage::resizeTo(CImage& dst, size_t w, size_t h) noexcept
{
	if (!hasData()) {
		return false;
	}
	if (info.bytesPerChannel != 1) {
		return false;
	}

	stbir_pixel_layout l;
	switch(info.channels) {
		case 1:
			l=STBIR_1CHANNEL;
			break;
		case 2:
			l=STBIR_2CHANNEL;
			break;
		case 3:
			l=STBIR_RGB;
			break;
		case 4:
			l=STBIR_RGBA;
			break;
		default:
			return false;
	}
	if (!dst.allocate(TImageInfo(w,h,info.channels,info.bytesPerChannel))) {
		return false;
	}

	stbir_resize_uint8_srgb((const unsigned char*)data, (int)info.width, (int)info.height, 0,
				(unsigned char*)dst.data, (int)dst.info.width, (int)dst.info.height, 0, l);
	return true;
}
