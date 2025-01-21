#ifndef FASTCROP_CONTROLLER_H
#define FASTCROP_CONTROLLER_H

#include "image.h"
#include "glimage.h"

#include <string>
#include <vector>

class CCodecs; // forward codec.h
struct CCodecSettings; // forward codec.h

struct TWindowState {
	int dims[2];

	TWindowState() noexcept :
		dims{1920, 1080}
	{}
};

struct TDisplayState {
	float zoom;
	float aspectCorrection;

	TDisplayState() noexcept :
		zoom(1.0f),
		aspectCorrection(1.0f)
	{}
};


struct TCropState {
	float posCenter[2];
	float aspectRatio[2];
	float scale;

	TCropState() noexcept :
		posCenter{0.5f, 0.5f},
		aspectRatio{1.0f, 1.0f},
		scale(1.0f)
	{}
};

const unsigned int FLAG_ENTITY_IMAGE = 0x1;
const unsigned int FLAG_ENTITY_IMAGE_PENDING = 0x2;
const unsigned int FLAG_ENTITY_GLIMAGE = 0x4;
const unsigned int FLAG_ENTITY_GLIMAGE_PENDING = 0x8;
const unsigned int FLAG_ENTITY_CROPPED = 0x10;

struct CImageEntity {
	std::string filename;
	CImage image;
	CGLImage glImage;

	TDisplayState display;
	TCropState crop;

	unsigned int flags;

	CImageEntity() :
		flags(0)
	{}
};


class CController {
	private:
		CCodecs& codecs;
		const CCodecSettings& decodeSettings;
		const CCodecSettings& encodeSettings;
		TWindowState windowState;

		std::vector<CImageEntity*> entities;
		CImageEntity dummy;

		size_t currentEntity;
		TCropState currentCropSate;

		int inDragCrop;
		double dragCropMouseBegin[2];
		double dragCropNCBegin[2];

		bool uploadGLImage(CImageEntity& e);
		void dropGLImage(CImageEntity& e);
		bool prepareImageEntity(CImageEntity& e);

		CImageEntity& getCurrentInternal();

		void winToNC(const double winPos[2], double nc[2]) const;
		void NCtoWin(const double nc[2], double winPos[2]) const;
		void NCtoImage(const CImageEntity& e, const double nc[2], double imgPos[2]) const;
		void imageToNC(const CImageEntity& e, const double imgPos[2], double nc[2]) const;
		void winToImage(const CImageEntity& e, const double winPos[2], double imgPos[2]) const;
		void imageToWin(const CImageEntity& e, const double imgPos[2], double winPos[2]) const;

		TCropState& getCropStateInternal(CImageEntity& e, bool& croppingEnabled);
		void getCropSizeNC(const TImageInfo& info, const TCropState& cs, double s[2]) const;
		void imageToCropNC(const CImageEntity& e, const double imgPos[2], double cropPosNC[2]) const;
		void cropNCToImage(const CImageEntity& e, const double cropPosNC[2], double imgPos[2]) const;
		void clampCrop(const CImageEntity& e, TCropState& cs);

	public:
		CController(CCodecs& c, const CCodecSettings& ds, const CCodecSettings& es);
		~CController();

		CController(const CController& other) = delete;
		CController(CController&& other) = delete;
		CController& operator=(const CController& other) = delete;
		CController& operator=(CController&& other) = delete;

		bool initGL();
		void dropGL();

		void setWindowSize(int w, int h) noexcept;
		const TWindowState& getWindowState() const noexcept;

		const CImageEntity& getCurrent();
		const TDisplayState& getDisplayState(const CImageEntity& e) const;
		const TCropState& getCropState(const CImageEntity& e, bool& croppingEnabled) const;
		void applyCropping(const TImageInfo& img, const TCropState& cs, int32_t pos[2], int32_t size[2]) const;

		void getDisplayTransform(const CImageEntity& e, double scale[2], double offset[2], bool minusOneToOne) const;

		void adjustZoom(float factor);
		void setZoom(float factor, bool relativeToPixels = false);

		void addFile(const char *name);

		bool beginDragCrop(const double winPos[2]);
		bool doDragCrop(const double winPos[2]);
		bool endDragCrop();

		void adjustCropScale(float factor);
		void setCropScale(float factor);
		void setCropAspect(float a, float b);
};

#endif /* !FASTCROP_CONTROLLER_H*/
