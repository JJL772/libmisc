/**
----------------------------------------------------------------------
CairoText.hpp - Simple text rendering library using Cairo.

Define CAIRO_TEXT_IMPLEMENTATION before including this file in a single 
translation unit.

----------------------------------------------------------------------

----------------------------------------------------------------------
-                      LICENSE FOR THIS COMPONENT                    -
----------------------------------------------------------------------
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
----------------------------------------------------------------------
*/
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <freetype/ftglyph.h>
#include <freetype/ftsizes.h>

#include <unordered_map>
#include <string>
#include <cassert>


// Represents a cairo text rendering context
// This can be used to render strings and such to a text buffer
// It's a relatively simple implementation and lacks support for many things, like ligatures
// This is primarily intended for relatively basic screen text rendering
class CairoTextContext {
	struct Font_t {
		cairo_font_face_t* fontFace;
		std::string fontName;
		void* fileBuf;
		int sizeH;
		FT_Face face;
		int maxPixH, maxPixW;
	};
	
public:
	using HFont = void*;
	static inline constexpr HFont INVALID_FONT = nullptr;
	
	enum TextAntialias {
		AA_None,
		AA_Subpixel,
		AA_Fast,
		AA_Good,
		AA_Best,
		AA_Gray,
	};

	enum TextHintStyle {
		Hint_None,
		Hint_Slight,
		Hint_Medium,
		Hint_Full,
	};
	
	enum BufferFormat {
		RGB24,
		ARGB32,
		A8
	};

	struct DrawParams_t {
		int x; // X pos
		int y; // Y pos 
		int w; // Buffer height
		int h; // Buffer wide 
		HFont font;
		float borderWidth;
		uint8_t borderColor[4];		// RGBA
		uint8_t fontColor[4];		// RGBA
		TextAntialias antiAliasing;
		TextHintStyle hintStyle;
		BufferFormat format;
	};
	
	// Init/shutdown the renderer
	bool init();
	void shutdown();
	
	// Load a font from disk or memory, store into font list with the specified ref name
	HFont loadFont(const char* fontRefName, int pixelH, const char* ttfPath);
	HFont loadFont(const char* fontRefName, int pixelH, const void* buffer, size_t bufferLen);
	
	// Finds a loaded font by ref name
	HFont findFont(const char* fontRefName);
	
	// Unloads a font
	void unloadFont(const char* fontRefName);
	void unloadFont(HFont handle);
	
	// Draws text to a buffer with the requested draw params
	// The buffer is returned into `buffer`, but you MUST free this manually, when you're done. 
	// 
	void drawToBuffer(const char* text, void** buffer, const DrawParams_t &params);
	
	// Returns the rendered size of the text 
	void textSize(const char* text, int& w, int& h, HFont hfont);
	
	// Returns the default font, if any.
	HFont defaultFont() const { return defaultFont_; }
	
	// Returns a ref to the font list. Do not modify
	const auto& fontList() const { return fontList_; }
private:

	inline cairo_font_face_t* HFontToCairo(HFont fnt) {
		return static_cast<Font_t*>(fnt)->fontFace;
	}

	HFont defaultFont_ = INVALID_FONT;
	std::unordered_map<std::string, Font_t*> fontList_;
	FT_Library freetype_ = nullptr;
};

bool CairoTextContext::init() {
	if (FT_Init_FreeType(&freetype_) != FT_Err_Ok) {
		freetype_ = nullptr;
		return false;
	}
	return true;
}

#ifdef CAIRO_TEXT_IMPLEMENTATION

void CairoTextContext::shutdown() {
	if (!freetype_)
		return;
	FT_Done_FreeType(freetype_);
	
	for (auto pair : fontList_) {
		free(pair.second->fileBuf);
		delete pair.second;
	}
	fontList_.clear();
}

// Load a font from memory, returns a handle to it
CairoTextContext::HFont CairoTextContext::loadFont(const char* fontRefName, int pixelSizeH, const void* buffer, size_t bufferLen) {
	int size = 0;
	FT_Face face;
	FT_Error status = FT_New_Memory_Face(freetype_, static_cast<const FT_Byte*>(buffer), bufferLen, 0, &face);
	
	if (status != FT_Err_Ok)
		return INVALID_FONT;
	
	// Request the size the user wants 
	FT_Set_Pixel_Sizes(face, 0, pixelSizeH);
	
	cairo_font_face_t* cairoFont = cairo_ft_font_face_create_for_ft_face(face, 0);

	Font_t* font = new Font_t;
	font->face = face;
	font->fontFace = cairoFont;
	font->fontName = FT_Get_Postscript_Name(face);
	font->sizeH = pixelSizeH;
	
	// Duplicate the buffer, needs to be kept around for the lifetime of the font
	font->fileBuf = malloc(bufferLen);
	memcpy(font->fileBuf, buffer, bufferLen);
	
	// This little snippet is very important! fontBuffer cannot be freed until the font is gone so we keep a ref of it in the font struct
	static cairo_user_data_key_t key;
	auto cairoStat = cairo_font_face_set_user_data(cairoFont, &key, font, (cairo_destroy_func_t)[](void* data) {
		auto* info = static_cast<Font_t*>(data);
		if(info->fileBuf)
			free(info->fileBuf);
		FT_Done_Face(info->face);
		delete info;
	});

	
	// Check error again
	if (!cairoFont || cairoStat != CAIRO_STATUS_SUCCESS) {
		cairo_font_face_destroy(cairoFont); // I hope to GOD this doesn't segfault 
		FT_Done_Face(face);
		
		if (font->fileBuf)
			free(font->fileBuf);
		
		delete font;
		return nullptr;
	}
	
	// Compute pixel sizes now. used for sizing buffers 
	FT_Pos gW = 0, gH = 0;
	for (int c = 0; c < 128; c++) {
		auto glyphIndex = FT_Get_Char_Index(face, c);
		
		auto err = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
		if(err)
			continue;
		
		auto realGH = face->glyph->bitmap.rows;
		auto realGW = face->glyph->bitmap.width;
		
		gW = gW > realGW ? gW : realGW;
		gH = gH > realGH ? gH : realGH;
	}
	font->maxPixH = gH;
	font->maxPixW = gW;
	
	if(!defaultFont_)
		defaultFont_ = font;
	
	fontList_.insert({fontRefName, font});
	return font;
}


void CairoTextContext::unloadFont(const char* fontRefName) {
	if (auto hFont = findFont(fontRefName))
		unloadFont(hFont);
}

void CairoTextContext::unloadFont(HFont handle) {
	auto font = static_cast<Font_t*>(handle);
	cairo_font_face_destroy(font->fontFace);
	if (auto it = fontList_.find(font->fontName); it != fontList_.end())
		fontList_.erase(it);
	delete font;
}

// Finds a loaded font by ref name
CairoTextContext::HFont CairoTextContext::findFont(const char* fontRefName) {
	if (auto it = fontList_.find(fontRefName); it != fontList_.end()) {
		return it->second;
	}
	return INVALID_FONT;
}

// Draws a string to a buffer, allocating that buffer
void CairoTextContext::drawToBuffer(const char* text, void** buffer, const DrawParams_t &params) {
	auto font = static_cast<Font_t*>(params.font);
	assert(font);
	
	cairo_surface_t* surface;
	cairo_t* cairo;
	
	int w = params.w;
	int h = params.h;
	
	auto stride = cairo_format_stride_for_width(static_cast<cairo_format_t>(params.format), w);
	unsigned char* imgData = static_cast<unsigned char*>(calloc(1, stride * h));
	
	surface = cairo_image_surface_create_for_data(imgData, static_cast<cairo_format_t>(params.format), w, h, stride);
	
	cairo = cairo_create(surface);
	
	// Choose the right AA mode
	cairo_antialias_t aamode;
	switch(params.antiAliasing) {
	case TextAntialias::AA_None:
		aamode = CAIRO_ANTIALIAS_NONE;
		break;
	case TextAntialias::AA_Subpixel:
		aamode = CAIRO_ANTIALIAS_SUBPIXEL;
		break;
	case TextAntialias::AA_Fast:
		aamode = CAIRO_ANTIALIAS_FAST;
		break;
	case TextAntialias::AA_Good:
		aamode = CAIRO_ANTIALIAS_GOOD;
		break;
	case TextAntialias::AA_Best:
		aamode = CAIRO_ANTIALIAS_BEST;
		break;
	case TextAntialias::AA_Gray:
		aamode = CAIRO_ANTIALIAS_GRAY;
		break;
	default:
		assert(0);
		break;
	}
	
	// Choose the right hint mode 
	cairo_hint_style_t hintmode;
	switch(params.hintStyle) {
	case TextHintStyle::Hint_Full: 
		hintmode = CAIRO_HINT_STYLE_FULL;
		break;
	case TextHintStyle::Hint_Medium:
		hintmode = CAIRO_HINT_STYLE_MEDIUM;
		break;
	case TextHintStyle::Hint_None:
		hintmode = CAIRO_HINT_STYLE_NONE;
		break;
	case TextHintStyle::Hint_Slight:
		hintmode = CAIRO_HINT_STYLE_SLIGHT;
		break;
	default:
		assert(0);
		break;
	}

	// Set some options 
	cairo_font_options_t* opts = cairo_font_options_create();
	cairo_font_options_set_antialias(opts, aamode);
	cairo_font_options_set_hint_style(opts, hintmode);
	cairo_set_font_options(cairo, opts);

	const auto& color = params.fontColor;
	
	cairo_set_font_size(cairo, font->sizeH);
	cairo_set_line_width(cairo, params.borderWidth);
	cairo_set_source_rgba(cairo, ((float)color[0])/255.f, ((float)color[1])/255.f, ((float)color[2])/255.f, ((float)color[3])/255.f);
	cairo_set_font_face(cairo, HFontToCairo(font));
	
	// Get text extents 
	cairo_text_extents_t ext;
	cairo_text_extents(cairo, text, &ext);
	cairo_move_to(cairo, - ext.x_bearing, - ext.y_bearing);
	
	// Trace a path & fill (needed for the outline)
	cairo_text_path(cairo, text);
	cairo_fill_preserve(cairo);
	
	// Draw an outline if we have one
	if(params.borderWidth > 0.0) {
		cairo_set_source_rgba(cairo, ((float)params.borderColor[0])/255.f, ((float)params.borderColor[1])/255.f, ((float)params.borderColor[2])/255.f,
			((float)params.borderColor[3])/255.f);
		cairo_set_line_width(cairo, params.borderWidth);
		cairo_stroke(cairo);
	}

	cairo_surface_flush(surface);
	cairo_destroy(cairo);
	
	*buffer = imgData;
}

// Returns the rough size of the text. This is used mainly for estimating buffer sizes for allocation
void CairoTextContext::textSize(const char* text, int& w, int& h, HFont hfont) {
	auto* font = static_cast<Font_t*>(hfont);

	auto len = strlen(text);
	w = len * font->maxPixW;
	h = font->maxPixH;
}

#endif // CAIRO_TEXT_IMPLEMENTATION

#if 0 
//-------------------------------------------------------------------------------//
// Purpose: Initialize freetype for use 
//-------------------------------------------------------------------------------//
void GL3_FreeType_Init() {
	if(FT_Init_FreeType(&freetype())) {
		Log::warning("WARNING: Failed to init ft2!!\n");
	}
	
	// By default we try to open a fallback font in platform
	// if that fails, we default to /usr/share/fonts/truetype/freefont/FreeMono.ttf
	void* fontBuffer = nullptr;
	int size = 0;
	if((size = fileSystem->LoadFile("fonts/FiraCode-Medium.ttf", &fontBuffer, SEARCHPATH_GAME)) != 0) {
		FT_Face face;
		FT_New_Memory_Face(freetype(), (const FT_Byte*)fontBuffer, size, 0, &face);
		
		cairo_font_face_t* cairoFont = cairo_ft_font_face_create_for_ft_face(face, 0);
		
		static cairo_user_data_key_t key;
		auto status = cairo_font_face_set_user_data(cairoFont, &key, face, (cairo_destroy_func_t)FT_Done_Face);
		
		FontInfo_GL3_t* font = new FontInfo_GL3_t();
		font->fontFace = cairoFont;
		font->fontName = "FiraCode";
		
		gFallbackFont = font;
		fontList().push_back(font);
	} else {
		Log::warning("Could not load backup font from platform!\n");
	}
}

//-------------------------------------------------------------------------------//
// Purpose: Allocate a opengl texture to draw with
//-------------------------------------------------------------------------------//
GLuint GL3_AllocTextTexture(const char* text, const TextDrawParams_t &params) {
	
	void* buffer = nullptr;
	GL3_DrawTextToBuffer(text, &buffer, params);
	
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.w, params.h, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);

	free(buffer);
	return tex;
}

//-------------------------------------------------------------------------------//
// Purpose: External text rendering API
//-------------------------------------------------------------------------------//


ScreenText_t* screenText(HScreenText txt) {
	for(ScreenText_t* s = screenTextHead; s; s = s->next) {
		if(s->serial == txt)
			return s;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------//
// Purpose: Insert screen text into the draw list
//-------------------------------------------------------------------------------//
static void GL3_AddScreenText(ScreenText_t* text) {
	ScreenText_t* s = nullptr;
	for(s = screenTextHead; s && s->next; s = s->next);
	if(s)
		s->next = text;
	else
		screenTextHead = text;
}

static void GL3_ScreenTextInit(ScreenText_t* text) {
	static HScreenText handle = 1;
	text->serial = handle++;
}

//-------------------------------------------------------------------------------//
// Purpose: Renders some text on screen with parameters
//-------------------------------------------------------------------------------//
HScreenText GL3_DrawScreenText(const char* text, const TextDrawParams_t& params, const ScreenTextParams_t& scrParams) {
	
	ScreenText_t* scrText = new ScreenText_t();
	GL3_ScreenTextInit(scrText);
	scrText->hidden = false;
	scrText->next = nullptr;
	scrText->oneshot = true;
	scrText->params = scrParams; 
	scrText->tex = 0;
	scrText->string = text;
	scrText->drawParams = params;
	
	GL3_TextSize(text, scrText->drawParams.w, scrText->drawParams.h, params.font);
	GL3_AddScreenText(scrText);
	
	return scrText->serial;
}

//-------------------------------------------------------------------------------//
// Purpose: Draw some simple screen text only for one frame
//-------------------------------------------------------------------------------//
void GL3_DrawSimpleScreenText(const char* text, int xpos, int ypos, HFont font, const colorRGBA &color) {
	
	ScreenTextParams_t params;
	params.duration = 0;
	params.fadestart = 0;
	params.starttime = 0;
	
	ScreenText_t* scrText = new ScreenText_t();
	GL3_ScreenTextInit(scrText);
	scrText->hidden = false;
	scrText->next = nullptr;
	scrText->oneshot = true;
	scrText->params = params; 
	scrText->tex = 0;
	scrText->string = text;
	
	scrText->drawParams.antiAliasing = TextAntialias::BEST;
	scrText->drawParams.borderColor = {1,1,1,1};
	scrText->drawParams.borderWidth = 1.0f;
	scrText->drawParams.fontColor = color;
	scrText->drawParams.font = font;
	scrText->drawParams.x = xpos;
	scrText->drawParams.y = ypos;
	scrText->drawParams.hintStyle = TextHintStyle::NONE;
	
	GL3_TextSize(text, scrText->drawParams.w, scrText->drawParams.h, font);
	
	GL3_AddScreenText(scrText);
}

//-------------------------------------------------------------------------------//
// Purpose: Updates existing screen text with new string
//-------------------------------------------------------------------------------//
void GL3_UpdateScreenText(HScreenText sText, const char* newText) {
	auto scrText = screenText(sText);
	
	// Don't update if there was no change to the text 
	if(Str::Equals(newText, scrText->string.c_str()))
		return;
	
	// ..otherwise destroy the current texture, update w/h and reupload tex
	if(scrText->tex) {
		glDeleteTextures(1, &scrText->tex);
	}
	
	GL3_TextSize(newText, scrText->drawParams.w, scrText->drawParams.h, scrText->drawParams.font);
	
	scrText->tex = GL3_AllocTextTexture(newText, scrText->drawParams);
}

//-------------------------------------------------------------------------------//
// Purpose: Hides screen text 
//-------------------------------------------------------------------------------//
void GL3_HideScreenText(HScreenText sText) {
	auto scrText = screenText(sText);
	scrText->hidden = true;
}

//-------------------------------------------------------------------------------//
// Purpose: Shows hidden screen text 
//-------------------------------------------------------------------------------//
void GL3_ShowScreenText(HScreenText sText) {
	auto scrText = screenText(sText);
	scrText->hidden = false;
}

void GL3_RemoveScreenText(HScreenText sText) {
	auto scrText = screenText(sText);
	// Hide, unset oneshot and set duration to 0
	scrText->hidden = true;
	scrText->oneshot = false; 
	scrText->params.duration = 0.0f;
}

//-------------------------------------------------------------------------------//
// Purpose: Deletes a screen text entry and all associated data 
//-------------------------------------------------------------------------------//
void GL3_DestroyScreenText(HScreenText sText) {
	auto scrText = screenText(sText);
	
	if(scrText->tex) {
		glDeleteTextures(1, &scrText->tex);
	}
	delete scrText;
}

void GL3_DestroyScreenText(ScreenText_t* txt) {
	if(txt) {
		glDeleteTextures(1, &txt->tex);
	}
	delete txt;
}

//-------------------------------------------------------------------------------//
// Purpose: Renders all active screen text entries
//-------------------------------------------------------------------------------//
void GL3_DrawScreenTextEntries(double curTime) {
	
	glUseProgram(gl3state.si2D.shaderProgram);
	
	ScreenText_t* prev = nullptr;
	for(auto* s = screenTextHead; s; s = s->next) {
		
		// Run text decay- remove entries we don't need anymore
		if(!s->oneshot && (s->params.duration + s->params.starttime >= curTime)) {
			if(prev)
				prev->next = s->next;
			else
				screenTextHead = s->next;
			GL3_DestroyScreenText(s);
			continue;
		}
		
		if(s->hidden)
			continue;
		
		// Create a buffer to draw to if we don't have it quite yet
		if(!s->tex) {
			s->tex = GL3_AllocTextTexture(s->string.c_str(), s->drawParams);
		}
		
		// Run draw
		GL3_Bind(s->tex);
		GL3_DrawTexturedRect(s->drawParams.x, s->drawParams.y, s->drawParams.w, s->drawParams.h);
		
		// Remove from the draw list if it's oneshot
		if(s->oneshot) {
			if(prev)
				prev->next = s->next;
			else
				screenTextHead = s->next;
			GL3_DestroyScreenText(s);
			continue;
		}
		
		prev = s;
	}
}
#endif