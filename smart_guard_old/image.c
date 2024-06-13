#include "image.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

bool takePicture(char *ImagePath) {
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  struct v4l2_requestbuffers req;
  enum v4l2_buf_type type;

  int fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
  if (fd < 0) {
    perror("OPEN_WEBCAM");
    return false;
  }

  memset(&cap, 0, sizeof(cap));
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    perror("QUERY_WEBCAM_INFO");
    close(fd);
    return false;
  }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 640;
  fmt.fmt.pix.height = 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
    perror("SET_WEBCAM_FORMAT");
    close(fd);
    return false;
  }

  memset(&req, 0, sizeof(req));
  req.count = 1;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
    perror("REQUEST_BUFFERS");
    close(fd);
    return false;
  }

  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = 0;
  if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
    perror("QUERY_BUFFER");
    close(fd);
    return false;
  }

  void *buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                      buf.m.offset);
  if (buffer == MAP_FAILED) {
    perror("MAPPING_MEMORY");
    close(fd);
    return false;
  }

  if (ioctl(fd, VIDIOC_STREAMON, &buf.type) < 0) {
    perror("START_STREAM");
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    perror("QUEUE_BUFFER");
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
    perror("DEQUEUE_BUFFER");
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  FILE *file = fopen(ImagePath, "wb");
  if (!file) {
    perror("CREATE_FILE");
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    perror("CREATE_PNG_FILE_STRUCT");
    fclose(file);
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    perror("CREATE_PNG_INFO_STRUCT");
    png_destroy_write_struct(&png, NULL);
    fclose(file);
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  if (setjmp(png_jmpbuf(png))) {
    perror("WRITE_PNG_FILE");
    png_destroy_write_struct(&png, &info);
    fclose(file);
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  png_init_io(png, file);
  png_set_IHDR(png, info, fmt.fmt.pix.width, fmt.fmt.pix.height, 8,
               PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  int width = fmt.fmt.pix.width;
  int height = fmt.fmt.pix.height;
  png_bytep row = (png_bytep)malloc(3 * width * sizeof(png_byte));
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      int index = i * width * 2 + j * 2;
      unsigned char y = ((unsigned char *)buffer)[index];
      unsigned char u = ((unsigned char *)buffer)[index + 1];
      unsigned char v = ((unsigned char *)buffer)[index + 3];

      int r = (int)(y + 1.402 * (v - 128));
      int g = (int)(y - 0.344 * (u - 128) - 0.714 * (v - 128));
      int b = (int)(y + 1.772 * (u - 128));

      row[j * 3] = (png_byte)((r < 0) ? 0 : ((r > 255) ? 255 : r));
      row[j * 3 + 1] = (png_byte)((g < 0) ? 0 : ((g > 255) ? 255 : g));
      row[j * 3 + 2] = (png_byte)((b < 0) ? 0 : ((b > 255) ? 255 : b));
    }
    png_write_row(png, row);
  }
  free(row);

  png_write_end(png, NULL);
  png_destroy_write_struct(&png, &info);
  fclose(file);

  if (ioctl(fd, VIDIOC_STREAMOFF, &buf.type) < 0) {
    perror("STOP_STREAM");
    munmap(buffer, buf.length);
    close(fd);
    return false;
  }

  munmap(buffer, buf.length);
  close(fd);

  return true;
}
