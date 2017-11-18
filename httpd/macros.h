#ifndef _Nullable
#define _Nullable
#endif

#ifndef _Nonnull
#define _Nonnull
#endif

#define __DARWIN_STRUCT_STAT64 {                                        \
	dev_t		st_dev;			/* [XSI] ID of device containing file */ \
	mode_t		st_mode;		/* [XSI] Mode of file (see below) */ \
	nlink_t		st_nlink;		/* [XSI] Number of hard links */ \
	__darwin_ino64_t st_ino _Alignas(8);    /* [XSI] File serial number */ \
	uid_t		st_uid;			/* [XSI] User ID of the file */ \
	gid_t		st_gid;			/* [XSI] Group ID of the file */ \
	dev_t		st_rdev;		/* [XSI] Device ID */   \
	__DARWIN_STRUCT_STAT64_TIMES                                    \
            off_t		st_size _Alignas(8); /* [XSI] file size, in bytes */ \
	blkcnt_t	st_blocks _Alignas(8);	/* [XSI] blocks allocated for file */ \
	blksize_t	st_blksize;		/* [XSI] optimal blocksize for I/O */ \
	__uint32_t	st_flags;		/* user defined flags for file */ \
	__uint32_t	st_gen;			/* file generation number */ \
	__int32_t	st_lspare;		/* RESERVED: DO NOT USE! */ \
	__int64_t	st_qspare[2] _Alignas(8);/* RESERVED: DO NOT USE! */ \
    }
