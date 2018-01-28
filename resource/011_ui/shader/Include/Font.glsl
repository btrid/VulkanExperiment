#ifndef BTR_FONT_GLSL_
#define BTR_FONT_GLSL_

struct GlyphInfo
{
	uint m_char_index;
	uint m_cache_index;
};

struct GlyphTextureInfo
{
	uvec2 m_glyph_size;
	uvec2 m_glyph_tex_size;
};

#ifdef USE_FONT_TEXTURE
layout(set=USE_FONT_TEXTURE, binding=0) uniform sampler2D tGlyph;
layout(set=USE_FONT_TEXTURE, binding=1, std140) buffer GlyphTextureInfoUniform
{
	GlyphTextureInfo u_glyph_texture_info;
};

#endif

#ifdef USE_FONT_MAP
layout(set=USE_FONT_MAP, binding=0, std430) buffer GlyphMapBuffer
{
	GlyphInfo b_glyph_map[];
};
#endif
#endif