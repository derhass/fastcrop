#include "render.h"
#include "controller.h"
#include "util.h"

#include <stdlib.h>
#include <string>

CRenderer::CRenderer() noexcept :
	vaoEmpty(0),
	ubosDirty(0)
{
	int i;
	for (i=0; i<(int)RENDER_PROGRAMS_COUNT; i++) {
		program[i] = 0;
	}

	for (i=0; i<(int)UBOS_COUNT; i++) {
		ubo[i] = 0;
	}
}

CRenderer::~CRenderer() noexcept
{
	dropGL();
}

bool CRenderer::initGL()
{
	dropGL();

	glGenVertexArrays(1, &vaoEmpty);
	bool success = loadPrograms();

	updateUBOs();
	
	return success;
}

void CRenderer::dropGL() noexcept
{
	if (vaoEmpty) {
		glDeleteVertexArrays(1, &vaoEmpty);
		vaoEmpty = 0;
	}
	dropPrograms();
	dropUBOs();
}

bool CRenderer::loadProgram(TRenderPrograms p)
{
	if (p >= RENDER_PROGRAMS_COUNT) {
		return false;
	}
	if (program[p]) {
		glDeleteProgram(program[p]);
		program[p] = 0;
	}
	std::string base = "shaders/";
	std::string selected;
	std::string vs;
	std::string fs;
	switch(p) {
		case RENDER_PROGRAM_IMG:
			selected = "img";
			break;
		case RENDER_PROGRAM_CROPLINE:
			selected = "cropline";
			break;
		default:
			return false;
	}

	vs = base + selected + ".vert.glsl";
	fs = base + selected + ".frag.glsl";
	program[p]=util::programCreateFromFiles(vs.c_str(), fs.c_str());
	if (!program[p]) {
		util::warn("failed to load shader %d: %s %s",(int)p,vs.c_str(),fs.c_str());
		return false;
	}
	return true;
}

bool CRenderer::loadPrograms()
{
	bool success = true;
	for (int p=0; p<(int)RENDER_PROGRAMS_COUNT; p++) {
		success = loadProgram((TRenderPrograms)p) && success;
	}
	return success;
}

void CRenderer::dropPrograms() noexcept
{
	for (int p=0; p<(int)RENDER_PROGRAMS_COUNT; p++) {
		if (program[p]) {
			glDeleteProgram(program[p]);
			program[p] = 0;
		}
	}
}

const void* CRenderer::getUBOData(GLsizeiptr& size, TUniformBuffers u) const
{
	switch(u) {
		case UBO_WINDOW_STATE:
			size = sizeof(TUBOWindowState);
			return (const void*)&uboWindowState;
		case UBO_DISPLAY_STATE:
			size = sizeof(TUBODisplayState);
			return (const void*)&uboDisplayState;
		case UBO_CROP_STATE:
			size = sizeof(TUBOCropState);
			return (const void*)&uboCropState;
		default:
			size = 0;
			return NULL;
	}
}

void CRenderer::updateUBO(TUniformBuffers u)
{
	const void *data;
	GLsizeiptr size;

	data = getUBOData(size, u);
	if (data) {
		if (!ubo[u]) {
			GLsizeiptr rem = size & 0xf;
			GLsizeiptr xsize = size;
			if (rem > 0) {
				xsize += (16 - rem);
			}
			glGenBuffers(1, &ubo[u]);
			glBindBufferBase(GL_UNIFORM_BUFFER, (GLuint)u, ubo[u]);
			glBufferStorage(GL_UNIFORM_BUFFER, xsize, NULL, GL_DYNAMIC_STORAGE_BIT);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
		} else {
			glBindBuffer(GL_UNIFORM_BUFFER, ubo[u]);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
		}
	}
}

void CRenderer::updateUBOs() noexcept
{
	for (int u=0; u<(int)UBOS_COUNT; u++) {
		updateUBO((TUniformBuffers)u);
	}
}

void CRenderer::dropUBOs() noexcept
{
	for (int u=0; u<(int)UBOS_COUNT; u++) {
		if (ubo[0]) {
			glDeleteBuffers(1, &ubo[u]);
			ubo[u] = 0;
		}
	}
}

void CRenderer::prepareUBOs(const CImageEntity& e, const CController& ctrl)
{
	if (ubosDirty & (1U<<(unsigned)UBO_WINDOW_STATE)) {
		const TWindowState& ws = ctrl.getWindowState();
		uboWindowState.dims[0] = (uint32_t)ws.dims[0];
		uboWindowState.dims[1] = (uint32_t)ws.dims[1];
		updateUBO(UBO_WINDOW_STATE);
		ubosDirty &= ~(1U<<(unsigned)UBO_WINDOW_STATE);
	}

	if (ubosDirty & (1U<<(unsigned)UBO_DISPLAY_STATE)) {
		double scale[2];
		double offset[2];
		ctrl.getDisplayTransform(e, scale, offset, true);
		if (e.flags & FLAG_ENTITY_IMAGE) {
			const TImageInfo& info = e.image.getInfo();
			uboDisplayState.imgDims[0] = (int32_t)info.width;
			uboDisplayState.imgDims[1] = (int32_t)info.height;
		} else {
			uboDisplayState.imgDims[0] = (int32_t)1024;
			uboDisplayState.imgDims[1] = (int32_t)1024;
		}
		uboDisplayState.scale[0] = (float)scale[0];
		uboDisplayState.scale[1] = (float)scale[1];
		uboDisplayState.offset[0] = (float)offset[0];
		uboDisplayState.offset[1] = (float)offset[1];
		updateUBO(UBO_DISPLAY_STATE);
		ubosDirty &= ~(1U<<(unsigned)UBO_DISPLAY_STATE);
	}

	if (ubosDirty & (1U<<(unsigned)UBO_CROP_STATE)) {
		bool croppingEnabled;
		const TCropState& cs = ctrl.getCropState(e, croppingEnabled);
		if (croppingEnabled) {
			const TImageInfo& info = e.image.getInfo();
			ctrl.applyCropping(info, cs, uboCropState.cropPos, uboCropState.cropSize);
			uboCropState.cropPos[1] = info.height - uboCropState.cropPos[1] -  uboCropState.cropSize[1];
		} else {
			uboCropState.cropPos[0] = 0;
			uboCropState.cropPos[1] = 0;
			uboCropState.cropSize[0] = 0;
			uboCropState.cropSize[1] = 0;
		}
		updateUBO(UBO_CROP_STATE);
		ubosDirty &= ~(1U<<(unsigned)UBO_CROP_STATE);
	}
}

void CRenderer::render(const CImageEntity& e, const CController& ctrl)
{

	glUseProgram(program[RENDER_PROGRAM_IMG]);
	glBindVertexArray(vaoEmpty);

	prepareUBOs(e, ctrl);
	glBindTextureUnit(0, e.glImage.getTex());
	glDrawArrays(GL_TRIANGLES, 0, 6);

	if (uboCropState.cropSize[0] > 0) {
		glUseProgram(program[RENDER_PROGRAM_CROPLINE]);
		glDrawArrays(GL_LINE_LOOP, 0, 4);
	}
}

void CRenderer::invalidateWindowState() noexcept
{
	ubosDirty |= (1U<<(unsigned)UBO_WINDOW_STATE);
	ubosDirty |= (1U<<(unsigned)UBO_DISPLAY_STATE);
}

void CRenderer::invalidateDisplayState() noexcept
{
	ubosDirty |= (1U<<(unsigned)UBO_DISPLAY_STATE);
}

void CRenderer::invalidateCropState() noexcept
{
	ubosDirty |= (1U<<(unsigned)UBO_CROP_STATE);
}

void CRenderer::invalidateImageState() noexcept
{
	ubosDirty |= (1U<<(unsigned)UBO_DISPLAY_STATE);
	ubosDirty |= (1U<<(unsigned)UBO_CROP_STATE);
}

