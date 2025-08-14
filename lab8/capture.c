#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DEVICE_PATH "/dev/video0"
#define OUTPUT_FILE "webroot/camera_frame.jpg"
#define BUFFER_COUNT 4

struct buffer {
    void *start;
    size_t length;
};

static struct buffer *buffers = NULL;
static int fd = -1;

void errno_exit(const char *msg) {
    fprintf(stderr, "%s error %d, %s\n", msg, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int xioctl(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

void init_device(unsigned int *width, unsigned int *height) {
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    
    // Query device capabilities
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (errno == EINVAL) {
            fprintf(stderr, "%s is not a V4L2 device\n", DEVICE_PATH);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }
    
    // Verify that device supports video capture
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is not a video capture device\n", DEVICE_PATH);
        exit(EXIT_FAILURE);
    }
    
    // Verify that device supports streaming I/O
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming I/O\n", DEVICE_PATH);
        exit(EXIT_FAILURE);
    }
    
    // Set the format to MJPG
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1280;       // Highest resolution
    fmt.fmt.pix.height = 720;       // Highest resolution
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; // MJPG format
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        errno_exit("VIDIOC_S_FMT");
    }
    
    // Note the size actually set
    *width = fmt.fmt.pix.width;
    *height = fmt.fmt.pix.height;
    
    printf("Camera format set: %dx%d, format: %.4s\n", 
           fmt.fmt.pix.width, fmt.fmt.pix.height, 
           (char*)&fmt.fmt.pix.pixelformat);
}

void init_mmap() {
    struct v4l2_requestbuffers req;
    
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            fprintf(stderr, "%s does not support memory mapping\n", DEVICE_PATH);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }
    
    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", DEVICE_PATH);
        exit(EXIT_FAILURE);
    }
    
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    
    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            errno_exit("VIDIOC_QUERYBUF");
        }
        
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length,
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, buf.m.offset);
        
        if (buffers[i].start == MAP_FAILED) {
            errno_exit("mmap");
        }
    }
}

void start_capturing() {
    enum v4l2_buf_type type;
    
    for (unsigned int i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;
        
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            errno_exit("VIDIOC_QBUF");
        }
    }
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        errno_exit("VIDIOC_STREAMON");
    }
}

void stop_capturing() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        errno_exit("VIDIOC_STREAMOFF");
    }
}

void uninit_device() {
    for (unsigned int i = 0; i < BUFFER_COUNT; i++) {
        if (munmap(buffers[i].start, buffers[i].length) == -1) {
            errno_exit("munmap");
        }
    }
    free(buffers);
}

void close_device() {
    if (close(fd) == -1) {
        errno_exit("close");
    }
    fd = -1;
}

int main() {
    unsigned int width, height;
    
    // Open the device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Cannot open device %s: %s\n", DEVICE_PATH, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Initialize device
    init_device(&width, &height);
    
    // Initialize memory mapping
    init_mmap();
    
    // Start capturing
    start_capturing();

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // Dequeue a buffer with captured data
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            errno_exit("VIDIOC_DQBUF");
        }
    }
    
    // Save the frame to a file
    FILE *fout = fopen(OUTPUT_FILE, "wb");
    if (!fout) {
        perror("Cannot open output file");
        exit(EXIT_FAILURE);
    }
    
    // Write the MJPG data directly to a JPG file
    fwrite(buffers[buf.index].start, buf.bytesused, 1, fout);
    fclose(fout);
    printf("Saved %u bytes to %s\n", buf.bytesused, OUTPUT_FILE);
    
    // Return the buffer to the queue
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        errno_exit("VIDIOC_QBUF");
    }
    
    // Stop capturing
    stop_capturing();
    
    // Cleanup
    uninit_device();
    close_device();
    
    printf("Successfully captured one frame as JPEG\n");
    
    return 0;
}