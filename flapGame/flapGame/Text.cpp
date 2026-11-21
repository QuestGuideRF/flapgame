#include <flapGame/Core.h>
#include <flapGame/Text.h>
#include <flapGame/VertexFormats.h>
#define STBI_MALLOC(sz)         PLY_HEAP.alloc(sz)
#define STBI_REALLOC(p, newsz)  PLY_HEAP.realloc(p, newsz)
#define STBI_FREE(p)            PLY_HEAP.free(p)
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
namespace flap {
void copy8Bit(image::Image& dst, const image::Image& src) {
    PLY_ASSERT(dst.bytespp == 1);
    PLY_ASSERT(src.bytespp == 1);
    PLY_ASSERT(sameDims(dst, src));
    for (s32 y = 0; y < dst.height; y++) {
        u8* d = (u8*) dst.getPixel(0, y);
        u8* dEnd = d + dst.width;
        u8* s = (u8*) src.getPixel(0, y);
        while (d < dEnd) {
            *d = *s;
            d++;
            s++;
        }
    }
}
PLY_NO_INLINE Owned<SDFCommon> SDFCommon::create() {
    Owned<SDFCommon> common = new SDFCommon;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "в vec2 vertPosition;\n"
                              "в vec2 vertTexCoord;\n"
                              "out vec2 fragTexCoord\n;"
                              "униформа mat4 modelToViewport;\n"
                              "\n"
                              "void main() {\n"
                              "    fragTexCoord = хостTexCoord;\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 0.0, 1.0);\n"
                              "}\n");
        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER,
            "в vec2 fragTexCoord;\n"
            "униформа sampler2D texImage;\n"
            "униформа vec2 sdfParams;\n"
            "однородный цвет vec4;\n"
            "out vec4 fragColor;\n"
            "\n"
            "void main() {\n"
            "    vec4 c = текстура(texImage, fragTexCoord);\n"
            "    float v = зажим(0.5 - (sdfParams.x - c.r) * sdfParams.y, 0.0, 1.0);\n"
            "    fragColor = vec4(color.rgb * v * color.a, 1.0 - v * color.a);\n"
            "}\n");
        common->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }
    common->positionAttrib = GL_NO_CHECK(GetAttribLocation(common->shader.id, "vertPosition"));
    PLY_ASSERT(common->positionAttrib >= 0);
    common->texCoordAttrib = GL_NO_CHECK(GetAttribLocation(common->shader.id, "vertTexCoord"));
    PLY_ASSERT(common->texCoordAttrib >= 0);
    common->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(common->shader.id, "modelToViewport"));
    PLY_ASSERT(common->modelToViewportUniform >= 0);
    common->textureUniform = GL_NO_CHECK(GetUniformLocation(common->shader.id, "texImage"));
    PLY_ASSERT(common->textureUniform >= 0);
    common->sdfParamsUniform = GL_NO_CHECK(GetUniformLocation(common->shader.id, "sdfParams"));
    PLY_ASSERT(common->sdfParamsUniform >= 0);
    common->colorUniform = GL_NO_CHECK(GetUniformLocation(common->shader.id, "color"));
    PLY_ASSERT(common->colorUniform >= 0);
    return common;
}
PLY_NO_INLINE Owned<SDFOutline> SDFOutline::create() {
    Owned<SDFOutline> outline = new SDFOutline;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "в vec2 vertPosition;\n"
                              "в vec2 vertTexCoord;\n"
                              "out vec2 fragTexCoord\n;"
                              "униформа mat4 modelToViewport;\n"
                              "\n"
                              "void main() {\n"
                              "    fragTexCoord = хостTexCoord;\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 0.0, 1.0);\n"
                              "}\n");
        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER,
            "в vec2 fragTexCoord;\n"
            "униформа sampler2D texImage;\n"
            "однородные цвета vec4[3];\n"
            "однородный vec2 centerSlope[2];\n"
            "универсальный поплавковый сепаратор;\n"
            "out vec4 fragColor;\n"
            "\n"
            "void main() {\n"
            "    float c = текстура(texImage, fragTexCoord).r;\n"
            "    int i = int(c > разделитель);\n"
            "    float f = зажим((c - centerSlope[i].x) * centerSlope[i].y, 0.0, 1.0);\n"
            "    fragColor = mix(colors[i], Colors[i + 1], f);\n"
            "}\n");
        outline->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }
    outline->positionAttrib = GL_NO_CHECK(GetAttribLocation(outline->shader.id, "vertPosition"));
    PLY_ASSERT(outline->positionAttrib >= 0);
    outline->texCoordAttrib = GL_NO_CHECK(GetAttribLocation(outline->shader.id, "vertTexCoord"));
    PLY_ASSERT(outline->texCoordAttrib >= 0);
    outline->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(outline->shader.id, "modelToViewport"));
    PLY_ASSERT(outline->modelToViewportUniform >= 0);
    outline->textureUniform = GL_NO_CHECK(GetUniformLocation(outline->shader.id, "texImage"));
    PLY_ASSERT(outline->textureUniform >= 0);
    outline->colorsUniform = GL_NO_CHECK(GetUniformLocation(outline->shader.id, "colors"));
    PLY_ASSERT(outline->colorsUniform >= 0);
    outline->centerSlopeUniform =
        GL_NO_CHECK(GetUniformLocation(outline->shader.id, "centerSlope"));
    PLY_ASSERT(outline->centerSlopeUniform >= 0);
    outline->separatorUniform = GL_NO_CHECK(GetUniformLocation(outline->shader.id, "separator"));
    PLY_ASSERT(outline->separatorUniform >= 0);
    return outline;
}
PLY_NO_INLINE Owned<SDFFont> SDFFont::bake(StringView ttfBuffer, float pixelHeight) {
    Owned<SDFFont> sdfFont = new SDFFont;
    s32 padding = 8;
    u8 onEdgeValue = 192;
    float pixelDistScale = 16.f;
    image::OwnImage atlasIm{512, 512, image::Format::Byte};
    memset(atlasIm.data, 0, atlasIm.size());
    sdfFont->chars.reserve(96);
    stbtt_fontinfo stbFont;
    stbFont.userdata = nullptr;
    int rc = stbtt_InitFont(&stbFont, (const unsigned char*) ttfBuffer.bytes, 0);
    PLY_ASSERT(rc);
    PLY_UNUSED(rc);
    float scale = stbtt_ScaleForPixelHeight(&stbFont, pixelHeight);
    Array<image::Image> glyphImages;
    Array<stbrp_rect> rectsToPack;
    glyphImages.reserve(96);
    for (u32 i = 0; i < 96; i++) {
        int codePoint = i + 32;
        if (codePoint == 64) {
            codePoint = 0xa9;
        }
        s32 g = stbtt_FindGlyphIndex(&stbFont, codePoint);
        IntVec2 glyphOffset = {0, 0};
        IntVec2 glyphSize = {0, 0};
        image::Image& glyphIm = glyphImages.append(nullptr, 0, 0, 0, image::Format::Byte);
        glyphIm.data =
            (char*) stbtt_GetGlyphSDF(&stbFont, scale, g, padding, onEdgeValue, pixelDistScale,
                                      &glyphSize.x, &glyphSize.y, &glyphOffset.x, &glyphOffset.y);
        glyphIm.width = glyphSize.x;
        glyphIm.height = glyphSize.y;
        glyphIm.stride = glyphSize.x;
        stbrp_rect& rtp = rectsToPack.append();
        rtp.id = 0;
        rtp.w = safeDemote<stbrp_coord>(glyphIm.width);
        rtp.h = safeDemote<stbrp_coord>(glyphIm.height);
        rtp.x = 0;
        rtp.y = 0;
        rtp.was_packed = 0;
        SDFFont::Char& cd = sdfFont->chars.append();
        s32 rawXAdvance = 0;
        stbtt_GetGlyphHMetrics(&stbFont, g, &rawXAdvance, nullptr);
        cd.offset = glyphOffset.to<Int2<s16>>();
        cd.xAdvance = rawXAdvance * scale;
    }
    {
        Array<stbrp_node> packNodes;
        packNodes.resize(atlasIm.width);
        stbrp_context packContext;
        stbrp_init_target(&packContext, atlasIm.width, atlasIm.height, packNodes.get(),
                          packNodes.numItems());
        stbrp_pack_rects(&packContext, rectsToPack.get(), rectsToPack.numItems());
    }
    for (u32 i = 0; i < rectsToPack.numItems(); i++) {
        stbrp_rect& sr = rectsToPack[i];
        PLY_ASSERT(sr.was_packed);
        IntRect dstRect = IntRect::fromSize({sr.x, sr.y}, {sr.w, sr.h});
        PLY_ASSERT(atlasIm.getRect().contains(dstRect));
        image::Image dstIm = image::crop(atlasIm, dstRect);
        copy8Bit(dstIm, glyphImages[i]);
        sdfFont->chars[i].atlasCoords = dstRect.to<Box<Int2<s16>>>();
    }
    for (image::Image& glyphIm : glyphImages) {
        stbtt_FreeSDF((unsigned char*) glyphIm.data, stbFont.userdata);
    }
    sdfFont->fontTexture.init(atlasIm);
    return sdfFont;
}
PLY_NO_INLINE TextBuffers generateTextBuffers(const SDFFont* sdfFont, StringView text) {
    TextBuffers tb;
    Float2 pos = {0, 0};
    Float2 ooAtlasSize = 1.f / Float2{512, 512};
    Array<u16> indices;
    Array<VertexP2T> vertices;
    for (u32 i = 0; i < text.numBytes; i++) {
        u32 code = text[i] - 32;
        const SDFFont::Char& cd = sdfFont->chars[code];
        Rect uv = cd.atlasCoords.to<Rect>() * ooAtlasSize;
        Rect r = Rect::fromSize(cd.offset.to<Float2>(), cd.atlasCoords.size().to<Float2>()) + pos;
        tb.xMin = min(tb.xMin, r.mins.x);
        tb.xMax = max(tb.xMax, r.maxs.x);
        pos.x += cd.xAdvance;
        u32 base = vertices.numItems();
        vertices.append() = {{r.mins.x, -r.maxs.y}, {uv.mins.x, uv.maxs.y}};
        vertices.append() = {{r.maxs.x, -r.maxs.y}, {uv.maxs.x, uv.maxs.y}};
        vertices.append() = {{r.maxs.x, -r.mins.y}, {uv.maxs.x, uv.mins.y}};
        vertices.append() = {{r.mins.x, -r.mins.y}, {uv.mins.x, uv.mins.y}};
        indices.append(safeDemote<u16>(base));
        indices.append(safeDemote<u16>(base + 1));
        indices.append(safeDemote<u16>(base + 2));
        indices.append(safeDemote<u16>(base + 2));
        indices.append(safeDemote<u16>(base + 3));
        indices.append(safeDemote<u16>(base));
    }
    tb.indices = DynamicArrayBuffers::instance->upload(indices.stringView());
    tb.vbo = DynamicArrayBuffers::instance->upload(vertices.stringView());
    tb.numIndices = indices.numItems();
    if (tb.xMin == Limits<float>::Max) {
        tb.xMin = 0;
        tb.xMax = 0;
    }
    return tb;
}
PLY_NO_INLINE void drawText(const SDFCommon* common, const SDFFont* sdfFont, const TextBuffers& tb,
                            const Float4x4& modelToViewport, const Float2& sdfParams,
                            const Float4& color, bool alphaOnly) {
    GL_CHECK(UseProgram(common->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    if (alphaOnly) {
        GL_CHECK(BlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_SRC_ALPHA));
    } else {
        GL_CHECK(BlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_ONE));
    }
    GL_CHECK(
        UniformMatrix4fv(common->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, sdfFont->fontTexture.id));
    GL_CHECK(Uniform1i(common->textureUniform, 0));
    GL_CHECK(Uniform2fv(common->sdfParamsUniform, 1, (const GLfloat*) &sdfParams));
    GL_CHECK(Uniform4fv(common->colorUniform, 1, (const GLfloat*) &color));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, tb.vbo));
    GL_CHECK(EnableVertexAttribArray(common->positionAttrib));
    GL_CHECK(VertexAttribPointer(common->positionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexP2T), (GLvoid*) offsetof(VertexP2T, pos)));
    GL_CHECK(EnableVertexAttribArray(common->texCoordAttrib));
    GL_CHECK(VertexAttribPointer(common->texCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexP2T), (GLvoid*) offsetof(VertexP2T, uv)));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, tb.indices));
    GL_CHECK(DrawElements(GL_TRIANGLES, (GLsizei) tb.numIndices, GL_UNSIGNED_SHORT, (void*) 0));
    GL_CHECK(DisableVertexAttribArray(common->texCoordAttrib));
    GL_CHECK(DisableVertexAttribArray(common->positionAttrib));
}
PLY_NO_INLINE void drawOutlinedText(const SDFOutline* outline, const SDFFont* sdfFont,
                                    const TextBuffers& tb, const Float4x4& modelToViewport,
                                    const Float4& fillColor, const Float4& outlineColor,
                                    ArrayView<const Float2> centerSlope) {
    GL_CHECK(UseProgram(outline->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA));
    GL_CHECK(UniformMatrix4fv(outline->modelToViewportUniform, 1, GL_FALSE,
                              (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, sdfFont->fontTexture.id));
    GL_CHECK(Uniform1i(outline->textureUniform, 0));
    Float4 colors[3] = {{0, 0, 0, 1}, outlineColor, fillColor};
    GL_CHECK(Uniform4fv(outline->colorsUniform, 3, (const GLfloat*) colors));
    PLY_ASSERT(centerSlope.numItems == 2);
    GL_CHECK(Uniform2fv(outline->centerSlopeUniform, 2, (const GLfloat*) centerSlope.items));
    GL_CHECK(Uniform1f(outline->separatorUniform, mix(centerSlope[0].x, centerSlope[1].x, 0.5f)));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, tb.vbo));
    GL_CHECK(EnableVertexAttribArray(outline->positionAttrib));
    GL_CHECK(VertexAttribPointer(outline->positionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexP2T), (GLvoid*) offsetof(VertexP2T, pos)));
    GL_CHECK(EnableVertexAttribArray(outline->texCoordAttrib));
    GL_CHECK(VertexAttribPointer(outline->texCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexP2T), (GLvoid*) offsetof(VertexP2T, uv)));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, tb.indices));
    GL_CHECK(DrawElements(GL_TRIANGLES, (GLsizei) tb.numIndices, GL_UNSIGNED_SHORT, (void*) 0));
    GL_CHECK(DisableVertexAttribArray(outline->texCoordAttrib));
    GL_CHECK(DisableVertexAttribArray(outline->positionAttrib));
}
}