#define FUSE_USE_VERSION 30
#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define return_ok_or_errno(func)                                               \
	errno = 0;                                                             \
	return (func != 0) ? -errno : 0;

#define BH_FUSE_OPT_KEY(t, p, v)                                               \
	{                                                                      \
		t, offsetof(struct options, p), v                              \
	}

enum { KEY_VERSION,
       KEY_HELP,
};

static int bh_getattr(const char *path, struct stat *out_st)
{
	return_ok_or_errno(stat(path, out_st));
}

static int bh_fgetattr(const char *path, struct stat *out_st,
		       struct fuse_file_info *fi)
{
	(void)path;

	FILE *fp = (FILE *)fi->fh;
	int fd = fileno(fp);

	return_ok_or_errno(fstat(fd, out_st));
}

static int bh_readlink(const char *path, char *out_link, size_t size)
{
	return_ok_or_errno(readlink(path, out_link, size));
}

static int bh_mknod(const char *path, mode_t mode, dev_t dev)
{
	return_ok_or_errno(mknod(path, mode, dev));
}

static int bh_mkdir(const char *path, mode_t mode)
{
	return_ok_or_errno(mkdir(path, mode));
}

static int bh_unlink(const char *path)
{
	return_ok_or_errno(unlink(path));
}

static int bh_rmdir(const char *path)
{
	return_ok_or_errno(rmdir(path));
}

static int bh_symlink(const char *oldpath, const char *newpath)
{
	return_ok_or_errno(symlink(oldpath, newpath));
}

static int bh_rename(const char *oldpath, const char *newpath)
{
	return_ok_or_errno(rename(oldpath, newpath));
}

static int bh_link(const char *oldpath, const char *newpath)
{
	return_ok_or_errno(link(oldpath, newpath));
}

static int bh_chmod(const char *path, mode_t mode)
{
	return_ok_or_errno(chmod(path, mode));
}

static int bh_chown(const char *path, uid_t uid, gid_t gid)
{
	return_ok_or_errno(chown(path, uid, gid));
}

static int bh_access(const char *path, int mode)
{
	return_ok_or_errno(access(path, mode));
}

static int bh_utimens(const char *path, const struct timespec tv[2])
{
	return_ok_or_errno(utimensat(-1, path, tv, 0));
}

static int bh_statfs(const char *path, struct statvfs *out_stat)
{
	return_ok_or_errno(statvfs(path, out_stat));
}

static int bh_setxattr(const char *path, const char *name, const char *value,
		       size_t size, int flags)
{
	return_ok_or_errno(setxattr(path, name, value, size, flags));
}

static int bh_getxattr(const char *path, const char *name, char *value,
		       size_t size)
{
	return_ok_or_errno(getxattr(path, name, value, size));
}

static int bh_listxattr(const char *path, char *list, size_t size)
{
	return_ok_or_errno(listxattr(path, list, size));
}

static int bh_removexattr(const char *path, const char *name)
{
	return_ok_or_errno(removexattr(path, name));
}

/* directory access */
static int bh_opendir(const char *path, struct fuse_file_info *fi)
{
	errno = 0;
	DIR *dirp = opendir(path);

	fi->fh = (uint64_t)dirp;

	return (dirp == NULL) ? -errno : 0;
}

static int bh_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		      off_t off, struct fuse_file_info *fi)
{
	(void)path;

	DIR *dirp = (DIR *)fi->fh;
	struct dirent *next;

	while (true) {
		errno = 0;
		next = readdir(dirp);

		if (next != NULL) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = next->d_ino;
			st.st_mode = next->d_type;

			if (filler(buf, next->d_name, &st, off) == 1) {
				break;
			}
		} else {
			break;
		}
	}

	return -errno;
}

static int bh_releasedir(const char *path, struct fuse_file_info *fi)
{
	(void)path;
	return_ok_or_errno(closedir((DIR *)fi->fh));
}

static int bh_fsyncdir(const char *path, int datasync,
		       struct fuse_file_info *fi)
{
	(void)path;

	DIR *dirp = (DIR *)fi->fh;
	int fd = dirfd(dirp);

	errno = 0;
	if (datasync != 0) {
		return (fdatasync(fd) != -1) ? 0 : -errno;
	} else {
		return (fsync(fd) != -1) ? 0 : -errno;
	}
}

static int bh_truncate(const char *path, off_t length)
{
	return_ok_or_errno(truncate(path, length));
}

static int bh_open(const char *path, struct fuse_file_info *fi)
{
	errno = 0;
	FILE *fp = fopen(path, "r+");

	if (fp == NULL) {
		return errno;
	} else {
		fi->fh = (uint64_t)fp;
		return 0;
	}
}

static int bh_read(const char *path, char *buf, size_t buf_size, off_t off,
		   struct fuse_file_info *fi)
{
	(void)path;

	FILE *fp = (FILE *)fi->fh;
	int fd = fileno(fp);

	struct stat st;
	bzero(&st, sizeof(st));
	fstat(fd, &st);

	size_t rsize = ((off + buf_size) > (size_t)st.st_size)
			   ? (st.st_size - off)
			   : buf_size;

	bzero(buf, buf_size);
	return rsize;
}

static int bh_write(const char *path, const char *buf, size_t size, off_t off,
		    struct fuse_file_info *fi)
{
	(void)path;
	(void)buf;

	FILE *fp = (FILE *)fi->fh;
	int fd = fileno(fp);

	struct stat st;
	bzero(&st, sizeof(st));

	errno = 0;
	if (fstat(fd, &st) == -1) {
		return errno;
	}

	size_t newsize = 0;
	size_t oldsize = st.st_size;

	if (off + size > SIZE_MAX) {
		newsize = SIZE_MAX;
	} else {
		newsize = off + size;
	}

	if (newsize > oldsize) {
		errno = 0;
		if (ftruncate(fd, newsize) == -1) {
			return -errno;
		}
		errno = 0;
		if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0,
			      newsize) == -1) {
			return -errno;
		}
	}
	return size;
}

static int bh_release(const char *path, struct fuse_file_info *fi)
{
	(void)path;

	FILE *fp = (FILE *)fi->fh;

	if (fclose(fp) != 0) {
		return -EIO;
	}

	return 0;
}

static int bh_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	(void)path;
	(void)datasync;
	(void)fi;

	return 0;
}

static int bh_ftruncate(const char *path, off_t length,
			struct fuse_file_info *fi)
{
	(void)path;

	FILE *fp = (FILE *)fi->fh;
	int fd = fileno(fp);

	errno = 0;
	if (ftruncate(fd, length) == -1) {
		return -errno;
	}
	errno = 0;
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0,
		      length) == -1) {
		return -errno;
	}

	return 0;
}

static int bh_fallocate(const char *path, int mode, off_t off, off_t len,
			struct fuse_file_info *fi)
{
	(void)path;
	(void)mode;

	int ret = bh_write(NULL, NULL, off, len, fi);

	return (ret == len) ? 0 : -EIO;
}

static struct fuse_operations bh_fuse_ops = {
    .access = &bh_access,
    .chmod = &bh_chmod,
    .chown = &bh_chown,
    .fallocate = &bh_fallocate,
    .fgetattr = &bh_fgetattr,
    .fsync = &bh_fsync,
    .fsyncdir = &bh_fsyncdir,
    .ftruncate = &bh_ftruncate,
    .getattr = &bh_getattr,
    .getxattr = &bh_getxattr,
    .link = &bh_link,
    .listxattr = &bh_listxattr,
    .mkdir = &bh_mkdir,
    .mknod = &bh_mknod,
    .open = &bh_open,
    .opendir = &bh_opendir,
    .read = &bh_read,
    .readdir = &bh_readdir,
    .readlink = &bh_readlink,
    .release = &bh_release,
    .releasedir = &bh_releasedir,
    .removexattr = &bh_removexattr,
    .rename = &bh_rename,
    .rmdir = &bh_rmdir,
    .setxattr = &bh_setxattr,
    .statfs = &bh_statfs,
    .symlink = &bh_symlink,
    .truncate = &bh_truncate,
    .unlink = &bh_unlink,
    .utimens = &bh_utimens,
    .write = &bh_write,
};

int main(int argc, char *argv[])
{
	int ret = fuse_main(argc, argv, &bh_fuse_ops, NULL);
	return ret;
}
