#include "controller.h"

#include "codec.h"
#include "util.h"

#include <ctgmath>

CController::CController(CCodecs& c, const CCodecSettings& ds, const CCodecSettings& es) :
	codecs(c),
	decodeSettings(ds),
	encodeSettings(es),
	currentEntity(0)
{
	// TODO: only for testing
	currentCropSate.aspectRatio[1] = 3.0f;
	currentCropSate.aspectRatio[0] = 2.0f;
	//currentCropSate.scale = 1.5f;
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
		if (codecs.decode(e.filename.c_str(), e.image, decodeSettings)) {
			e.image.transpose(true);
			e.image.flipH();
			e.image.flipV();
			/* XXX only testing
			e.image.resize(720,1280);
			codecs.encode("xxx.png", e.image, encodeSettings);
			codecs.encode("xxx.tga", e.image, encodeSettings);
			*/
			e.flags |= FLAG_ENTITY_IMAGE;
		}
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

CImageEntity& CController::getCurrentInternal()
{
	CImageEntity *e = NULL;
	if (currentEntity < entities.size()) {
		e = entities[currentEntity];
	}
	if (!e) {
		e = &dummy;
	}
	return *e;
}

const CImageEntity& CController::getCurrent()
{
	CImageEntity& e = getCurrentInternal();
	prepareImageEntity(e);
	return e;
}

const TDisplayState& CController::getDisplayState(const CImageEntity& e) const
{
	return e.display;
}

const TCropState& CController::getCropState(const CImageEntity& e, bool& croppingEnabled) const
{
	if (e.flags & FLAG_ENTITY_CROPPED) {
		croppingEnabled = true;
		return e.crop;
	}
	croppingEnabled = true; // TODO: switchable mode
	return currentCropSate;
}

void CController::applyCropping(const TImageInfo& info, const TCropState& cs, int32_t pos[2], int32_t size[2]) const
{
	float is[2];
	is[0] = (float)info.width;
	is[1] = (float)info.height;
	float imgAspect = is[0]/is[1];
	float cropAspect = cs.aspectRatio[0] / cs.aspectRatio[1];
	float s[2],sp[2];

	if (cropAspect > imgAspect) {
		s[0] = 1.0f;
		s[1] = imgAspect / cropAspect;
	} else {
		s[0] = cropAspect /imgAspect;
		s[1] = 1.0f;
	}

	for (int i=0; i<2; i++) {
		s[i] *= cs.scale;
		sp[i] = s[i] * is[i];
		size[i] = (int32_t)std::roundf(sp[i]);
		pos[i] = (int32_t)std::roundf((cs.posCenter[i] - 0.5f * s[i]) * is[i]);
		//util::info("XXX %d %d %d",i,size[i],pos[i]);
	}
}

/*
void CController::reconstructCropping(const TImageInfo& info, TCropState& cs, const int32_t pos[2], const int32_t size[2], bool mayAdjustSize) const
{
	float is[2];
	is[0] = (float)info.width;
	is[1] = (float)info.height;
	for (int i=0; i<2; i++) {
		float cp = (float)pos[i] + 0.5f * (float)size[i];
		cs.posCenter[i] = cp / is[i];
	}

	if (mayAdjustSize) {
		int32_t np[2];
		int32_t ns[2];
		applyCropping(info, cs, np, sd);
		if (ns[0] != size[0] || ns[1] != size[1]) {
			float f[2];
			f[0] = (float)ns[0] / (float)ns[1

		}
	}

	if (withSize) {
		float cropAspect = (float)size[0] / (float)size[1];
		
	}
	float imgAspect = is[0]/is[1];
	float cropAspect = cs.aspectRatio[0] / cs.aspectRatio[1];
	float s[2],sp[2];

	if (cropAspect > imgAspect) {
		s[0] = 1.0f;
		s[1] = imgAspect / cropAspect;
	} else {
		s[0] = cropAspect /imgAspect;
		s[1] = 1.0f;
	}
}
*/

void CController::adjustZoom(float factor)
{
	CImageEntity& e = getCurrentInternal();
	e.display.zoom *= factor;
	if (e.display.zoom < 1.0e-6f) {
		e.display.zoom = 1.0e-6f;
	}
	float delta = 1.0f - e.display.zoom;
	if (delta > 1.0e-3f &&  delta < 1.0e-3f) {
		e.display.zoom = 1.0f;
	}
	// TODO: snap in to pixel scales???
}

void CController::setZoom(float factor, bool relativeToPixels)
{
	float baseFactor = 1.0f;
	if (relativeToPixels) {
		// TODO
	}
	CImageEntity& e = getCurrentInternal();
	e.display.zoom = baseFactor * factor;
}

void CController::addFile(const char *name)
{
	CImageEntity *e = new CImageEntity();
	e->filename = std::string(name);
	entities.push_back(e);
}
