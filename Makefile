# 指定交叉编译工具链
CROSS_COMPILE = /home/rlk/toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC = $(CROSS_COMPILE)gcc

# 编译参数
CFLAGS = -Wall -g
LDFLAGS = 
LDLIBS = -ldrm

CFLAGS += -I/home/rlk/toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include/drm/
#LDFLAGS += -L/path/to/your/rootfs/usr/lib


# 源文件 & 目标
SRCS = dmabuf-v4l2.c dmabuf.c drm_display.c
OBJS = $(SRCS:.c=.o)
TARGET = capture_display

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
