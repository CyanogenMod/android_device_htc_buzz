/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <linux/msm_hw3d.h>
#include <linux/android_pmem.h>

#include "gralloc_priv.h"
#include "allocator.h"

/*****************************************************************************/

// NOTE: must be the same than in oem.h
#define ALLOCATORREGION_RESERVED_SIZE           (1200<<10)
#define FB_ARENA                                HW3D_EBI



static SimpleBestFitAllocator sAllocator;
static SimpleBestFitAllocator sAllocatorGPU(ALLOCATORREGION_RESERVED_SIZE);

/*****************************************************************************/

struct gralloc_context_t {
    alloc_device_t  device;
    /* our private data here */
};

static int gralloc_alloc_buffer(alloc_device_t* dev,
        size_t size, int usage, buffer_handle_t* pHandle);

/*****************************************************************************/

int fb_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

static int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

extern int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        void** vaddr);

extern int gralloc_unlock(gralloc_module_t const* module, 
        buffer_handle_t handle);

extern int gralloc_register_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

extern int gralloc_unregister_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

extern int gralloc_perform(struct gralloc_module_t const* module,
        int operation, ... );

/*****************************************************************************/

static struct hw_module_methods_t gralloc_module_methods = {
        open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            version_major: 1,
            version_minor: 0,
            id: GRALLOC_HARDWARE_MODULE_ID,
            name: "Graphics Memory Allocator Module",
            author: "The Android Open Source Project",
            methods: &gralloc_module_methods
        },
        registerBuffer: gralloc_register_buffer,
        unregisterBuffer: gralloc_unregister_buffer,
        lock: gralloc_lock,
        unlock: gralloc_unlock,
        perform: gralloc_perform,
    },
    framebuffer: 0,
    flags: 0,
    numBuffers: 0,
    bufferMask: 0,
    lock: PTHREAD_MUTEX_INITIALIZER,
    currentBuffer: 0,
    pmem_master: -1,
    pmem_master_base: 0,
    master_phys: 0,
    gpu: -1,
    gpu_base: 0,
    fb_map_offset: 0
};

/*****************************************************************************/

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev,
        size_t size, int usage, buffer_handle_t* pHandle)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);

    // allocate the framebuffer
    if (m->framebuffer == NULL) {
        // initialize the framebuffer, the framebuffer is mapped once
        // and forever.
        int err = mapFrameBufferLocked(m);
        if (err < 0) {
            return err;
        }
    }

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    const size_t bufferSize = m->finfo.line_length * m->info.yres;
    if (numBuffers == 1) {
        // If we have only one buffer, we never use page-flipping. Instead,
        // we return a regular buffer which will be memcpy'ed to the main
        // screen when post is called.
        int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;
        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle);
    }

    if (bufferMask >= ((1LU<<numBuffers)-1)) {
        // We ran out of buffers.
        return -ENOMEM;
    }

    // create a "fake" handles for it
    intptr_t vaddr = intptr_t(m->framebuffer->base);
    private_handle_t* hnd = new private_handle_t(dup(m->framebuffer->fd), size,
            private_handle_t::PRIV_FLAGS_USES_PMEM |
            private_handle_t::PRIV_FLAGS_FRAMEBUFFER);

    // find a free slot
    for (uint32_t i=0 ; i<numBuffers ; i++) {
        if ((bufferMask & (1LU<<i)) == 0) {
            m->bufferMask |= (1LU<<i);
            break;
        }
        vaddr += bufferSize;
    }
    
    hnd->base = vaddr;
    hnd->offset = vaddr - intptr_t(m->framebuffer->base);
    *pHandle = hnd;

    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev,
        size_t size, int usage, buffer_handle_t* pHandle)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle);
    pthread_mutex_unlock(&m->lock);
    return err;
}


static int init_pmem_area_locked(private_module_t* m)
{
    int err = 0;
    int master_fd = open("/dev/pmem", O_RDWR, 0);
    if (master_fd >= 0) {
        
        size_t size;
        pmem_region region;
        if (ioctl(master_fd, PMEM_GET_TOTAL_SIZE, &region) < 0) {
            LOGE("PMEM_GET_TOTAL_SIZE failed, limp mode");
            size = 8<<20;   // 8 MiB
        } else {
            size = region.len;
        }
        sAllocator.setSize(size);

        void* base = mmap(0, size, 
                PROT_READ|PROT_WRITE, MAP_SHARED, master_fd, 0);
        if (base == MAP_FAILED) {
            err = -errno;
            base = 0;
            close(master_fd);
            master_fd = -1;
        }
        m->pmem_master = master_fd;
        m->pmem_master_base = base;
    } else {
        err = -errno;
    }
    return err;
}

static int init_pmem_area(private_module_t* m)
{
    pthread_mutex_lock(&m->lock);
    int err = m->pmem_master;
    if (err == -1) {
        // first time, try to initialize pmem
        err = init_pmem_area_locked(m);
        if (err) {
            m->pmem_master = err;
        }
    } else if (err < 0) {
        // pmem couldn't be initialized, never use it
    } else {
        // pmem OK
        err = 0;
    }
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int init_gpu_area_locked(private_module_t* m)
{
    int err = 0;
    int gpu = open("/dev/msm_hw3dm", O_RDWR, 0);
    LOGE_IF(gpu<0, "could not open hw3dm (%s)", strerror(errno));
    if (gpu >= 0) {
        struct hw3d_region regions[HW3D_NUM_REGIONS];
        if (ioctl(gpu, HW3D_GET_REGIONS, regions) < 0) {
            LOGE("HW3D_GET_REGIONS failed (%s)", strerror(errno));
            err = -errno;
        } else {
            LOGD("smi: offset=%08lx, len=%08lx, phys=%p", 
                    regions[HW3D_SMI].map_offset, 
                    regions[HW3D_SMI].len, 
                    regions[HW3D_SMI].phys);
            LOGD("ebi: offset=%08lx, len=%08lx, phys=%p", 
                    regions[HW3D_EBI].map_offset,
                    regions[HW3D_EBI].len,
                    regions[HW3D_EBI].phys);
            LOGD("reg: offset=%08lx, len=%08lx, phys=%p", 
                    regions[HW3D_REGS].map_offset,
                    regions[HW3D_REGS].len,
                    regions[HW3D_REGS].phys);

            void* base = mmap(0, ALLOCATORREGION_RESERVED_SIZE,
                    PROT_READ|PROT_WRITE, MAP_SHARED,
                    gpu, regions[FB_ARENA].map_offset);

            if (base == MAP_FAILED) {
                LOGE("mmap EBI1 (%s)", strerror(errno));
                err = -errno;
                base = 0;
                close(gpu);
                gpu = -1;
            }

            m->fb_map_offset = regions[FB_ARENA].map_offset;
            m->gpu = gpu;
            m->gpu_base = base;
        }
    } else {
        err = -errno;
        m->gpu = 0;
        m->gpu_base = 0;
    }
    return err;
}

static int init_gpu_area(private_module_t* m)
{
    pthread_mutex_lock(&m->lock);
    int err = m->gpu;
    if (err == -1) {
        // first time, try to initialize gpu
        err = init_gpu_area_locked(m);
        if (err) {
            m->gpu = err;
        }
    } else if (err < 0) {
        // gpu couldn't be initialized, never use it
    } else {
        // gpu OK
        err = 0;
    }
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int gralloc_alloc_buffer(alloc_device_t* dev,
        size_t size, int usage, buffer_handle_t* pHandle)
{
    int err = 0;
    int flags = 0;

    int fd = -1;
    int gpu_fd = -1;
    void* base = 0;
    int offset = 0;

    size = roundUpToPageSize(size);
    
    if (usage & GRALLOC_USAGE_HW_TEXTURE) {
        // enable pmem in that case, so our software GL can fallback to
        // the copybit module.
        flags |= private_handle_t::PRIV_FLAGS_USES_PMEM;
    }
    
    if (usage & GRALLOC_USAGE_HW_2D) {
        flags |= private_handle_t::PRIV_FLAGS_USES_PMEM;
    }

    if ((flags & private_handle_t::PRIV_FLAGS_USES_PMEM) == 0) {
try_ashmem:
        fd = ashmem_create_region("gralloc-buffer", size);
        if (fd < 0) {
            LOGE("couldn't create ashmem (%s)", strerror(errno));
            err = -errno;
        }
    } else if ((usage & GRALLOC_USAGE_HW_RENDER) == 0) {
        private_module_t* m = reinterpret_cast<private_module_t*>(
                dev->common.module);

        err = init_pmem_area(m);
        if (err == 0) {
            // PMEM buffers are always mmapped
            base = m->pmem_master_base;
            offset = sAllocator.allocate(size);
            if (offset < 0) {
                // no more pmem memory
                err = -ENOMEM;
            } else {
                struct pmem_region sub = { offset, size };
                
                // now create the "sub-heap"
                fd = open("/dev/pmem", O_RDWR, 0);
                err = fd < 0 ? fd : 0;
                
                // and connect to it
                if (err == 0)
                    err = ioctl(fd, PMEM_CONNECT, m->pmem_master);

                // and make it available to the client process
                if (err == 0)
                    err = ioctl(fd, PMEM_MAP, &sub);

                if (err < 0) {
                    err = -errno;
                    close(fd);
                    sAllocator.deallocate(offset);
                    fd = -1;
                }
                memset((char*)base + offset, 0, size);
                //LOGD_IF(!err, "allocating pmem size=%d, offset=%d", size, offset);
            }
        } else {
            if ((usage & GRALLOC_USAGE_HW_2D) == 0) {
                // the caller didn't request PMEM, so we can try something else
                flags &= ~private_handle_t::PRIV_FLAGS_USES_PMEM;
                err = 0;
                goto try_ashmem;
            } else {
                LOGE("couldn't open pmem (%s)", strerror(errno));
            }
        }
    } else {
        // looks like we want 3D...
        flags &= ~private_handle_t::PRIV_FLAGS_USES_PMEM;
        flags |= private_handle_t::PRIV_FLAGS_USES_GPU;

        private_module_t* m = reinterpret_cast<private_module_t*>(
                dev->common.module);

        err = init_gpu_area(m);
        if (err == 0) {
            // GPU buffers are always mmapped
            base = m->gpu_base;

            // When a process holding GPU surfaces gets killed, it may take
            // up to a few seconds until SurfaceFlinger is notified and can
            // release the memory. So it's useful to wait a little bit here.
            long sleeptime = 0;
            int retry = 8; // roughly 5 seconds
            do {
                offset = sAllocatorGPU.allocate(size);
                if (offset < 0) {
                    // no more pmem memory
                    LOGW("%d KiB allocation failed in GPU memory, retrying...",
                            size/1024);
                    err = -ENOMEM;
                    sleeptime += 250000;
                    usleep(sleeptime);
                } else {
                    LOGD("allocating GPU size=%d, offset=%d", size, offset);
                    fd = open("/dev/null", O_RDONLY); // just so marshalling doesn't fail
                    gpu_fd = m->gpu;
                    memset((char*)base + offset, 0, size);
                    err = 0;
                }
            } while ((err == -ENOMEM) && (retry-- > 0));

        } else {
            // not enough memory, try ashmem
            flags &= ~private_handle_t::PRIV_FLAGS_USES_GPU;
            err = 0;
            goto try_ashmem;
        }
    }

    if (err == 0) {
        private_handle_t* hnd = new private_handle_t(fd, size, flags);
        if (base == NULL) {
            gralloc_module_t* module = reinterpret_cast<gralloc_module_t*>(
                    dev->common.module);
            err = mapBuffer(module, hnd);
            if (err == 0) {
                *pHandle = hnd;
            }
        } else {
            private_module_t* m = reinterpret_cast<private_module_t*>(
                    dev->common.module);
            hnd->offset = offset;
            hnd->base = int(base)+offset;
            hnd->gpu_fd = gpu_fd;
            hnd->map_offset = m->fb_map_offset;
            *pHandle = hnd;
        }
    }
    
    LOGE_IF(err, "gralloc failed err=%s", strerror(-err));
    
    return err;
}

/*****************************************************************************/

static int gralloc_alloc(alloc_device_t* dev,
        int w, int h, int format, int usage,
        buffer_handle_t* pHandle, int* pStride)
{
    if (!pHandle || !pStride)
        return -EINVAL;

    size_t size, stride;

    int bpp = 0;
    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            bpp = 3;
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_RGBA_5551:
        case HAL_PIXEL_FORMAT_RGBA_4444:
            bpp = 2;
            break;
        default:
            return -EINVAL;
    }

    if (usage & GRALLOC_USAGE_HW_RENDER) {
        /* buffers MUST be aligned to the NEXT 8 pixels multiple any other
         * alignments will fail do to assumptions in the driver */
        const int pixelAlignment = 8;
        const int mask = pixelAlignment - 1;
        stride = (w + mask) & ~mask;
        size = stride * h * bpp;
    } else {
        const int align = 4;
        size_t bpr = (w*bpp + (align-1)) & ~(align-1);
        size = bpr * h;
        stride = bpr / bpp;
    }

    int err;
    if (usage & GRALLOC_USAGE_HW_FB) {
        err = gralloc_alloc_framebuffer(dev, size, usage, pHandle);
    } else {
        err = gralloc_alloc_buffer(dev, size, usage, pHandle);
    }

    if (err < 0) {
        return err;
    }

    *pStride = stride;
    return 0;
}

static int gralloc_free(alloc_device_t* dev,
        buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        // free this buffer
        private_module_t* m = reinterpret_cast<private_module_t*>(
                dev->common.module);
        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;
        m->bufferMask &= ~(1<<index); 
    } else { 
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM) {
            if (hnd->fd >= 0) {
                struct pmem_region sub = { hnd->offset, hnd->size };
                int err = ioctl(hnd->fd, PMEM_UNMAP, &sub);
                LOGE_IF(err<0, "PMEM_UNMAP failed (%s), "
                        "fd=%d, sub.offset=%lu, sub.size=%lu",
                        strerror(errno), hnd->fd, hnd->offset, hnd->size);
                if (err == 0) {
                    // we can't deallocate the memory in case of UNMAP failure
                    // because it would give that process access to someone else's
                    // surfaces, which would be a security breach.
                    sAllocator.deallocate(hnd->offset);
                }
            }
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_GPU) {
            LOGD("freeing GPU buffer at %d", hnd->offset);
            sAllocatorGPU.deallocate(hnd->offset);
        }

        gralloc_module_t* module = reinterpret_cast<gralloc_module_t*>(
                dev->common.module);
        terminateBuffer(module, const_cast<private_handle_t*>(hnd));
    }

    close(hnd->fd);
    delete hnd;
    return 0;
}

/*****************************************************************************/

static int gralloc_close(struct hw_device_t *dev)
{
    gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
    if (ctx) {
        /* TODO: keep a list of all buffer_handle_t created, and free them
         * all here.
         */
        free(ctx);
    }
    return 0;
}

int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
        gralloc_context_t *dev;
        dev = (gralloc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = gralloc_close;

        dev->device.alloc   = gralloc_alloc;
        dev->device.free    = gralloc_free;

        *device = &dev->device.common;
        status = 0;
    } else {
        status = fb_device_open(module, name, device);
    }
    return status;
}
