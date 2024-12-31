#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <utility>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#define GET_PIXEL_OFFSET(i,x,y,c) ((((y)*i.width + (x)) * i.channels + (c)) * i.bytesPerChannel)
#define GET_PIXEL(i,d,x,y,c) (((unsigned char*)d) + GET_PIXEL_OFFSET(i,x,y,c))
#define GET_PIXELC(i,d,x,y,c) (((const unsigned char*)d) + GET_PIXEL_OFFSET(i,x,y,c))

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

	setFormat(other.info);
	data = other.data;
	other.data = NULL;
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

bool CImage::resizeTo(CImage& dst, size_t w, size_t h) const noexcept
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

bool CImage::resize(size_t w, size_t h) noexcept
{
	CImage dst;
	if (resizeTo(dst, w, h)) {
		*this = std::move(dst);
		return true;
	}
	return false;
}

bool CImage::transposeTo(CImage& dst, bool flip) const noexcept
{
	if (!hasData()) {
		return false;
	}
	if (!dst.allocate(TImageInfo(info.height, info.width, info.channels, info.bytesPerChannel))) {
		return false;
	}
	size_t x,y,i;
	size_t ps = info.channels * info.bytesPerChannel;
	ssize_t ls = info.width * ps;
	size_t ss = 0;
	size_t dls = dst.info.width * ps;
	if (flip) {
		ss = info.height - 1;
		ls = -ls;
	}
	const unsigned char *s;
	unsigned char *d= GET_PIXEL(dst.info, dst.data, 0, 0, 0);
	for (y=0; y<dst.info.height; y++) {
       		s = GET_PIXELC(info,data,y,ss,0);
		for (x=0; x<dst.info.width; x++) {
			for (i=0; i<ps; i++) {
				d[x*ps+i] = s[i];
			}
			s += ls;
		}
		d += dls;
	}
	return true;
}

bool CImage::transpose(bool flip) noexcept
{
	CImage dst;
	if (transposeTo(dst, flip)) {
		*this = std::move(dst);
		return true;
	}
	return false;
}

bool CImage::flipH() noexcept
{
	if (!hasData()) {
		return false;
	}

	size_t x,y,i;
	size_t ps = info.channels * info.bytesPerChannel;
	size_t a = info.width>>1U;
	size_t b = info.width - 1;

	for (y=0; y<info.height; y++) {
		unsigned char *p = GET_PIXEL(info, data, 0, y, 0);
		for (x=0; x<a; x++) {
			for (i=0; i<ps; i++) {
				unsigned char tmp = p[x*ps + i];
				p[x*ps + i] = p[(b-x)*ps + i];
				p[(b-x)*ps + i] = tmp;
			}
		}
	}
	return true;
}

bool CImage::flipV() noexcept
{
	if (!hasData()) {
		return false;
	}

	size_t i,y;
	size_t ps = info.channels * info.bytesPerChannel;
	size_t a = info.height>>1U;
	size_t b = info.height - 1;
	size_t c = info.width * ps;

	for (y=0; y<a; y++) {
		unsigned char *p = GET_PIXEL(info, data, 0, y, 0);
		unsigned char *q = GET_PIXEL(info, data, 0, b-y, 0);
		for (i=0; i<c; i++) {
			unsigned char tmp = p[i];
			p[i] = q[i];
			q[i] = tmp;
		}
	}
	return true;
}
