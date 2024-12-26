#include "render.h"
#include "image.h"
#include "util.h"

#include <stdlib.h>
#include <string>

CRenderer::CRenderer() noexcept :
	img(NULL),
	vaoEmpty(0)
{
	for (int p=0; p<(int)RENDER_PROGRAMS_COUNT; p++) {
		program[p] = 0;
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
	
	CImage img;
	img.makeChecker(TImageInfo(16,16,1));
	success = dummyImg.create(img) && success;
	glBindTexture(GL_TEXTURE_2D, dummyImg.getTex());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return success;
}

void CRenderer::dropGL() noexcept
{
	if (vaoEmpty) {
		glDeleteVertexArrays(1, &vaoEmpty);
		vaoEmpty = 0;
	}
	dummyImg.drop();
	dropPrograms();
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
	std::string vs;
	std::string fs;
	switch(p) {
		case RENDER_PROGRAM_IMG:
			vs = base + "img.vert.glsl";
			fs = base + "img.frag.glsl";
			break;
		default:
			return false;
	}

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

void CRenderer::render()
{
	glUseProgram(program[RENDER_PROGRAM_IMG]);
	glBindVertexArray(vaoEmpty);

	const CGLImage* curImg = (img)?img:&dummyImg;
	glBindTextureUnit(0, curImg->getTex());
	glDrawArrays(GL_TRIANGLES, 0, 6);

}
