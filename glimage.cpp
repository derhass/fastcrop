#include "glimage.h"
#include "image.h"

#include <utility>

CGLImage::CGLImage() noexcept :
	tex(0)
{
	reset();
}

CGLImage::CGLImage(CGLImage&& other) noexcept
{
	*this = std::move(other);
}

CGLImage& CGLImage::operator=(CGLImage&& other) noexcept
{
	if (this == &other) {
		return *this;
	}

	drop();
	width = other.width;
	height = other.height;
	internalFormat = other.internalFormat;
	tex = other.tex;
	other.tex = 0;

	return *this;
}

CGLImage::~CGLImage() noexcept
{
	drop();
}

void CGLImage::createTex(GLsizei w, GLsizei h, GLsizei ifmt) noexcept
{
	drop();
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, ifmt, w, h);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	width = w;
	height = h;
	internalFormat = ifmt;
}

void CGLImage::drop() noexcept
{
	if (tex) {
		glDeleteTextures(1, &tex);
		tex = 0;
	}
}

void CGLImage::reset() noexcept
{
	drop();
	width = 0;
	height = 0;
	internalFormat = GL_NONE;
}

bool CGLImage::create(const CImage& img) noexcept
{
	const void *data;
	GLenum ifmt,fmt,dtype;

	drop();
	data = img.getData();
	const TImageInfo& info = img.getInfo();
	ifmt = GL_NONE;
	fmt = GL_NONE;
	dtype = GL_NONE;
	if (info.bytesPerChannel == 1) {
		dtype = GL_UNSIGNED_BYTE;
		switch(info.channels) {
			case 1:
				ifmt = GL_R8;
				fmt = GL_RED;
				break;
			case 2:
				ifmt = GL_RG8;
				fmt = GL_RG;
				break;
			case 3:
				ifmt = GL_RGB8;
				fmt = GL_RGB;
				break;
			case 4:
				ifmt = GL_RGBA8;
				fmt = GL_RGBA;
				break;
		}
	} else if (info.bytesPerChannel == 2) {
		dtype = GL_UNSIGNED_SHORT;
		switch(info.channels) {
			case 1:
				ifmt = GL_R16;
				fmt = GL_RED;
				break;
			case 2:
				ifmt = GL_RG16;
				fmt = GL_RG;
				break;
			case 3:
				ifmt = GL_RGB16;
				fmt = GL_RGB;
				break;
			case 4:
				ifmt = GL_RGBA16;
				fmt = GL_RGBA;
				break;
		}
	}

	if (!data || ifmt == GL_NONE) {
		reset();
		return false;
	}

	createTex((GLsizei)info.width,(GLsizei)info.height, ifmt);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, fmt, dtype, data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}
