// Based on https://github.com/floooh/sokol-samples/blob/master/html5/mipmap-emsc.c
//------------------------------------------------------------------------------
//  mipmap-glfw.c
//  Test mipmapping behaviour.
//  Top row: NEAREST_MIPMAP_NEAREST to LINEAR_MIPMAP_LINEAR
//  Middle row: the same with the LOD clamped to 2..4
//  Bottom row: anistropy levels 2, 4, 8 and 16
//
//  The original sample draws tilted checkerboard quads. How those look depends
//  on how each renderer approximates the level of detail (LOD) and anisotropic
//  filtering, so its output cannot be compared against one reference image.
//  This version draws an image that is the same in every renderer: each mip
//  level has one solid color, and each texture is drawn as pixel-aligned strips
//  with a texel density that gives a known LOD.
//------------------------------------------------------------------------------
#include <GLES3/gl3.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "emsc.h"

typedef struct {
    hmm_mat4 mvp;
    hmm_vec2 uv_scale;
} vs_params_t;

static struct {
    uint32_t mip0[65536];   /* 256x256 */
    uint32_t mip1[16384];   /* 128x128 */
    uint32_t mip2[4096];    /* 64*64 */
    uint32_t mip3[1024];    /* 32*32 */
    uint32_t mip4[256];     /* 16*16 */
    uint32_t mip5[64];      /* 8*8 */
    uint32_t mip6[16];      /* 4*4 */
    uint32_t mip7[4];       /* 2*2 */
    uint32_t mip8[1];       /* 1*2 */
} pixels;

static uint32_t mip_colors[9] = {
    0xFF0000FF,     /* red */
    0xFF00FF00,     /* green */
    0xFFFF0000,     /* blue */
    0xFFFF00FF,     /* magenta */
    0xFFFFFF00,     /* cyan */
    0xFF00FFFF,     /* yellow */
    0xFFFF00A0,     /* violet */
    0xFFFFA0FF,     /* orange */
    0xFFA000FF,     /* purple */
};

static sg_pipeline pip;
static sg_bindings bind;
static sg_image img[12];

static void draw();

int main() {
    /* try to setup WebGL2 context (for the mipmap min/max lod stuff) */
    emsc_init("#canvas", EMSC_TRY_WEBGL2);

    /* setup sokol_gfx */
    sg_desc desc = {
        .context.gl.force_gles2 = emsc_webgl_fallback()
    };
    sg_setup(&desc);

    /* a plane vertex buffer */
    float vertices[] = {
        -1.0, -1.0, 0.0,  0.0, 0.0,
        +1.0, -1.0, 0.0,  1.0, 0.0,
        -1.0, +1.0, 0.0,  0.0, 1.0,
        +1.0, +1.0, 0.0,  1.0, 1.0,
    };
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });

    /* initialize mipmap content, a different solid color for each mip level */
    sg_image_content img_content;
    uint32_t* ptr = pixels.mip0;
    for (int mip_index = 0; mip_index <= 8; mip_index++) {
        const int dim = 1<<(8-mip_index);
        img_content.subimage[0][mip_index].ptr = ptr;
        img_content.subimage[0][mip_index].size = dim * dim * 4;
        for (int y = 0; y < dim; y++) {
            for (int x = 0; x < dim; x++) {
                *ptr++ = mip_colors[mip_index];
            }
        }
    }
    /* the first 4 images are just different min-filters, the last
       4 images are different anistropy levels */
    sg_image_desc img_desc = {
        .width = 256,
        .height = 256,
        .num_mipmaps = 9,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .mag_filter = SG_FILTER_LINEAR,
        .content = img_content
    };
    sg_filter min_filter[] = {
        SG_FILTER_NEAREST_MIPMAP_NEAREST,
        SG_FILTER_LINEAR_MIPMAP_NEAREST,
        SG_FILTER_NEAREST_MIPMAP_LINEAR,
        SG_FILTER_LINEAR_MIPMAP_LINEAR,
    };
    for (int i = 0; i < 4; i++) {
        img_desc.min_filter = min_filter[i];
        img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 2.0f;
    img_desc.max_lod = 4.0f;
    for (int i = 4; i < 8; i++) {
        img_desc.min_filter = min_filter[i-4];
        img[i] = sg_make_image(&img_desc);
    }
    img_desc.min_lod = 0.0f;
    img_desc.max_lod = 0.0f;    /* for max_lod, zero-initialized means "FLT_MAX" */
    for (int i = 8; i < 12; i++) {
        img_desc.max_anisotropy = 1<<(i-7);
        img[i] = sg_make_image(&img_desc);
    }

    /* shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].name = "position",
            [1].name = "texcoord0"
        },
        .vs = {
            .uniform_blocks[0] = {
                .size = sizeof(vs_params_t),
                .uniforms = {
                    [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 },
                    [1] = { .name="uv_scale", .type=SG_UNIFORMTYPE_FLOAT2 }
                }
            },
            .source =
                "uniform mat4 mvp;\n"
                "uniform vec2 uv_scale;\n"
                "attribute vec4 position;\n"
                "attribute vec2 texcoord0;\n"
                "varying vec2 uv;\n"
                "void main() {\n"
                "  gl_Position = mvp * position;\n"
                "  uv = texcoord0 * uv_scale;\n"
                "}\n"
        },
        .fs = {
            .images[0] = { .name="tex", .type = SG_IMAGETYPE_2D },
            .source =
                "precision mediump float;"
                "uniform sampler2D tex;"
                "varying vec2 uv;\n"
                "void main() {\n"
                "  gl_FragColor = texture2D(tex, uv);\n"
                "}\n"
        }
    });

    /* pipeline state */
    pip = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = {
            .attrs = {
                [0].format=SG_VERTEXFORMAT_FLOAT3,
                [1].format=SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });

    draw();
    return 0;
}

void draw() {
    /* projection matrix: canvas pixels, with the origin in the top left corner */
    hmm_mat4 proj = HMM_Orthographic(0.0f, (float)emsc_width(), (float)emsc_height(), 0.0f, -1.0f, 1.0f);

    vs_params_t vs_params;

    sg_begin_default_pass(&(sg_pass_action){0}, emsc_width(), emsc_height());
    sg_apply_pipeline(pip);
    for (int i = 0; i < 12; i++) {
        bind.fs_images[0] = img[i];
        sg_apply_bindings(&bind);
        /* each image is drawn as two bands of nine strips, one strip per mip level */
        for (int band = 0; band < 2; band++) {
            for (int strip = 0; strip < 9; strip++) {
                /* Texels per pixel along u and v. When both are 2^n, the LOD is n.
                   In the first band the LOD is the strip index. */
                float du = HMM_PowerF(2.0f, (float)strip);
                float dv = du;
                if (band == 1) {
                    if (i < 8) {
                        /* A quarter of the way from one mip level to the next:
                           the nearest-mip filters still show a single level, and
                           the linear-mip filters blend two levels 3:1. Not half
                           way, where renderers round differently, and not below
                           an LOD of 1, where they may magnify instead. */
                        du = dv = HMM_PowerF(2.0f, (float)strip + 1.25f);
                    }
                    else {
                        /* An anisotropic footprint of 1 x 2^strip texels: each
                           doubling of the anisotropy level lowers the LOD by one. */
                        du = 1.0f;
                    }
                }
                /* a strip of 8x20 pixels */
                const float w = 8.0f;
                const float h = 20.0f;
                const float x = (float)((i & 3) * 75 + 1 + strip * 8);
                const float y = (float)((i / 4) * 50 + 3 + band * 24);
                hmm_mat4 model = HMM_MultiplyMat4(
                    HMM_Translate(HMM_Vec3(x + w * 0.5f, y + h * 0.5f, 0.0f)),
                    HMM_Scale(HMM_Vec3(w * 0.5f, h * 0.5f, 1.0f)));
                vs_params.mvp = HMM_MultiplyMat4(proj, model);
                vs_params.uv_scale = HMM_Vec2(w * du / 256.0f, h * dv / 256.0f);

                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
                sg_draw(0, 4, 1);
            }
        }
    }
    sg_end_pass();
    sg_commit();
}
