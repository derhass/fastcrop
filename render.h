#ifndef FASTCROP_RENDER_H
#define FASTCROP_RENDER_H

#include <glad/gl.h>
#include "glimage.h"

#include <stdint.h>

struct TUBODisplayState {
	uint32_t dims[2];
	float scale[2];
	float offset[2];
	uint32_t cropPos[2];
	uint32_t cropSize[2];

	TUBODisplayState() noexcept
	{
		reset();
	}

	void reset() noexcept
	{
		dims[0] = 1;
		dims[1] = 1;
		scale[0] = 1.0f;
		scale[1] = 1.0f;
		offset[0] = 0.0f;
		offset[1] = 0.0f;
		cropPos[0] = 0;
		cropPos[1] = 0;
		cropSize[0] = 0;
		cropSize[1] = 0;
	}
};

class CRenderer {
	private:
		enum TRenderPrograms {
			RENDER_PROGRAM_IMG = 0,
			RENDER_PROGRAMS_COUNT // end marker
		};

		enum TUniformBuffers {
			UBO_DISPLAY_STATE = 0,
			UBOS_COUNT // end marker
		};

		CGLImage *img;
		CGLImage dummyImg;
		GLuint program[RENDER_PROGRAMS_COUNT];
		GLuint ubo[UBOS_COUNT];
		GLuint vaoEmpty;

		TUBODisplayState uboDisplayState;
		
		bool loadProgram(TRenderPrograms p);
		bool loadPrograms();
		void dropPrograms() noexcept;

		const void* getUBOData(GLsizeiptr& size, TUniformBuffers u) const;
		void updateUBO(TUniformBuffers u);
		void updateUBOs() noexcept;
		void dropUBOs() noexcept;

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
