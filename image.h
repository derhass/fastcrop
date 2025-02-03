#ifndef FASTCROP_IMAGE_H
#define FASTCROP_IMAGE_H

#include "exif.h"
//#include <unistd.h>

const size_t maxImageSize = 1*1024U*1024U*1024U;

struct TImageInfo {
	size_t width;
	size_t height;
	size_t channels;
	size_t bytesPerChannel;

	TImageInfo() noexcept :
		width(0),
		height(0),
		channels(0),
		bytesPerChannel(0)
	{}

	TImageInfo(size_t w, size_t h, size_t c=3, size_t bpc=1) noexcept :
		width(w),
		height(h),
		channels(c),
		bytesPerChannel(bpc)
	{}

	void reset() noexcept
	{
		width = 0;
		height = 0;
		channels = 0;
		bytesPerChannel = 0;
	}

	bool isValid() const noexcept
	{
		if (!width || !height || !channels || !bytesPerChannel || channels > 4 || bytesPerChannel > 4 || bytesPerChannel == 3) {
			return false;
		}

		return true;
	}

	size_t getDataSize(size_t maxSize=maxImageSize) const noexcept
	{
		if (!isValid()) {
			return 0;
		}

		size_t s = maxSize;
		s /= bytesPerChannel;
		s /= channels;
		s /= height;
		if (s < width) {
			return 0;
		}
		return width * height * channels * bytesPerChannel;
	}
};

typedef enum {
	FC_RESIZE_AUTO = 0,
	FC_RESIZE_STB,
#ifdef WITH_LIBSWSCALE
	FC_RESIZE_SWSCALE,
#endif
	FC_RESIZE_COUNT /* end marker */	
} TFCResizeMode;

#ifdef WITH_LIBSWSCALE
typedef enum {
	FC_SWS_FAST_BILINEAR=0,
	FC_SWS_BILINEAR,
	FC_SWS_BICUBIC,
	FC_SWS_X,
	FC_SWS_POINT,
	FC_SWS_AREA,
	FC_SWS_BICUBLIN,
	FC_SWS_GAUSS,
	FC_SWS_SINC,
	FC_SWS_LANCZOS,
	FC_SWS_SPLINE,
	FC_SWS_COUNT /* end marker */
} TFCSWSMode;
#endif

struct TImageResizeCtx {
#ifdef WITH_LIBSWSCALE
	TFCSWSMode swsMode;
#endif
	TFCResizeMode mode;

	TImageResizeCtx() noexcept :
#ifdef WITH_LIBSWSCALE
		swsMode(FC_SWS_SPLINE),
#endif
		mode(FC_RESIZE_AUTO)
	{}
};

class CImage {
	private:
		TImageInfo info;
		TExifData  exif;

		void* data;

		void dropData() noexcept;
		void setFormat(const TImageInfo& newInfo) noexcept;
		bool allocate(const TImageInfo& newInfo, bool clear=false) noexcept;

	public:
		CImage() noexcept;
		CImage(const CImage& other) noexcept;
		CImage(CImage&& other) noexcept;
		~CImage() noexcept;

		CImage& operator=(const CImage& other) noexcept;
		CImage& operator=(CImage&& other) noexcept;

		void reset() noexcept;
		bool isValidDims() const noexcept;
		const void* getData() const noexcept; // NULL if invalid 
		void* getData() noexcept; // NULL if invalid
		bool hasData() const noexcept;
		const TImageInfo& getInfo() const noexcept {return info;}
		const TExifData& getExif() const noexcept {return exif;}
		TExifData& getExif() noexcept {return exif;}

		bool create(const TImageInfo& newInfo) noexcept;
		bool adopt(const TImageInfo& newInfo, void *dataPtr) noexcept;
		bool makeChecker(const TImageInfo& newInfo) noexcept;

		bool resizeTo(CImage& dst, const TImageResizeCtx& ctx, size_t w, size_t h) const noexcept;
		bool resizeToLimits(CImage& dst, const TImageResizeCtx& ctx, size_t maxSize, size_t maxWidth, size_t maxHeight, size_t minSize, size_t minWidth, size_t minHeight) const noexcept;
		bool resize(const TImageResizeCtx& ctx, size_t w, size_t h) noexcept;

		bool transposeTo(CImage& dst, bool flip) const noexcept;
		bool transpose(bool flip) noexcept;

		bool flipH() noexcept;
		bool flipV() noexcept;

		bool cropTo(CImage& dst, const int32_t pos[2], const int32_t size[2]) const noexcept;
};

#endif /* !FASTCROP_IMAGE_H */
