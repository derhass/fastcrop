#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <cmath>
#include <utility>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#ifdef WITH_LIBSWSCALE
extern "C" {
#include "libswscale/swscale.h"
}
#endif

#include "util.h"

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
	exif = other.exif;
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
	exif = other.exif;
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
	exif.parsed = false;
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

static bool resizeSTB(const unsigned char *src, const TImageInfo& info, unsigned char *dst, const TImageInfo& dstInfo, const TImageResizeCtx& ctx) noexcept
{
	(void)ctx;

	if (!src || !dst) {
		util::warn("resizeSTB: no valid data");
		return false;
	}
	if (info.bytesPerChannel != 1) {
		util::warn("resizeSTB: unsupported bit depth %u", (unsigned)info.bytesPerChannel*8U);
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
			util::warn("resizeSTB: unsupported channel count %u", (unsigned)info.channels);
			return false;
	}
	stbir_resize_uint8_srgb(src, (int)info.width, (int)info.height, 0,
				dst, (int)dstInfo.width, (int)dstInfo.height, 0, l);
	return true;
}

#ifdef WITH_LIBSWSCALE
static bool resizeSWS(const uint8_t *src, const TImageInfo& info, uint8_t *dst, const TImageInfo& dstInfo, const TImageResizeCtx& ctx) noexcept
{
	enum AVPixelFormat fmt;
	bool littleEndian;
	uint16_t data;
	uint8_t *ptr;
       
	if (!src || !dst) {
		util::warn("resizeSWS: no valid data");
		return false;
	}

	switch(info.bytesPerChannel) {
		case 1:
			switch(info.channels) {
				case 1:
					fmt = AV_PIX_FMT_GRAY8;
					break;
				case 3:
					fmt = AV_PIX_FMT_RGB24;
					break;
				case 4:
					fmt = AV_PIX_FMT_RGBA;
					break;
				default:
					fmt = AV_PIX_FMT_NONE;
			}
			break;
		case 2:
			data = 0x1122;
			ptr = (uint8_t*)&data;
			if (ptr[0] == 0x22) {
				littleEndian = true;
			} else if (ptr[0] == 0x11) {
				littleEndian = false;
			} else {
				util::warn("resizeSWS: endianness detection failed!");
				return false;
			}
			switch(info.channels) {
				case 1:
					fmt = (littleEndian)?AV_PIX_FMT_GRAY16LE : AV_PIX_FMT_GRAY16BE;
					break;
				case 3:
					fmt = (littleEndian)?AV_PIX_FMT_RGB48LE : AV_PIX_FMT_RGB48BE;
					break;
				case 4:
					fmt = (littleEndian)?AV_PIX_FMT_RGBA64LE : AV_PIX_FMT_RGBA64BE;
					break;
				default:
					fmt = AV_PIX_FMT_NONE;
			}
			break;
		default:
			fmt = AV_PIX_FMT_NONE;
	}

	if (fmt == AV_PIX_FMT_NONE) {
		util::warn("resizeSWS: unsupported format: %u channels, bit depth %u", (unsigned)info.channels, (unsigned)info.bytesPerChannel*8U);
		return false;
	}

	if (!sws_isSupportedInput(fmt)) {
		util::warn("resizeSWS: unsupported INPUT format %d: %u channels, bit depth %u", (int)fmt, (unsigned)info.channels, (unsigned)info.bytesPerChannel*8U);
		return false;
	}
	if (!sws_isSupportedOutput(fmt)) {
		util::warn("resizeSWS: unsupported OUTPUT format %d: %u channels, bit depth %u", (int)fmt, (unsigned)info.channels, (unsigned)info.bytesPerChannel*8U);
		return false;
	}
	
	debug("resizeSWS: selected format %d: %u channels, bit depth %u", (int)fmt, (unsigned)info.channels, (unsigned)info.bytesPerChannel*8U);

	int flags = 0;

	switch (ctx.swsMode) {
		case FC_SWS_FAST_BILINEAR:	flags |= SWS_FAST_BILINEAR; break;
		case FC_SWS_BILINEAR:		flags |= SWS_BILINEAR; break;
		case FC_SWS_BICUBIC:		flags |= SWS_BICUBIC; break;
		case FC_SWS_X:			flags |= SWS_X; break;
		case FC_SWS_POINT:		flags |= SWS_POINT; break;
		case FC_SWS_AREA:		flags |= SWS_AREA; break;
		case FC_SWS_BICUBLIN:		flags |= SWS_BICUBLIN; break;
		case FC_SWS_GAUSS:		flags |= SWS_GAUSS; break;
		case FC_SWS_SINC:		flags |= SWS_SINC; break;
		case FC_SWS_LANCZOS:		flags |= SWS_LANCZOS; break;
		case FC_SWS_SPLINE:		flags |= SWS_SPLINE; break;
		default:
			util::warn("resizeSWS: invalid scaler mode %d", (int)ctx.swsMode);
			return false;
	}
	struct SwsContext *swsctx = sws_getContext((int)info.width, (int)info.height, fmt, (int)dstInfo.width, (int)dstInfo.height, fmt, flags, NULL, NULL, NULL);
	if (!swsctx) {
		util::warn("resizeSWS: failed to get context");
		return false;
	}

	bool success = false;
	const uint8_t* srcData[3];
	uint8_t* dstData[3];
	int srcStride[3];
	int dstStride[3];

	srcData[0] = src;
	srcData[1] = NULL;
	srcData[2] = NULL;
	srcStride[0] = (int)(info.channels * info.bytesPerChannel * info.width);
	srcStride[1] = 0;
	srcStride[2] = 0;

	dstData[0] = dst;
	dstData[1] = NULL;
	dstData[2] = NULL;
	dstStride[0] = (int)(dstInfo.channels * dstInfo.bytesPerChannel * dstInfo.width);
	dstStride[1] = 0;
	dstStride[2] = 0;

	int res = sws_scale(swsctx, srcData, srcStride, 0, (int)info.height, dstData, dstStride);
	if (res == (int)dstInfo.height) {
		success = true;
	} else {
		util::warn("resizeSWS: failed to scale: %d",res);
	}

	sws_freeContext(swsctx);
	return success;
}
#endif /* WITH_LIBSWSCALE */

bool CImage::resizeTo(CImage& dst, const TImageResizeCtx& ctx, size_t w, size_t h) const noexcept
{
	if (!hasData()) {
		util::warn("resize: no valid data");
		return false;
	}

	TFCResizeMode mode = ctx.mode;
	if (mode == FC_RESIZE_AUTO) {
#ifdef WITH_LIBSWSCALE
		mode = FC_RESIZE_SWSCALE;
#else
		mode = FC_RESIZE_STB;
#endif
	}

	if (!dst.allocate(TImageInfo(w,h,info.channels,info.bytesPerChannel))) {
		util::warn("resize: failed to allocate output");
		return false;
	}

	bool success;
	switch(mode) {
		case FC_RESIZE_STB:
			success = resizeSTB((const unsigned char*)data, info, (unsigned char*)dst.data, dst.info, ctx);
			break;
#ifdef WITH_LIBSWSCALE
		case FC_RESIZE_SWSCALE:
			success = resizeSWS((const uint8_t*)data, info, (uint8_t*)dst.data, dst.info, ctx);
			break;
#endif
		default:
			util::warn("resize: invalid mode %u", (unsigned)mode);
			success = false;
	}
	if (!success) {
		util::warn("resize failed");
		dst.dropData();
	}
	return success;
}

bool CImage::resizeToLimits(CImage& dst, const TImageResizeCtx& ctx, size_t maxSize, size_t maxWidth, size_t maxHeight, size_t minSize, size_t minWidth, size_t minHeight) const noexcept
{
	if (!hasData()) {
		return false;
	}
	double aspect = (double)info.width / (double)info.height;
	size_t s[2];
	s[0] = info.width;
	s[1] = info.height;


	if (minSize && s[0] < minSize) { 
		s[0] = minSize;
		s[1] = (size_t)std::round((double)s[0] / aspect);
	}
	if (minSize && s[1] < minSize) { 
		s[1] = minSize;
		s[0] = (size_t)std::round((double)s[1] * aspect);
	}
	if (minWidth && s[0] < minWidth) {
		s[0] = minWidth;
		s[1] = (size_t)std::round((double)s[0] / aspect);
	}
	if (minHeight && s[1] < minSize) { 
		s[1] = minHeight;
		s[0] = (size_t)std::round((double)s[1] * aspect);
	}

	if (maxSize && s[0] > maxSize) {
		s[0] = maxSize;
		s[1] = (size_t)std::round((double)s[0] / aspect);
	}
	if (maxSize && s[1] > maxSize) {
		s[1] = maxSize;
		s[0] = (size_t)std::round((double)s[1] * aspect);
	}
	if (maxWidth && s[0] > maxWidth) {
		s[0] = maxWidth;
		s[1] = (size_t)std::round((double)s[0] / aspect);
	}
	if (maxHeight && s[1] > maxHeight) {
		s[1] = maxHeight;
		s[0] = (size_t)std::round((double)s[1] * aspect);
	}

	if (s[0] == info.width && s[1] == info.height) {
		dst = *this;
	}
	return resizeTo(dst, ctx, s[0], s[1]);
}

bool CImage::resize(const TImageResizeCtx& ctx, size_t w, size_t h) noexcept
{
	CImage dst;
	if (resizeTo(dst, ctx, w, h)) {
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
	intptr_t ls = info.width * ps;
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

bool CImage::cropTo(CImage& dst, const int32_t pos[2], const int32_t size[2]) const noexcept
{
	if (size[0] < 1 || size[1] < 1) {
		dst.reset();
		return false;
	}

	if (!dst.allocate(TImageInfo((size_t)size[0], (size_t)size[1], info.channels, info.bytesPerChannel))) {
		return false;
	}

	int32_t w = (int32_t)info.width;
	int32_t h = (int32_t)info.height;
	int32_t ps = (int32_t)(info.channels * info.bytesPerChannel);
	int32_t i;
	uint8_t *data = (uint8_t*)dst.getData();
	const uint8_t* sdata = (const uint8_t*)getData();
	if (!sdata || !data) {
		return false;
	}

	for (int32_t y = 0; y < size[1]; y++) {
		int32_t sy = y + pos[1];
		for (int32_t x = 0; x < size[0]; x++) {
			int32_t sx = x + pos[0];
			if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
				for (i=0; i<ps; i++) {
					data[x*ps+i] = sdata[(sy * w + sx)*ps + i];
				}
			} else {
				for (i=0; i<ps; i++) {
					data[x*ps+i] = 0;
				}
			}
		}
		data += size[0] * ps;
	}
	return true;
}
