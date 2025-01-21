#ifndef FASTCROP_RENDER_H
#define FASTCROP_RENDER_H

#include <glad/gl.h>
#include <stdint.h>

struct CImageEntity; // forward controller.h
class CController; // forward controller.h

struct TUBOWindowState {
	float cropLineColor[4];
	uint32_t dims[2];

	TUBOWindowState() noexcept
	{
		reset();
	}

	void reset() noexcept
	{
		cropLineColor[0] = 1.0f;
		cropLineColor[1] = 1.0f;
		cropLineColor[2] = 1.0f;
		cropLineColor[3] = 1.0f;

		dims[0] = 1920;
		dims[1] = 1080;
	}
};

struct TUBODisplayState {
	int32_t imgDims[2];
	float scale[2];
	float offset[2];

	TUBODisplayState() noexcept
	{
		reset();
	}

	void reset() noexcept
	{
		imgDims[0] = 1;
		imgDims[1] = 1;
		scale[0] = 1.0f;
		scale[1] = 1.0f;
		offset[0] = 0.0f;
		offset[1] = 0.0f;
	}
};

struct TUBOCropState {
	int32_t cropPos[2];
	int32_t cropSize[2];

	TUBOCropState() noexcept
	{
		reset();
	}

	void reset() noexcept
	{
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
			RENDER_PROGRAM_CROPLINE,
			RENDER_PROGRAMS_COUNT // end marker
		};

		enum TUniformBuffers {
			UBO_WINDOW_STATE = 0,
			UBO_DISPLAY_STATE,
			UBO_CROP_STATE,
			UBOS_COUNT // end marker
		};

		GLuint program[RENDER_PROGRAMS_COUNT];
		GLuint ubo[UBOS_COUNT];
		GLuint vaoEmpty;

		TUBOWindowState uboWindowState;
		TUBODisplayState uboDisplayState;
		TUBOCropState uboCropState;
		unsigned int ubosDirty;
		
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

		void prepareUBOs(const CImageEntity& e, const CController& ctrl);
		void render(const CImageEntity& e, const CController& ctrl);

		void invalidateWindowState() noexcept;
		void invalidateDisplayState() noexcept;
		void invalidateCropState() noexcept;
		void invalidateImageState() noexcept;
};

#endif /* !FASTCROP_RENDER_H */
