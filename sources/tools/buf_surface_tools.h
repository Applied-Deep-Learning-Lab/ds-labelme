

#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "gstnvdsmeta.h"



inline NvBufSurfaceCreateParams makeCreateParams(int gpu_id, int width, int height, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type);

inline NvBufSurfaceCreateParams makeCreateParamsBasedOn(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type);

inline NvBufSurface* createSurfaceBasedOn(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type);

inline NvBufSurfTransformRect makeTransformRect(int left, int top, int width, int height);

inline NvBufSurfTransformRect getTransformRect(NvBufSurface* ip_surf);

inline NvBufSurfTransformParams makeTransformParams(NvBufSurfTransformRect src_rect, NvBufSurfTransformRect dst_rect);

inline NvBufSurface* getNewColorTypeSurface(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type);

inline NvBufSurface* getSystemMemorySurface(NvBufSurface* ip_surf);
#ifndef BUF_SURFACE_TOOLS
#define BUF_SURFACE_TOOLS

inline NvBufSurfaceCreateParams makeCreateParams(int gpu_id, int width, int height, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type){
    NvBufSurfaceCreateParams params;
    
    params.gpuId  = gpu_id;
    params.width  = (guint) width;
    params.height = (guint) height;
    params.size = 0;
    params.isContiguous = true;
    params.colorFormat = color_type;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.memType = memory_type;
    return params;
}

inline NvBufSurfaceCreateParams makeCreateParamsBasedOn(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type){
    NvBufSurfaceCreateParams params;
    
    return makeCreateParams(
        ip_surf->gpuId,
        ip_surf->surfaceList[0].width, ip_surf->surfaceList[0].height,
        color_type, memory_type
    );
}

inline NvBufSurface* createSurfaceBasedOn(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type, NvBufSurfaceMemType memory_type){
    NvBufSurface* new_surf;
    auto params = makeCreateParamsBasedOn(ip_surf, color_type, memory_type);
    NvBufSurfaceCreate(&new_surf, ip_surf->batchSize, &params);
    return new_surf;
}

inline NvBufSurfTransformRect makeTransformRect(int left, int top, int width, int height){
    NvBufSurfTransformRect rect;
    rect.top = top;
    rect.left = left;
    rect.height = width;
    rect.width = height;
    return rect;
}

inline NvBufSurfTransformRect getTransformRect(NvBufSurface* ip_surf){
    return makeTransformRect(0, 0, ip_surf->surfaceList[0].width, ip_surf->surfaceList[0].height);
}

inline NvBufSurfTransformParams makeTransformParams(NvBufSurfTransformRect src_rect, NvBufSurfTransformRect dst_rect){
    NvBufSurfTransformParams transform;
    transform.dst_rect = &src_rect;
    transform.src_rect = &dst_rect;
    transform.transform_filter = NvBufSurfTransformInter_Nearest;
    transform.transform_flip = NvBufSurfTransform_None;
    transform.transform_flag = NVBUFSURF_TRANSFORM_CROP_SRC | NVBUFSURF_TRANSFORM_CROP_DST;
    return transform;
}

inline NvBufSurface* getNewColorTypeSurface(NvBufSurface* ip_surf, NvBufSurfaceColorFormat color_type){
    auto new_surf = createSurfaceBasedOn(ip_surf, color_type, ip_surf->memType);
    auto rect = getTransformRect(ip_surf);
    auto transform = makeTransformParams(rect, rect);
    NvBufSurfTransform(ip_surf, new_surf, &transform);
    return new_surf;
}

inline NvBufSurface* getSystemMemorySurface(NvBufSurface* ip_surf){
    NvBufSurface* new_surf = createSurfaceBasedOn(ip_surf, ip_surf->surfaceList[0].colorFormat, NVBUF_MEM_SYSTEM);
    int result = NvBufSurfaceCopy(ip_surf, new_surf);
    return new_surf;
}
#endif