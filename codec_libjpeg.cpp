#ifdef WITH_LIBJPEG

#include "codec_libjpeg.h"

#include "image.h"
#include "util.h"

#include <jpeglib.h>

#include <string.h>
#include <setjmp.h>

static bool supportsName(const char *filename, const char *ext, const CCodecSettings& cfg)
{
	(void)cfg;
	if (!ext) {
		ext = filename;
		if (!ext) {
			return false;
		}
	}
	
	if (!strcasecmp(ext,"jpg") || !strcasecmp(ext,"jpeg")) {
		return true;
	}
	return false;
}

static bool supportsFormat(const void *header, size_t size, const CCodecSettings& cfg)
{
	if (size >= 2) {
		const unsigned char *d = (const unsigned char*)header;
		if (d[0] == 0xff && d[1] == 0xd8) {
			return true;
		}
	}
	return false;
}

struct fc_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

void my_error_exit(j_common_ptr cinfo)
{
  struct fc_error_mgr* err = (struct fc_error_mgr*)cinfo->err;
  longjmp(err->setjmp_buffer, 1);
}
static bool decode(const char *filename, CImage& img, const CCodecSettings& cfg)
{
	(void)cfg;
	if (!filename) {
		return false;
	}
	struct jpeg_decompress_struct cinfo;
	struct fc_error_mgr jerr;
	jpeg_saved_marker_ptr marker;
	FILE* infile = NULL;

	if (filename) {
		infile = util::fopen_wrapper(filename, "rb");
		if (!infile) {
			return false;
		}
	}

	bool success = false;
	bool haveExif = false;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		if (infile) {
			fclose(infile);
		}
		return false;
	}
	
	jpeg_create_decompress(&cinfo);
	jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xffff); /* EXIF, Thumbnail */
	jpeg_stdio_src(&cinfo, infile);
  	jpeg_read_header(&cinfo, 1);

	for (marker = cinfo.marker_list; marker; marker = marker->next) {
		if (marker->marker == JPEG_APP0 + 1) {
			/* this could be an EXIF tag */
			if (marker->data_length > 6) {
				const char* exif = (const char *)marker->data;
				if (exif[0] == 'E' && exif[1] == 'x' && exif[2] == 'i' && exif[3] == 'f') {
					size_t exifSize = (size_t)(marker->data_length);
					EXIFParse(img.getExif(), exif, exifSize);
					haveExif = img.getExif().parsed;
				}
			}
		}
	}

	// TODO: cfg.autoRotate

	if (img.create(TImageInfo((size_t)cinfo.image_width, (size_t)cinfo.image_height, (size_t)cinfo.num_components, 1))) {
		unsigned char *data = (unsigned char*)img.getData();
		size_t offset = (size_t)cinfo.image_width *  (size_t)cinfo.num_components;
		if (data) {
			jpeg_start_decompress(&cinfo);
			while (cinfo.output_scanline < cinfo.output_height) {
				JSAMPROW line = data + offset * (size_t)cinfo.output_scanline;
				jpeg_read_scanlines(&cinfo, &line, 1);
			}
			jpeg_finish_decompress(&cinfo);
			img.getExif().parsed = haveExif;
			success = true;
		}
	}
	fclose(infile);
	return success;
}

CCodecDesc codecLibjpeg("libjpeg", supportsName,supportsFormat,decode,NULL);

#endif /* WITH_LIBJPEG */
