#include "exif.h"
#include "util.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

typedef enum {
	FC_META_TAG_GENERIC = 0,
	FC_META_TAG_TIFF,
	FC_META_TAG_EXIF,
} TMetaTagType;

typedef enum {
	FC_META_ST_UNKNOWN = 0,
	FC_META_ST_INVALID,
	FC_META_ST_EXIF,
	FC_META_ST_TIFF,
	FC_META_ST_IFD,
	FC_META_ST_SUBIFD,
} TMetaTagSubType;

typedef enum {
	FC_META_DTYPE_TIFF_BEGIN_MARKER = 0,
	FC_META_DTYPE_UNUSED = FC_META_DTYPE_TIFF_BEGIN_MARKER,
	FC_META_DTYPE_BYTE,
	FC_META_DTYPE_ASCII,
	FC_META_DTYPE_SHORT,
	FC_META_DTYPE_LONG,
	FC_TIFF_RATIONAL,
	// TIFF 6.0
	FC_META_DTYPE_SBYTE,
	FC_META_DTYPE_UNDEFINED_BYTE,
	FC_META_DTYPE_SSHORT,
	FC_META_DTYPE_SLONG,
	FC_META_DTYPE_SRATIONAL,
	FC_META_DTYPE_FLOAT,
	FC_META_DTYPE_DOUBLE,
	FC_META_DTYPE_TIFF_END_MARKER = FC_META_DTYPE_DOUBLE,
	FC_META_DTYPE_END_MARKER = FC_META_DTYPE_TIFF_END_MARKER,
} TMetaDType;

typedef struct {
	uint16_t tag;
	uint16_t type;
	uint32_t count;
	uint32_t offset;
} fc_tiff_ifd_entry_t;

typedef struct {
	uint32_t offset;
	unsigned int index;
	TMetaTagType baseType;
	uint16_t count;
	unsigned int callbackFlags;
} fc_tiff_ifd_ctx_t;

typedef struct fc_tiff_decoder_s {
	const uint8_t* data; 
	uint32_t current_offset;
	size_t size;
	uint16_t endian;
	unsigned int error_status;
	unsigned int img_index;
	unsigned int callbackFlags;

	uint16_t(*get2)(const uint8_t* ptr);
	uint32_t(*get4)(const uint8_t* ptr);
	float(*getf32)(const uint8_t* ptr);
	double(*getf64)(const uint8_t* ptr);
	uint32_t ifd0;

	fc_tiff_ifd_ctx_t img_ifd;
	uint32_t meta_offset; /* recursive collector */

	unsigned int (*handleEntry)(struct fc_tiff_decoder_s* td, const fc_tiff_ifd_entry_t* e, uint32_t offset, fc_tiff_ifd_ctx_t* ifd);
	void(*handleRecursion)(int mode, TMetaTagType baseType, fc_tiff_decoder_s* td, const fc_tiff_ifd_entry_t* e, uint32_t offset, fc_tiff_ifd_ctx_t* ifd, TMetaTagSubType, unsigned int idx);
	void* userPtr;
	//TCtx* ctx;
} fc_tiff_decoder_t;

#define FC_TIFF_OUT_OF_BOUNDS 0x1
#define FC_TIFF_READ_ERROR	  0x2

#define FC_META_CALLBACK_ABORT               0x1
#define FC_META_CALLBACK_ABORT_CURRENT_LEVEL 0x2

/* forward declarations */
static uint32_t
fc_tiff_decode_ifd(fc_tiff_decoder_t* td, uint32_t offset, int end, unsigned int ifd_idx, TMetaTagType baseType);
//static void
//fc_tiff_get_values(fc_tiff_decoder_t* td, void* outptr, TMetaDType type, uint32_t count, uint32_t offset);


static const uint32_t fc_tiff_size[FC_META_DTYPE_TIFF_END_MARKER+1] = {
	0, /* undefined */
	1, /* byte */
	1, /* ascii */
	2, /* short */
	4, /* long */
	8, /* rational */
	1, /* sbyte */
	1, /* undefined byte */
	2, /* sshort */
	4, /* slong */
	8, /* srational */
	4, /* float */
	8, /* double */
};

/* tags */
#define FC_TIFF_SUBIFD 0x14a
#define FC_TIFF_EXIF 0x8769

#define FC_TIFF_EXIF_JPEG_OFFSET 0x201
#define FC_TIFF_EXIF_JPEG_SIZE 0x202


static void
fc_tiff_ifd_init(fc_tiff_ifd_ctx_t* ifd)
{
	ifd->offset = 0;
	ifd->count = 0;
	ifd->baseType = FC_META_TAG_TIFF;
	ifd->index = 0;
	ifd->callbackFlags = 0;
}

static void
fc_tiff_decoder_init(fc_tiff_decoder_t* td)
{
	td->data = NULL;
	td->current_offset = 0;
	td->size = 0;
	td->error_status = 0;
	td->img_index = 0;
	td->callbackFlags = 0;
	td->meta_offset = 0;
	td->handleEntry = NULL;
	td->handleRecursion = NULL;
	td->userPtr = NULL;
	//td->ctx = ctx;
	fc_tiff_ifd_init(&td->img_ifd);
}

static uint16_t
fc_tiff_get2le(const uint8_t* ptr)
{
	uint16_t value;

	value = (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
	return value;
}

static uint16_t
fc_tiff_get2be(const uint8_t* ptr)
{
	uint16_t value;

	value = ((uint16_t)ptr[0] << 8) | (uint16_t)ptr[1];
	return value;
}

static uint32_t
fc_tiff_get4le(const uint8_t* ptr)
{
	uint32_t value;

	value = (uint32_t)ptr[0] |
		((uint32_t)ptr[1] << 8) |
		((uint32_t)ptr[2] << 16) |
		((uint32_t)ptr[3] << 24);
	return value;
}

static uint32_t
fc_tiff_get4be(const uint8_t* ptr)
{
	uint32_t value;

	value = ((uint32_t)ptr[0] << 24) |
		((uint32_t)ptr[1] << 16) |
		((uint32_t)ptr[2] << 8) |
		((uint32_t)ptr[3]);
	return value;
}

static float
fc_tiff_getf32direct(const uint8_t* ptr)
{
	return *((float*)ptr);
}

static float
fc_tiff_getf32swap(const uint8_t* ptr)
{
	uint8_t buf[4];

	buf[0] = ptr[3];
	buf[1] = ptr[2];
	buf[2] = ptr[1];
	buf[3] = ptr[0];
	return *(float*)buf;
}

static double
fc_tiff_getf64direct(const uint8_t* ptr)
{
	return *((double*)ptr);
}

static double
fc_tiff_getf64swap(const uint8_t* ptr)
{
	uint8_t buf[8];

	buf[0] = ptr[7];
	buf[1] = ptr[6];
	buf[2] = ptr[5];
	buf[3] = ptr[4];
	buf[4] = ptr[3];
	buf[5] = ptr[2];
	buf[6] = ptr[1];
	buf[7] = ptr[0];
	return *(double*)buf;
}

static const uint8_t *
fc_tiff_get_data_offset(fc_tiff_decoder_t* td, uint32_t offset, uint8_t *buf, uint32_t size)
{
	if (size < 1) {
		return NULL;
	}
	if (size < 1 || offset >= td->size || offset + size - 1 >= td->size) {
		td->error_status |= FC_TIFF_OUT_OF_BOUNDS;
		return NULL;
	}

	if (td->data) {
		td->current_offset = offset + size;
		return td->data + offset;
	}
	return NULL;
}

static void
fc_tiff_shift_offset(fc_tiff_decoder_t* td, uint32_t value)
{
	if (td->size < value) {
		return;
	}

	if (td->data) {
		td->data += value;
		td->size -= value;
		td->current_offset = td->size;
	}
}

static const uint8_t*
fc_tiff_get_data_offset_alloc(fc_tiff_decoder_t* td, uint32_t offset, uint32_t size, uint8_t* bufSmall, uint32_t bufSmallSize, uint8_t** bufBig)
{
	*bufBig = NULL;
	if (td->data || (bufSmall && size <= bufSmallSize)) {
		return fc_tiff_get_data_offset(td, offset, bufSmall, size);
	}
	*bufBig = (uint8_t*)malloc(size);
	return fc_tiff_get_data_offset(td, offset, *bufBig, size);
}


static int
fc_tiff_get_data_offset_copy(fc_tiff_decoder_t* td, uint32_t offset, uint8_t* buf, uint32_t size)
{
	const uint8_t* dptr = fc_tiff_get_data_offset(td, offset, buf, size);
	if (dptr && buf && (dptr != buf)) {
		memcpy(buf, dptr, size);
	} 
	if (!dptr && buf) {
		memset(buf, 0, size);
	}
	return (dptr != NULL);
}

static uint8_t
fc_tiff_get1_offset(fc_tiff_decoder_t* td, uint32_t offset)
{
	uint8_t buf;
	const uint8_t* ptr = fc_tiff_get_data_offset(td, offset, &buf, 1);
	if (!ptr) {
		return 0;
	}
	return ptr[0];
}


static uint16_t
fc_tiff_get2_offset(fc_tiff_decoder_t* td, uint32_t offset)
{
	uint8_t buf[2];
	const uint8_t* ptr = fc_tiff_get_data_offset(td, offset, buf, sizeof(buf));
	if (!ptr) {
		return 0;
	}

	return td->get2(ptr);
}

static uint32_t
fc_tiff_get4_offset(fc_tiff_decoder_t* td, uint32_t offset)
{
	uint8_t buf[4];
	const uint8_t* ptr = fc_tiff_get_data_offset(td, offset, buf, sizeof(buf));
	if (!ptr) {
		return 0;
	}
	return td->get4(ptr);
}

static float
fc_tiff_getf32_offset(fc_tiff_decoder_t* td, uint32_t offset)
{
	uint8_t buf[4];
	const uint8_t* ptr = fc_tiff_get_data_offset(td, offset, buf, sizeof(buf));
	if (!ptr) {
		return 0.0f;
	}
	return td->getf32(ptr);
}

static double
fc_tiff_getf64_offset(fc_tiff_decoder_t* td, uint32_t offset)
{
	uint8_t buf[8];
	const uint8_t* ptr = fc_tiff_get_data_offset(td, offset, buf, sizeof(buf));
	if (!ptr) {
		return 0.0;
	}
	return td->getf64(ptr);
}

static void
fc_tiff_decode_ifd_entry(fc_tiff_decoder_t* td, uint32_t offset, fc_tiff_ifd_entry_t* e)
{
	e->tag = fc_tiff_get2_offset(td, offset);
	e->type = fc_tiff_get2_offset(td, offset + 2);
	e->count = fc_tiff_get4_offset(td, offset + 4);
	e->offset = fc_tiff_get4_offset(td, offset + 8);
}

static int
fc_tiff_get_value_at_idx(fc_tiff_decoder_t* td, void* outptr, TMetaDType type, uint32_t idx, uint32_t offset)
{
	uint8_t* ptr;
	int r = 0;
	ptr = (uint8_t*)outptr;

	switch (type) {
	case FC_META_DTYPE_BYTE:
	case FC_META_DTYPE_ASCII:
	case FC_META_DTYPE_UNDEFINED_BYTE:
	case FC_META_DTYPE_SBYTE:
		offset += idx;
		fc_tiff_get_data_offset_copy(td, offset, ptr, 1);
		r = 1;
		break;
	case FC_META_DTYPE_SHORT:
	case FC_META_DTYPE_SSHORT:
		offset += idx * 2;
		*(uint16_t*)ptr = fc_tiff_get2_offset(td, offset);
		r = 2;
		break;
	case FC_META_DTYPE_LONG:
	case FC_META_DTYPE_SLONG:
		offset += idx * 4;
		*(uint32_t*)ptr = fc_tiff_get4_offset(td, offset);
		r = 4;
		break;
	case FC_TIFF_RATIONAL:
	case FC_META_DTYPE_SRATIONAL:
		offset += idx * 8;
		*(uint32_t*)ptr = fc_tiff_get4_offset(td, offset);
		ptr += sizeof(uint32_t);
		offset += 4;
		*(uint32_t*)ptr = fc_tiff_get4_offset(td, offset);
		r = 8;
		break;
	case FC_META_DTYPE_FLOAT:
		offset += idx * 4;
		*(float*)ptr = fc_tiff_getf32_offset(td, offset); 
		r = 4;
		break;
	case FC_META_DTYPE_DOUBLE:
		offset += idx * 8;
		*(double*)ptr = fc_tiff_getf64_offset(td, offset);
		r = 8;
		break;
	default:
		util::warn("TIFF decoder: unsupported type %d",(int)type);
	}

	return r;
}

static int
fc_tiff_get_value_at_idx_as_u4(fc_tiff_decoder_t* td, uint32_t *value, TMetaDType type, uint32_t idx, uint32_t offset)
{
	int r = 0;

	switch (type) {
	case FC_META_DTYPE_BYTE:
	case FC_META_DTYPE_ASCII:
	case FC_META_DTYPE_UNDEFINED_BYTE:
	case FC_META_DTYPE_SBYTE:
		offset += idx;
		*value = (uint32_t)fc_tiff_get1_offset(td, offset);
		r = 1;
		break;
	case FC_META_DTYPE_SHORT:
		offset += idx * 2;
		*value = (uint32_t)fc_tiff_get2_offset(td, offset);
		r = 2;
		break;
	case FC_META_DTYPE_LONG:
		offset += idx * 4;
		*value = (uint32_t)fc_tiff_get4_offset(td, offset);
		r = 4;
		break;
	default:
		*value = 0;
		util::warn("TIFF decoder: unsupported type %d",(int)type);
	}

	return r;
}

static int
fc_tiff_get_value_at_idx_as_f64(fc_tiff_decoder_t* td, double* value, TMetaDType type, uint32_t idx, uint32_t offset)
{
	int r = 0;

	switch (type) {
	case FC_META_DTYPE_BYTE:
	case FC_META_DTYPE_ASCII:
	case FC_META_DTYPE_UNDEFINED_BYTE:
		offset += idx;
		*value = (double)fc_tiff_get1_offset(td, offset);
		r = 1;
		break;
	case FC_META_DTYPE_SBYTE:
		offset += idx;
		*value = (double)((int8_t)fc_tiff_get1_offset(td, offset));
		r = 1;
		break;
	case FC_META_DTYPE_SHORT:
		offset += idx * 2;
		*value = (double)fc_tiff_get2_offset(td, offset);
		r = 2;
		break;
	case FC_META_DTYPE_SSHORT:
		offset += idx * 2;
		*value = (double)((int16_t)fc_tiff_get2_offset(td, offset));
		r = 2;
		break;
	case FC_META_DTYPE_LONG:
		offset += idx * 4;
		*value = (double)fc_tiff_get4_offset(td, offset);
		r = 4;
		break;
	case FC_META_DTYPE_SLONG:
		offset += idx * 4;
		*value = (double)((int32_t)fc_tiff_get4_offset(td, offset));
		r = 4;
		break;
	case FC_TIFF_RATIONAL:
		offset += idx * 8;
		*value = (double)fc_tiff_get4_offset(td, offset);
		offset += 4;
		*value = (*value)/(double)fc_tiff_get4_offset(td, offset);
		r = 8;
		break;
	case FC_META_DTYPE_SRATIONAL:
		offset += idx * 8;
		*value = (double)((int32_t)fc_tiff_get4_offset(td, offset));
		offset += 4;
		*value = (*value) / (double)((int32_t)fc_tiff_get4_offset(td, offset));
		r = 8;
		break;
	case FC_META_DTYPE_FLOAT:
		offset += idx * 4;
		*value = (double) fc_tiff_getf32_offset(td, offset);
		r = 4;
		break;
	case FC_META_DTYPE_DOUBLE:
		offset += idx * 8;
		*value = fc_tiff_getf64_offset(td, offset);
		r = 8;
		break;
	default:
		*value = 0.0f;
			//Warning("TIFF decoder: unsupported type")
	}

	return r;
}

static void
fc_tiff_get_values(fc_tiff_decoder_t* td, void* outptr, TMetaDType type, uint32_t count, uint32_t offset)
{
	uint8_t* ptr;
	uint32_t i;
	ptr = (uint8_t*)outptr;

	for (i = 0; i < count; i++) {
		int r;
		r = fc_tiff_get_value_at_idx(td, ptr, type, i, offset);
		if (r < 1) {
			break;
		}
		ptr += r;
	}
}

static void
fc_tiff_get_u4(fc_tiff_decoder_t* td, uint32_t* outptr, TMetaDType type, uint32_t count, uint32_t offset)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		int read = fc_tiff_get_value_at_idx_as_u4(td, &outptr[i], type, i, offset);
		offset += read;
	}
}

static void
fc_tiff_get_f64(fc_tiff_decoder_t* td, double* outptr, TMetaDType type, uint32_t count, uint32_t offset)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		int read = fc_tiff_get_value_at_idx_as_f64(td, &outptr[i], type, i, offset);
		offset += read;
	}
}

#if 0
static int
fc_tiff_get_entry_values(fc_tiff_decoder_t* td, void* outptr, const fc_tiff_ifd_entry_t* e, uint32_t offset)
{
	const uint8_t* pos;

	pos = td->data + offset;
	fc_tiff_get_values(td, outptr, e->type, e->count, pos);
	return 0;
}
#endif


static unsigned int
fc_tiff_handle_entry(fc_tiff_decoder_t* td, const fc_tiff_ifd_entry_t* e, uint32_t offset, fc_tiff_ifd_ctx_t* ifd, TMetaTagType baseType)
{
	unsigned int returnFlags = 0;
	uint32_t suboffset;
	uint32_t size;
	//double value;
	uint32_t i;
	//double values[2];
	//double tmp[4];
	unsigned int subcnt;

	if (e->type <= (uint16_t)FC_META_DTYPE_TIFF_BEGIN_MARKER || e->type > FC_META_DTYPE_TIFF_END_MARKER) {
		/* ignore it */
		return 0;
	}

	size = fc_tiff_size[e->type] * e->count;
	if (size <= 4) {
		/* value is stored in the offset field */
		offset = offset + 8;
	} else {
		offset = e->offset;
	}

	if (offset + size > td->size) {
		//Warning("TIFF error: tag 0x%x: data out of bounds", e->tag);
		/* ignore it */
		return 0;
	}

	//Debug("TIFF: tag: 0x%x, type: 0x%x, count: %u, %s: 0x%x",e->tag,e->type,e->count,(offset != e->offset)?"value":"offset",e->offset);

	// Rekursion...
	switch (e->tag) {
	case FC_TIFF_EXIF:
		for (i = 0; i < e->count; i++) {
			if (td->handleRecursion) {
				td->handleRecursion(1, ifd->baseType, td, e, offset, ifd, FC_META_ST_EXIF, i);
			}

			fc_tiff_get_value_at_idx_as_u4(td, &suboffset, (TMetaDType)e->type, i, offset);
			subcnt = 0;
			while (suboffset) {
				suboffset = fc_tiff_decode_ifd(td, suboffset, 1, subcnt++, FC_META_TAG_EXIF);
			}
			if (td->handleRecursion) {
				td->handleRecursion(0, ifd->baseType, td, e, offset, ifd, FC_META_ST_EXIF, i);
			}
		}
		return 0;
		//break;
	case FC_TIFF_SUBIFD:
		for (i = 0; i < e->count; i++) {
			if (td->handleRecursion) {
				td->handleRecursion(1, ifd->baseType, td, e, offset, ifd, FC_META_ST_SUBIFD, i);
			}
			fc_tiff_get_value_at_idx_as_u4(td, &suboffset, (TMetaDType)e->type, i, offset);
			subcnt = 0;
			while (suboffset) {
				suboffset = fc_tiff_decode_ifd(td, suboffset, 1, subcnt++, FC_META_TAG_TIFF);
			}
			if (td->handleRecursion) {
				td->handleRecursion(0, ifd->baseType, td, e, offset, ifd, FC_META_ST_SUBIFD, i);
			}
		}
		return 0;
		//break;
	}
	if (td->handleEntry) {
		returnFlags = td->handleEntry(td, e, offset, ifd);
	}
	return returnFlags;
}

static void
fc_tiff_handle_ifd(fc_tiff_decoder_t* td, fc_tiff_ifd_ctx_t* ifd)
{
	td->img_index++;
}

static uint32_t
fc_tiff_decode_ifd(fc_tiff_decoder_t* td, uint32_t offset, int end, unsigned int ifd_index, TMetaTagType baseType)
{
	uint32_t next;
	uint32_t nextOffset;
	uint16_t i;
	fc_tiff_ifd_ctx_t ifd;

	fc_tiff_ifd_init(&ifd);
	ifd.offset = offset;
	ifd.index = ifd_index;
	ifd.baseType = baseType;

	if (td->callbackFlags & FC_META_CALLBACK_ABORT) {
		// neuen IFD gar nicht betreten
		return 0;
	}

	if (td->handleRecursion) {
		td->handleRecursion(1, ifd.baseType, td, NULL, 0, &ifd, FC_META_ST_IFD, ifd_index);
	}


	//Debug("TIFF: IFD at 0x%x",offset);
	ifd.count = fc_tiff_get2_offset(td, offset);
	if (td->error_status) {
		//Warning("TIFF error: decoding IFD failed");
		return 0;
	}
	//Debug("TIFF: IFD has %u entries",ifd.count);

	offset += 2;
	nextOffset = offset + ifd.count * 12;
	for (i = 0; i < ifd.count; i++) {
		unsigned int res;

		fc_tiff_ifd_entry_t e;
		fc_tiff_decode_ifd_entry(td, offset, &e);
		if (td->error_status) {
			//Warning("TIFF error: decoding IFD entry failed");
			return 0;
		}
		res = fc_tiff_handle_entry(td, &e, offset, &ifd, baseType);
		if (res & FC_META_CALLBACK_ABORT) {
			td->callbackFlags |= FC_META_CALLBACK_ABORT;
			ifd.callbackFlags |= FC_META_CALLBACK_ABORT_CURRENT_LEVEL;
		}
		if (res & FC_META_CALLBACK_ABORT_CURRENT_LEVEL) {
			ifd.callbackFlags |= FC_META_CALLBACK_ABORT_CURRENT_LEVEL;
		}
		res = td->callbackFlags | ifd.callbackFlags;
		if (res & (FC_META_CALLBACK_ABORT | FC_META_CALLBACK_ABORT_CURRENT_LEVEL)) {
			break;
		}
		offset += 12;
	}

	if (end) {
		next = fc_tiff_get4_offset(td, nextOffset);
	} else {
		next = 0;
	}

	if (td->handleRecursion) {
		td->handleRecursion(0, ifd.baseType, td, NULL, 0, &ifd, FC_META_ST_IFD, ifd_index);
	}


	if (td->error_status) {
		//Warning("TIFF error: decoding IFD failed");
		return 0;
	} else {
		fc_tiff_handle_ifd(td, &ifd);
	}


	return next;
}

static int
fc_tiff_decode_internal(fc_tiff_decoder_t* td, TMetaTagType baseType, bool mayHaveExifHeader)
{
	uint32_t offset;
	uint32_t value;
	uint8_t* ptr;
	uint8_t  hdr[8];
	int self_bigendian = 0;

	value = 0x01020304;
	ptr = (uint8_t*)&value;
	if (ptr[0] == 0x04) {
		self_bigendian = 1;
	}

	for (int pass = 0; pass < 2; pass++) {
		bool found = true;
		if (td->size < 8) {
			//Warning("TIFF error: not enough header data");
			return -1;
		}

		fc_tiff_get_data_offset_copy(td, 0, hdr, 2);
		td->endian = fc_tiff_get2le(hdr);
		switch (td->endian) {
		case 0x4949: /* little-endian file */
			td->get2 = fc_tiff_get2le;
			td->get4 = fc_tiff_get4le;
			if (self_bigendian) {
				td->getf32 = fc_tiff_getf32swap;
				td->getf64 = fc_tiff_getf64swap;
			} else {
				td->getf32 = fc_tiff_getf32direct;
				td->getf64 = fc_tiff_getf64direct;
			}
			break;
		case 0x4D4D: /* big-endian file */
			td->get2 = fc_tiff_get2be;
			td->get4 = fc_tiff_get4be;
			if (self_bigendian) {
				td->getf32 = fc_tiff_getf32direct;
				td->getf64 = fc_tiff_getf64direct;
			} else {
				td->getf32 = fc_tiff_getf32swap;
				td->getf64 = fc_tiff_getf64swap;
			}
			break;
		default:
			found = false;
		}
		if (found) {
			break;
		}
		if (pass > 0 || !mayHaveExifHeader) {
			//Warning("TIFF error: invalid endian specification");
			return -2;
		}

		// Zweiter Pass: eventuellen Exif-Header ueberlesen
		fc_tiff_get_data_offset_copy(td, 0, hdr, 8);
		size_t exifOffset = EXIFFindBegin(hdr, 8);
		if (exifOffset != (size_t)-1) {
			fc_tiff_shift_offset(td,(uint32_t)exifOffset + 6);
		}
	}

	if (fc_tiff_get2_offset(td, 2) != 42) {
		//Warning("TIFF error: not a TIFF file");
		return -3;
	}

	unsigned int ifd_index = 0;
	TMetaTagSubType mode;
	switch (baseType) {
	case FC_META_TAG_EXIF:
		mode = FC_META_ST_EXIF;
		break;
	case FC_META_TAG_TIFF:
		mode = FC_META_ST_TIFF;
		break;
	default:
		mode = FC_META_ST_UNKNOWN;
	}
	if (td->handleRecursion) {
		td->handleRecursion(1, baseType,  td, NULL, 0, NULL, mode, 0);
	}

	td->ifd0 = fc_tiff_get4_offset(td, 4);
	offset = td->ifd0;
	

	while (offset) {
		offset = fc_tiff_decode_ifd(td, offset, 1, ifd_index++, baseType);
	}
	if (td->handleRecursion) {
		td->handleRecursion(0, baseType, td, NULL, 0, NULL, mode, 0);
	}
	return 0;
}

static int
fc_tiff_decode_memory(fc_tiff_decoder_t* td, const uint8_t* data, size_t size, TMetaTagType baseType, bool mayHaveExifHeader)
{
	td->data = data;
	td->size = size;

	return fc_tiff_decode_internal(td, baseType, mayHaveExifHeader);
}



typedef struct {
	uint32_t offset;
	uint32_t size;
} fc_tiff_thumbnail_jpeg_t;

unsigned int handleEntryFindThumbnailJPEG(fc_tiff_decoder_t * td, const fc_tiff_ifd_entry_t * e, uint32_t offset, fc_tiff_ifd_ctx_t * ifd)
{
	fc_tiff_thumbnail_jpeg_t* thumb = (fc_tiff_thumbnail_jpeg_t*)td->userPtr;
	if (thumb->size > 0) {
		return FC_META_CALLBACK_ABORT;
	}
	if (e->tag == FC_TIFF_EXIF_JPEG_OFFSET) {
		fc_tiff_get_u4(td, &thumb->offset, (TMetaDType)e->type, 1, offset);
	} else if (e->tag == FC_TIFF_EXIF_JPEG_SIZE) {
		fc_tiff_get_u4(td, &thumb->size, (TMetaDType)e->type, 1, offset);
	}
	return 0;
}

const int stackDepth = 8;

typedef struct {
	TExifData *data;
	int depth;
	TMetaTagSubType stackT[stackDepth];
	unsigned        stackI[stackDepth];
} TExifParseCtx;

static bool getCurLevel(const TExifParseCtx *ctx, TMetaTagSubType& st, unsigned& idx)
{
	st = FC_META_ST_INVALID;
	idx = (unsigned)-1;
	if (ctx) {
		if (ctx->depth > 0 && ctx->depth <= stackDepth) {
			st = ctx->stackT[ctx->depth - 1];
			idx= ctx->stackI[ctx->depth - 1];
			return true;
		}
	}
	return false;
}

static void handleRecursionParse(int mode, TMetaTagType baseType, fc_tiff_decoder_t * td, const fc_tiff_ifd_entry_t * e, uint32_t offset, fc_tiff_ifd_ctx_t * ifd, TMetaTagSubType subType, unsigned int idx )
{
	TExifParseCtx *ctx = (TExifParseCtx*)td->userPtr;
	if (ctx) {
		if (mode) {
			if (ctx->depth < stackDepth) {
				ctx->stackT[ctx->depth] = subType;
				ctx->stackI[ctx->depth] = idx;
			}
			ctx->depth++;
		} else {
			ctx->depth--;
		}
	}
}

static unsigned int handleEntryParse(fc_tiff_decoder_t * td, const fc_tiff_ifd_entry_t * e, uint32_t offset, fc_tiff_ifd_ctx_t * ifd)
{
	TExifParseCtx *ctx = (TExifParseCtx*)td->userPtr;
	if (ctx) {
		TExifData* data = ctx->data;
		if (data) {
			uint32_t val[2];
			TMetaTagSubType st;
			unsigned idx;
			getCurLevel(ctx, st, idx);
			if (st == FC_META_ST_IFD && idx == 0) {
				if (e->tag == 0x112) {
					fc_tiff_get_u4(td, val, (TMetaDType)e->type, 1, offset);
					data->orientation = (uint16_t)val[0];
				}
			}
		}
	}
	return 0;
}

/*****************************************************************************
 * EXIF API                                                                  *
 *****************************************************************************/

extern size_t TIFFFindBegin(const void* data, size_t maxSize)
{
	size_t i;

	if (!data || maxSize < 2) {
		return (size_t)-1;
	}

	const uint8_t* ptr = (const uint8_t*)data;

	for (i = 0; i < maxSize - 1; i++) {
		if ((ptr[i] == 0x49 && ptr[i + 1] == 0x49) || (ptr[i] == 0x4d && ptr[i + 1] == 0x4d)) {
			return i;
		}
		if ((ptr[i] == 0x49 && ptr[i + 1] == 0x49) || (ptr[i] == 0x4d && ptr[i + 1] == 0x4d)) {
			return i;
		}
	}
	return (size_t)-1;
}

extern size_t EXIFFindBegin(const void* data, size_t maxSize)
{
	size_t i;

	if (!data || maxSize < 2) {
		return (size_t)-1;
	}

	const uint8_t* ptr = (const uint8_t*)data;

	for (i = 0; i < maxSize - 1; i++) {
		if (TIFFFindBegin(ptr + i, 2) != (size_t)-1) {
			return i;
		}
		if (i + 3 < maxSize) {
			if ((ptr[i] == 0x45 || ptr[i] == 0x65) && (ptr[i + 1] == 0x58 || ptr[i + 1] == 0x78) && (ptr[i + 2] == 0x49 || ptr[i + 2] == 0x69) && (ptr[i + 3] == 0x46 || ptr[i + 3] == 0x66)) {
				// Exif-Header
				return i;
			}
		}
	}
	return (size_t)-1;
}

extern bool EXIFParse(TExifData& info, const void* data, size_t size)
{
	info.parsed = false;
	TExifParseCtx ctx;
	ctx.depth = 0;
	ctx.data = &info;

	fc_tiff_decoder_t td;
	fc_tiff_decoder_init(&td);
	td.userPtr = &ctx;
	td.handleEntry = handleEntryParse;
	td.handleRecursion = handleRecursionParse;
	if (fc_tiff_decode_memory(&td, (const uint8_t*)data, size, FC_META_TAG_EXIF, true)) {
		return false;
	}
	info.parsed = true;
	return true;
}
