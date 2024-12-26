#ifndef FASTCROP_IMAGE_H
#define FASTCROP_IMAGE_H

#include <unistd.h>

const size_t maxImageSize = 1*1024U*1024U*1024U;

class CImage {
	private:
		unsigned width;
		unsigned height;
		unsigned channels;
		unsigned bytesPerChannel;

		void* data;

		void dropData() noexcept;
		void setFormat(unsigned w, unsigned h, unsigned c, unsigned bpc=1) noexcept;
		bool allocate(unsigned w, unsigned h, unsigned c, unsigned bpc=1) noexcept;

	public:
		CImage() noexcept;
		CImage(const CImage& other) noexcept;
		CImage(CImage&& other) noexcept;
		~CImage() noexcept;

		CImage& operator=(const CImage& other) noexcept;
		CImage& operator=(CImage&& other) noexcept;

		void reset() noexcept;
		bool isValidDims() const noexcept;
		size_t getDataSize(size_t maxSize = maxImageSize) const noexcept; // 0 if invalid
		const void* getData(unsigned&w, unsigned& h, unsigned& c, unsigned& bpc) const noexcept; // NULL if invalid 
		bool hasData() const noexcept;

		bool makeChecker(unsigned w, unsigned h, unsigned c=3) noexcept;
};

#endif /* !FASTCROP_IMAGE_H */
