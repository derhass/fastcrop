#ifndef FASTCROP_RENDER_H
#define FASTCROP_RENDER_H

#include <glad/gl.h>
#include "glimage.h"

class CRenderer {
	private:

		enum TRenderPrograms {
			RENDER_PROGRAM_IMG = 0,
			RENDER_PROGRAMS_COUNT // end marker
		};

		CGLImage *img;
		CGLImage dummyImg;
		GLuint program[RENDER_PROGRAMS_COUNT];
		GLuint vaoEmpty;
		
		bool loadProgram(TRenderPrograms p);
		bool loadPrograms();
		void dropPrograms() noexcept;

	public:
		CRenderer() noexcept;
		~CRenderer() noexcept;

		CRenderer(const CRenderer& other) = delete;
		CRenderer(CRenderer&& other) = delete;
		CRenderer& operator=(const CRenderer& other) = delete;
		CRenderer& operator=(CRenderer&& other) = delete;

		bool initGL();
		void dropGL() noexcept;

		void render();
};

#endif /* !FASTCROP_RENDER_H */
