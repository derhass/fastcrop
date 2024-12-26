#ifndef FASTCROP_GLIMAGE_H
#define FASTCROP_GLIMAGE_H

#include <glad/gl.h>

class CImage; // forward from Image.h

class CGLImage {
	private:
		GLuint tex;
		GLsizei width;
		GLsizei height;
		GLenum	internalFormat;

		void createTex(GLsizei w, GLsizei h, GLsizei ifmt) noexcept;

	public:
		CGLImage() noexcept;
		CGLImage(const CGLImage& other) = delete; // no copy-construct
		CGLImage(CGLImage&& other) noexcept;
		CGLImage& operator=(const CGLImage& other) = delete; // no copy-assingment
		CGLImage& operator=(CGLImage&& other) noexcept;
		~CGLImage() noexcept;

		void drop() noexcept;
		void reset() noexcept;

		bool create(const CImage& img) noexcept;

		GLuint getTex() const noexcept { return tex; }
};

#endif /* !FASTCROP_GLIMAGE_H */
