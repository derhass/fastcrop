#include "controller.h"

CController::CController() :
	currentEntity(0)
{
}

CController::~CController()
{
	dropGL();
}

bool CController::uploadGLImage(CImageEntity& e)
{
	bool success;
	if (e.flags & FLAG_ENTITY_GLIMAGE) {
		success = true;
	} else {
		success = e.glImage.create(e.image);
		if (success) {
			e.flags |= FLAG_ENTITY_GLIMAGE;
		}
	}
	return success;
}

void CController::dropGLImage(CImageEntity& e)
{
	if (e.flags & FLAG_ENTITY_GLIMAGE) {
		e.glImage.drop();
		e.flags &= ~ FLAG_ENTITY_GLIMAGE;
	}
}

bool CController::prepareImageEntity(CImageEntity& e)
{
	if (!(e.flags & FLAG_ENTITY_IMAGE)) {
		// TODO: load image
	}
	return uploadGLImage(e);
}

bool CController::initGL()
{
	dummy.image.makeChecker(TImageInfo(16,16,1));
	dummy.flags |= FLAG_ENTITY_IMAGE;

	bool success = uploadGLImage(dummy);

	if (success) {
		glBindTexture(GL_TEXTURE_2D, dummy.glImage.getTex());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	return success;
}

void CController::dropGL()
{
	for(size_t i=0; i<entities.size(); i++) {
		if (entities[i]) {
			dropGLImage(*entities[i]);
		}
	}
	dropGLImage(dummy);
}

void CController::setWindowSize(int w, int h) noexcept
{
	windowState.dims[0] = w;
	windowState.dims[1] = h;
}

const TWindowState& CController::getWindowState() const noexcept
{
	return windowState;
}

const CImageEntity& CController::getCurrent()
{
	CImageEntity *e = NULL;
	if (currentEntity < entities.size()) {
		e = entities[currentEntity];
	}
	if (!e) {
		e = &dummy;
	}

	prepareImageEntity(*e);
	return *e;
}

