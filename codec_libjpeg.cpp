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
	unsigned char *scanline = NULL;

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
		if (scanline) {
			free(scanline);
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

	uint16_t orientation = 1;
	if (cfg.autoRotate && haveExif) {
		orientation = img.getExif().orientation;
	}
	if (orientation < 1 || orientation > 8) {
		orientation = 1;
	}

	TImageInfo info;
	if (orientation > 4) {
		info.width = (size_t)cinfo.image_height;
		info.height = (size_t)cinfo.image_width;
	} else {
		info.width = (size_t)cinfo.image_width;
		info.height = (size_t)cinfo.image_height;
	}
	info.channels = (size_t)cinfo.num_components;
	info.bytesPerChannel = 1;

	if (img.create(info)) {
		unsigned char *data = (unsigned char*)img.getData();
		if (data) {
			size_t offset = (size_t)cinfo.image_width *  (size_t)cinfo.num_components;
			jpeg_start_decompress(&cinfo);
			if (orientation <= 1) {
				while (cinfo.output_scanline < cinfo.output_height) {
					JSAMPROW line = data + offset * (size_t)cinfo.output_scanline;
					jpeg_read_scanlines(&cinfo, &line, 1);
				}
				success = true;
			} else {
				scanline = (unsigned char*)malloc(offset);
				if (scanline) {
					unsigned char *pos;
					ptrdiff_t pixel_offset;
					ptrdiff_t row_offset;
					ptrdiff_t w = (ptrdiff_t)cinfo.image_width;
					ptrdiff_t n = (ptrdiff_t)cinfo.num_components;
					switch(orientation) {
						case 2:
							row_offset = (ptrdiff_t)info.width * n;
							pos = data + row_offset- n;
							pixel_offset = -n;
							break;
						case 3:
							row_offset = (ptrdiff_t)info.width * n;
							pos = data + (ptrdiff_t)info.height * row_offset - n;
							row_offset = -row_offset;
							pixel_offset = -n;
							break;
						case 4:
							row_offset = (ptrdiff_t)info.width * n;
							pos = data + (ptrdiff_t)(info.height-1) * row_offset;
							row_offset = -row_offset;
							pixel_offset = n;
							break;
						case 5:
							pixel_offset = (ptrdiff_t)info.width * n;
							pos = data + (ptrdiff_t)info.height * pixel_offset - n;
							row_offset = -n;
							pixel_offset = -pixel_offset;
							break;
						case 6:
							pixel_offset = (ptrdiff_t)info.width * n;
							row_offset = -n;
							pos = data + (ptrdiff_t)(info.width-1) * n;
							break;
						case 7:
							pos = data;
							pixel_offset = (ptrdiff_t)info.width * n;
							row_offset = n;
							break;
						case 8:
							pixel_offset = (ptrdiff_t)info.width * n;
							pos = data + pixel_offset * (ptrdiff_t)(info.height-1);
							pixel_offset = - pixel_offset;
							row_offset = n;
							break;
						default:
							pos = data;
							pixel_offset = n;
							row_offset = (ptrdiff_t)offset;
					}
					JSAMPROW line = scanline;
					while (cinfo.output_scanline < cinfo.output_height) {
						ptrdiff_t x,c;
						jpeg_read_scanlines(&cinfo, &line, 1);
						for (x=0; x<w; x++) {
							for (c=0; c<n; c++) {
								pos[x*pixel_offset+c] = scanline[x*n+c];
							}
						}
						pos += row_offset;

					}
					free(scanline);
					success = true;
				}
			}
			jpeg_finish_decompress(&cinfo);
			img.getExif().parsed = haveExif;
		}
	}
	fclose(infile);
	return success;
}

static bool encode(const char *filename, const CImage& img, const CCodecSettings& cfg)
{
	/* TODO: subsampling, progressive, etc... */

	const TImageInfo& info = img.getInfo();
	if (!img.hasData()) {
		return false;
	}
	if (info.bytesPerChannel != 1) {
		return false;
	}



	struct jpeg_compress_struct cinfo;
	struct fc_error_mgr jerr;
	const char* ptr;
	J_COLOR_SPACE in_color_space = JCS_RGB;
	J_COLOR_SPACE set_color_space = JCS_YCbCr;
	int num_components = 3;
	FILE* outfile = NULL;
	int i;

	switch (info.channels) {
		case 1:
			in_color_space = JCS_GRAYSCALE;
			set_color_space = JCS_GRAYSCALE;
			num_components = 1;
			break;
		case 2:
			in_color_space = JCS_UNKNOWN;
			set_color_space = JCS_UNKNOWN;
			num_components = 2;
			break;
		case 3:
			in_color_space = JCS_RGB;
			set_color_space = JCS_YCbCr;
			num_components = 3;
			break;
		case 4:
			in_color_space = JCS_CMYK;
			set_color_space = JCS_YCCK;
			num_components = 4;
			break;
		default:
			return false;
	}

	int quality = (int)(cfg.quality * 100.0f + 0.5f);
	if (quality > 100) {
		quality = 100;
	}
	if (quality < 1) {
		quality = 1;
	}

	outfile = util::fopen_wrapper(filename, "wb");
	if (!outfile) {
		return false;
	}
	ptr = (const char*)img.getData();
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_compress(&cinfo);
		if (outfile) {
			fclose(outfile);
		}
		return false;
	}

	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = (int)info.width;
	cinfo.image_height = (int)info.height;
	cinfo.input_components = (int)info.channels;
	cinfo.in_color_space = in_color_space;
	cinfo.num_components = num_components;

	jpeg_set_defaults(&cinfo);
	jpeg_set_colorspace(&cinfo, set_color_space);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_simple_progression(&cinfo);
	cinfo.dct_method = JDCT_ISLOW;
	cinfo.optimize_coding = TRUE;
	cinfo.smoothing_factor = cfg.jpegSmooth;
	for (i=0; i<cinfo.num_components; i++) {
		int hf = 1;
		int vf = 1;
		if (i == 0 && cinfo.num_components > 1) {
			switch (cfg.jpegSubsamplingMode) {
				case JPEG_SUBSAMPLING_444:
					hf = 1;
					vf = 1;
					break;
				case JPEG_SUBSAMPLING_422:
					hf = 2;
					vf = 1;
					break;
				case JPEG_SUBSAMPLING_420:
					hf = 2;
					vf = 2;
					break;
				default:
					// leave at library defaults
					hf = 0;
					vf = 0;
			}
		}
		if (hf > 0) {
			cinfo.comp_info[i].h_samp_factor = hf;
		}
		if (vf > 0) {
			cinfo.comp_info[i].v_samp_factor = vf;
		}
	}
	jpeg_start_compress(&cinfo, TRUE);
	/*
	if (comment) {
		jpeg_write_marker(&cinfo, JPEG_COM, (unsigned const char*)comment, strlen(comment));
	}
	*/
	size_t stride = info.width * info.channels * info.bytesPerChannel;
	while (cinfo.next_scanline < cinfo.image_height) {
		jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&ptr, 1);
		ptr += stride;
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	bool success = true;
	if (outfile) {
		if (ferror(outfile)) {
			success = false;
		}
		fclose(outfile);
	}
	return success;
}

CCodecDesc codecLibjpeg("libjpeg", supportsName,supportsFormat,decode,encode);

#endif /* WITH_LIBJPEG */
