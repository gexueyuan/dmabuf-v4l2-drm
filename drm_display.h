/*
 * @Author: Gexueyaun 
 * @Date: 2025-04-29 11:18:24 
 * @Last Modified by:   Gexueyuan 
 * @Last Modified time: 2025-04-29 11:18:24 
 */

#ifndef DRM_DISPLAY_H
#define DRM_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// 前向声明 DRM 上下文结构体
typedef struct drm_context drm_context;

/**
 * 初始化 DRM 显示上下文
 *
 * @param ctx         输出参数，成功时返回指向 drm_context 的指针
 * @param width       显示输出宽度（像素）
 * @param height      显示输出高度（像素）
 * @param drm_format  DRM 图像格式（FourCC，如 DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12 等）
 * @param fullscreen  是否全屏显示
 * @return  返回 0 表示成功，非 0 表示失败
 */
int drm_display_init(struct drm_context **ctx, uint32_t width, uint32_t height, uint32_t drm_format, bool fullscreen);

/**
 * 显示一帧图像
 *
 * 将给定的 dma-buf 文件描述符对应的帧导入 DRM，并创建帧缓冲，通过 plane 显示出来。
 *
 * @param ctx         DRM 上下文指针，由 drm_display_init 初始化获得
 * @param dma_buf_fd  要显示的帧对应的 DMA-BUF 文件描述符
 * @return  返回 0 表示成功，否则返回负值错误码
 */
int drm_display_frame(struct drm_context *ctx, int dma_buf_fd);

/**
 * 清理 DRM 相关资源
 *
 * 删除当前帧缓冲并关闭 DRM 设备，释放上下文结构体等资源。
 *
 * @param ctx  DRM 上下文指针
 */
void drm_display_cleanup(struct drm_context *ctx);

#endif // DRM_DISPLAY_H
