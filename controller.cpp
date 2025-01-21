#include "controller.h"

#include "codec.h"
#include "util.h"

#include <ctgmath>

CController::CController(CCodecs& c, const CCodecSettings& ds, const CCodecSettings& es) :
	codecs(c),
	decodeSettings(ds),
	encodeSettings(es),
	currentEntity(0),
	inDragCrop(0)
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
			//e.image.transpose(true); // XXX
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

TCropState& CController::getCropStateInternal(CImageEntity& e, bool& croppingEnabled)
{
	if (e.flags & FLAG_ENTITY_CROPPED) {
		croppingEnabled = true;
		return e.crop;
	}
	croppingEnabled = true; // TODO: switchable mode
	return currentCropSate;
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

void CController::getCropSizeNC(const TImageInfo& info, const TCropState& cs, double s[2]) const
{
	double is[2];
	is[0] = (double)info.width;
	is[1] = (double)info.height;
	double imgAspect = is[0]/is[1];
	double cropAspect = cs.aspectRatio[0] / cs.aspectRatio[1];

	if (cropAspect > imgAspect) {
		s[0] = (double)cs.scale;
		s[1] = (imgAspect / cropAspect) * (double)cs.scale;;
	} else {
		s[0] = (cropAspect /imgAspect) * (double)cs.scale;
		s[1] = (double)cs.scale;
	}
}

void CController::applyCropping(const TImageInfo& info, const TCropState& cs, int32_t pos[2], int32_t size[2]) const
{
	double is[2];
	is[0] = (double)info.width;
	is[1] = (double)info.height;
	double s[2],sp[2];
	getCropSizeNC(info, cs, s);

	for (int i=0; i<2; i++) {
		sp[i] = s[i] * is[i];
		size[i] = (int32_t)std::round(sp[i]);
		pos[i] = (int32_t)std::round((cs.posCenter[i] - 0.5f * s[i]) * is[i]);
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

void CController::getDisplayTransform(const CImageEntity& e, double scale[2], double offset[2], bool minusOneToOne) const
{
	double winAspect = (double)windowState.dims[0] / (double)windowState.dims[1];
	double imgAspect;
	if (e.flags & FLAG_ENTITY_IMAGE) {
		const TImageInfo& info = e.image.getInfo();
		imgAspect = ((double)info.width / (double)info.height) * e.display.aspectCorrection;
	} else {
		imgAspect = e.display.aspectCorrection;
	}
	double s = e.display.zoom;
	if (minusOneToOne) {
		s *= 2.0;
	}
	if (imgAspect >= winAspect) {
		scale[0] = s;
		scale[1] = s * winAspect / imgAspect;
	} else {
		scale[0] = s * (imgAspect / winAspect);
		scale[1] = s;
	}

	if (minusOneToOne) {
		offset[0] = -0.5 * scale[0];
		offset[1] = -0.5 * scale[1];
	} else {
		offset[0] = 0.5 * (1.0 - scale[0]);
		offset[1] = 0.5 * (1.0 - scale[1]);
	}
}

void CController::winToNC(const double winPos[2], double nc[2]) const
{
	nc[0] = winPos[0] / (double)windowState.dims[0];
	nc[1] = 1.0 - (winPos[1] / (double)windowState.dims[1]);
}

void CController::NCtoWin(const double nc[2], double winPos[2]) const
{
	winPos[0] = nc[0] * (double)windowState.dims[0];
	winPos[1] = (1.0 - nc[1]) * (double)windowState.dims[1];
}

void CController::NCtoImage(const CImageEntity& e, const double nc[2], double imgPos[2]) const
{
	double s[2], o[2];
	getDisplayTransform(e, s, o, false);
	imgPos[0] = (nc[0] - o[0]) / s[0];
	imgPos[1] = (nc[1] - o[1]) / s[1];
}

void CController::imageToNC(const CImageEntity& e, const double imgPos[2], double nc[2]) const
{
	double s[2], o[2];
	getDisplayTransform(e, s, o, false);
	nc[0] = s[0] * imgPos[0] + o[0];
	nc[1] = s[1] * imgPos[1] + o[1];
}


void CController::winToImage(const CImageEntity& e, const double winPos[2], double imgPos[2]) const
{
	double nc[2];
	winToNC(winPos, nc);
	NCtoImage(e, nc, imgPos);
}

void CController::imageToWin(const CImageEntity& e, const double imgPos[2], double winPos[2]) const
{
	double nc[2];
	imageToNC(e, imgPos, nc);
	NCtoWin(nc, winPos);
}

void CController::imageToCropNC(const CImageEntity& e, const double imgPos[2], double cropPosNC[2]) const
{
	double s[2];
	double o[2];
	bool enabled;
	const TCropState& cs = getCropState(e, enabled);
	getCropSizeNC(e.image.getInfo(), cs, s);
	o[0] = cs.posCenter[0] - 0.5 * s[0];
	o[1] = cs.posCenter[1] - 0.5 * s[1];
	cropPosNC[0] = (imgPos[0] - o[0])/s[0];
	cropPosNC[1] = (imgPos[1] - o[1])/s[1];
}

void CController::cropNCToImage(const CImageEntity& e, const double cropPosNC[2], double imgPos[2]) const
{

	double s[2];
	double o[2];
	bool enabled;
	const TCropState& cs = getCropState(e, enabled);
	getCropSizeNC(e.image.getInfo(), cs, s);
	o[0] = cs.posCenter[0] - 0.5 * s[0];
	o[1] = cs.posCenter[1] - 0.5 * s[1];
	imgPos[0] = cropPosNC[0] * s[0] + o[0];
	imgPos[1] = cropPosNC[1] * s[1] + o[1];
}

void CController::clampCrop(const CImageEntity& e, TCropState& cs)
{
	double s[2];
	getCropSizeNC(e.image.getInfo(), cs, s);
	for (int i=0; i<2; i++) {
		if (cs.posCenter[i] - 0.5 * s[i] < 0.0) {
			cs.posCenter[i] = (float)( 0.5 * s[i] );
		} else if (cs.posCenter[i] + 0.5 * s[i] > 1.0) {
			cs.posCenter[i] = (float) (1.0 - 0.5 * s[i]);
		}
	}
}

bool CController::beginDragCrop(const double winPos[2])
{
	const CImageEntity& e = getCurrentInternal();
	double imgPos[2];
	double cropPos[2];

	winToImage(e, winPos, imgPos);
	imageToCropNC(e, imgPos, cropPos);
	if (cropPos[0] >= 0.0 && cropPos[0] <= 1.0 && cropPos[1] >= 0.0 && cropPos[1] <= 1.0) {
		inDragCrop = 1;
		dragCropMouseBegin[0] = winPos[0];
		dragCropMouseBegin[1] = winPos[1];
		dragCropNCBegin[0] = cropPos[0];
		dragCropNCBegin[1] = cropPos[1];
		return true;
	}
	return false;
}

bool CController::doDragCrop(const double winPos[2])
{
	if (!inDragCrop) {
		return false;
	}

	if ((winPos[0] != dragCropMouseBegin[0]) || (winPos[1] != dragCropMouseBegin[1])) {
		inDragCrop = 2;
	}
	if (inDragCrop >= 2) {
		bool enabled;
		CImageEntity& e = getCurrentInternal();
		TCropState& cs = getCropStateInternal(e,enabled);
		if (enabled) {
			double imgPos[2];
			double dragPos[2];
			winToImage(e, winPos, imgPos);
			cropNCToImage(e, dragCropNCBegin, dragPos);

			cs.posCenter[0] -= (float)(dragPos[0] - imgPos[0]);
			cs.posCenter[1] -= (float)(dragPos[1] - imgPos[1]);
			clampCrop(e, cs);
			return true;
		}
	}
	return false;
}

bool CController::endDragCrop()
{
	bool old = (inDragCrop >= 2);
	inDragCrop = 0;
	return old;
}

void CController::adjustCropScale(float factor)
{
	CImageEntity& e = getCurrentInternal();
	bool enabled;
	TCropState& cs=getCropStateInternal(e,enabled);
	if (enabled) {
		cs.scale *= factor;
		if (cs.scale < 1.0e-6f) {
			cs.scale = 1.0e-6f;
		}
		float delta = 1.0f - e.crop.scale;
		if (delta > 1.0e-3f &&  delta < 1.0e-3f) {
			cs.scale = 1.0f;
		}
		if (cs.scale > 1.0f) {
			cs.scale = 1.0f;
		}
		clampCrop(e, cs);
	}
}

void CController::setCropScale(float factor)
{
	float baseFactor = 1.0f;
	CImageEntity& e = getCurrentInternal();
	bool enabled;
	TCropState& cs=getCropStateInternal(e,enabled);
	if (enabled) {
		cs.scale = baseFactor * factor;
		if (cs.scale > 1.0f) {
			cs.scale = 1.0f;
		}
		clampCrop(e, cs);
	}
}

void CController::setCropAspect(float a, float b)
{
	CImageEntity& e = getCurrentInternal();
	bool enabled;
	TCropState& cs = getCropStateInternal(e, enabled);
	if (enabled) {
		cs.aspectRatio[0] = a;
		cs.aspectRatio[1] = b;
		clampCrop(e, cs);
	}
}

void CController::addFile(const char *name)
{
	CImageEntity *e = new CImageEntity();
	e->filename = std::string(name);
	entities.push_back(e);
}
