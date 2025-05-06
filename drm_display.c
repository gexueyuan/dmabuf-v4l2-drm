/*
 * @Author: Gexueyaun 
 * @Date: 2025-04-29 11:01:10 
 * @Last Modified by: Gexueyuan
 * @Last Modified time: 2025-04-29 11:27:24
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include "drm_display.h"

// DRM 上下文结构体定义，保存 DRM 设备和显示相关信息
struct drm_context {
    int drm_fd;               // DRM 设备文件描述符
    uint32_t connector_id;    // 选中的连接器ID
    uint32_t crtc_id;         // 选中的CRTC ID
    uint32_t plane_id;        // 用于显示的平面(plane) ID
    drmModeModeInfo mode;     // 当前显示模式信息
    bool fullscreen;          // 是否全屏显示标志
    uint32_t fb_id;           // 当前帧缓冲区 ID（用于后续释放）
    uint32_t width;           // 帧宽度
    uint32_t height;          // 帧高度
    uint32_t format;          // DRM 图像格式 (FourCC)
};

/**
 * 初始化 DRM 显示上下文
 *
 * 本函数打开 DRM 设备，选择一个已连接的连接器 (connector)，并找到对应的 CRTC 和 plane，设置显示模式。
 */
int drm_display_init(struct drm_context **ctx, uint32_t width, uint32_t height, uint32_t drm_format, bool fullscreen)
{
    // 分配上下文结构体
    struct drm_context *d = calloc(1, sizeof(*d));
    if (!d) {
        fprintf(stderr, "drm_display_init: 分配 drm_context 失败\n");
        return -1;
    }
    d->fullscreen = fullscreen;
    d->width = width;
    d->height = height;
    d->format = drm_format;
    d->fb_id = 0;

    // 打开 DRM 设备，默认使用第一个 DRM 设备节点
    const char *card = "/dev/dri/card0";
    d->drm_fd = open(card, O_RDWR | O_CLOEXEC);
    if (d->drm_fd < 0) {
        fprintf(stderr, "drm_display_init: 无法打开 DRM 设备 %s\n", card);
        free(d);
        return -1;
    }

    // 获取 DRM 资源，包括连接器、编码器、CRTC、plane 等
    drmModeRes *res = drmModeGetResources(d->drm_fd);
    if (!res) {
        fprintf(stderr, "drm_display_init: drmModeGetResources 失败\n");
        close(d->drm_fd);
        free(d);
        return -1;
    }

    // 查找第一个已连接并有模式的连接器
    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *tmp = drmModeGetConnector(d->drm_fd, res->connectors[i]);
        if (tmp && tmp->connection == DRM_MODE_CONNECTED && tmp->count_modes > 0) {
            conn = tmp;
            break;
        }
        drmModeFreeConnector(tmp);
    }
    if (!conn) {
        fprintf(stderr, "drm_display_init: 未找到可用的已连接连接器\n");
        drmModeFreeResources(res);
        close(d->drm_fd);
        free(d);
        return -1;
    }
    d->connector_id = conn->connector_id;

    // 选择显示模式：如果指定了 width/height，尝试匹配对应模式；否则使用第一个默认模式
    drmModeModeInfo mode = {0};
    if (width > 0 && height > 0) {
        for (int i = 0; i < conn->count_modes; i++) {
            if ((uint32_t)conn->modes[i].hdisplay == width && (uint32_t)conn->modes[i].vdisplay == height) {
                mode = conn->modes[i];
                break;
            }
        }
    }
    if (mode.hdisplay == 0 || mode.vdisplay == 0) {
        mode = conn->modes[0];
    }
    d->mode = mode;

    // 获取连接器的编码器，以便找到对应的 CRTC ID
    drmModeEncoder *enc = NULL;
    if (conn->encoder_id)
        enc = drmModeGetEncoder(d->drm_fd, conn->encoder_id);
    if (!enc) {
        // 如果连接器没有指定 encoder，则遍历所有 encoder
        for (int i = 0; i < res->count_encoders; i++) {
            enc = drmModeGetEncoder(d->drm_fd, res->encoders[i]);
            if (enc) {
                // 随便取一个encoder
                break;
            }
        }
    }
    if (!enc) {
        fprintf(stderr, "drm_display_init: 无法获取 Encoder\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(d->drm_fd);
        free(d);
        return -1;
    }
    d->crtc_id = enc->crtc_id;

    // 查找支持本 CRTC 且包含所需像素格式的 plane
    drmModePlaneRes *plane_res = drmModeGetPlaneResources(d->drm_fd);
    if (!plane_res) {
        fprintf(stderr, "drm_display_init: drmModeGetPlaneResources 失败\n");
        drmModeFreeEncoder(enc);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(d->drm_fd);
        free(d);
        return -1;
    }
    // 找到 CRTC 在资源列表中的索引
    int crtc_index = -1;
    for (int i = 0; i < res->count_crtcs; i++) {
        if (res->crtcs[i] == d->crtc_id) {
            crtc_index = i;
            break;
        }
    }
    // 遍历所有平面，找到一个可用于该 CRTC 并支持所需格式的平面
    for (int i = 0; i < plane_res->count_planes; i++) {
        drmModePlane *plane = drmModeGetPlane(d->drm_fd, plane_res->planes[i]);
        if (!plane) continue;
        // 检查此 plane 是否可以输出到指定 CRTC
        if (!(plane->possible_crtcs & (1 << crtc_index))) {
            drmModeFreePlane(plane);
            continue;
        }
        // 检查此 plane 是否支持所需的像素格式
        bool fmt_ok = false;
        for (int j = 0; j < plane->count_formats; j++) {
            if (plane->formats[j] == d->format) {
                fmt_ok = true;
                break;
            }
        }
        if (fmt_ok) {
            d->plane_id = plane->plane_id;
            drmModeFreePlane(plane);
            break;
        }
        drmModeFreePlane(plane);
    }
    if (d->plane_id == 0) {
        fprintf(stderr, "drm_display_init: 未找到支持格式的 plane，可能无法显示\n");
        // 如果没有找到合适 plane，可以继续，但 drm_display_frame 时可能失败
    }

    // 清理临时资源
    drmModeFreeEncoder(enc);
    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);

    // 返回上下文指针
    *ctx = d;
    return 0;
}

/**
 * 显示一帧图像
 *
 * 本函数使用 drmPrimeFDToHandle 将 DMA-BUF fd 导入为 DRM GEM 句柄，然后使用 drmModeAddFB2
 * 创建帧缓冲区，最后通过 drmModeSetPlane 将帧缓冲显示到屏幕对应的 plane 上。
 */
int drm_display_frame(struct drm_context *ctx, int dma_buf_fd)
{
    if (!ctx) {
        fprintf(stderr, "drm_display_frame: 上下文为空\n");
        return -1;
    }

    // 将 dma-buf fd 转换为 DRM GEM handle
    uint32_t handle;
    if (drmPrimeFDToHandle(ctx->drm_fd, dma_buf_fd, &handle) != 0) {
        fprintf(stderr, "drm_display_frame: drmPrimeFDToHandle 失败\n");
        return -1;
    }

    // 配置帧缓冲参数：handles, pitches, offsets
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    handles[0] = handle;
    // 根据不同格式计算行跨度(pitch)和偏移(offset)
    switch (ctx->format) {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_XBGR8888:
            pitches[0] = ctx->width * 4;  // 4 字节/像素
            break;
        case DRM_FORMAT_RGB565:
            pitches[0] = ctx->width * 2;  // 2 字节/像素
            break;
        case DRM_FORMAT_NV12:
            // NV12 格式有两个平面，Y 平面和 UV 平面，使用同一个 handle
            pitches[0] = ctx->width;
            pitches[1] = ctx->width;
            offsets[1] = ctx->width * ctx->height;
            handles[1] = handle;
            break;
        default:
            // 默认假设 4 字节/像素
            pitches[0] = ctx->width * 4;
            break;
    }

    // 添加帧缓冲区
    uint32_t fb_id;
    int ret = drmModeAddFB2(ctx->drm_fd, ctx->width, ctx->height, ctx->format,
                            handles, pitches, offsets, &fb_id, 0);
    if (ret != 0) {
        fprintf(stderr, "drm_display_frame: drmModeAddFB2 失败 (%s)\n", strerror(errno));
        return -1;
    }

    // 计算显示目标尺寸 (如果全屏则使用模式尺寸)
    uint32_t dst_w = ctx->fullscreen ? ctx->mode.hdisplay : ctx->width;
    uint32_t dst_h = ctx->fullscreen ? ctx->mode.vdisplay : ctx->height;

    // 将帧缓冲设置到 plane 上，目标坐标 (0,0)，宽高为 dst_w,dst_h
    // 源坐标 (0,0)，宽高为原始尺寸 (采用 16.16 固定点格式)
    ret = drmModeSetPlane(ctx->drm_fd,
                          ctx->plane_id,
                          ctx->crtc_id,
                          fb_id, 0,
                          0, 0, dst_w, dst_h,
                          0, 0, ctx->width << 16, ctx->height << 16);
    if (ret != 0) {
        fprintf(stderr, "drm_display_frame: drmModeSetPlane 失败 (%s)\n", strerror(errno));
        // 如果失败，删除此帧缓冲并返回错误
        drmModeRmFB(ctx->drm_fd, fb_id);
        return -1;
    }

    // 删除旧的帧缓冲（如果存在）
    if (ctx->fb_id != 0) {
        drmModeRmFB(ctx->drm_fd, ctx->fb_id);
    }
    // 更新当前帧缓冲 ID
    ctx->fb_id = fb_id;
    return 0;
}

/**
 * 清理 DRM 相关资源
 *
 * 删除当前帧缓冲并关闭 DRM 设备，释放 drm_context 对象。
 */
void drm_display_cleanup(struct drm_context *ctx)
{
    if (!ctx) return;

    // 删除当前帧缓冲
    if (ctx->fb_id != 0) {
        drmModeRmFB(ctx->drm_fd, ctx->fb_id);
    }
    // 关闭 DRM 设备
    if (ctx->drm_fd >= 0) {
        close(ctx->drm_fd);
    }
    // 释放上下文结构
    free(ctx);
}
