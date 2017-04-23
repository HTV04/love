/**
 * Copyright (c) 2006-2017 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "common/config.h"
#include "wrap_Graphics.h"
#include "Texture.h"
#include "image/ImageData.h"
#include "image/Image.h"
#include "font/Rasterizer.h"
#include "filesystem/wrap_Filesystem.h"
#include "video/VideoStream.h"
#include "image/wrap_Image.h"
#include "common/Reference.h"
#include "math/wrap_Transform.h"

#include "opengl/Graphics.h"

#include <cassert>
#include <cstring>
#include <cstdlib>

#include <algorithm>

// Shove the wrap_Graphics.lua code directly into a raw string literal.
static const char graphics_lua[] =
#include "wrap_Graphics.lua"
;

namespace love
{
namespace graphics
{

#define instance() (Module::getInstance<Graphics>(Module::M_GRAPHICS))

static int luax_checkgraphicscreated(lua_State *L)
{
	if (!instance()->isCreated())
		return luaL_error(L, "love.graphics cannot function without a window!");
	return 0;
}

int w_reset(lua_State *)
{
	instance()->reset();
	return 0;
}

int w_clear(lua_State *L)
{
	OptionalColorf color(Colorf(0.0f, 0.0f, 0.0f, 0.0f));
	std::vector<OptionalColorf> colors;

	OptionalInt stencil(0);
	OptionalDouble depth(1.0);

	int argtype = lua_type(L, 1);
	int startidx = -1;

	if (argtype == LUA_TTABLE)
	{
		int maxn = lua_gettop(L);
		colors.reserve(maxn);

		for (int i = 0; i < maxn; i++)
		{
			argtype = lua_type(L, i + 1);

			if (argtype == LUA_TNUMBER || argtype == LUA_TBOOLEAN)
			{
				startidx = i + 1;
				break;
			}
			else if (argtype == LUA_TNIL || argtype == LUA_TNONE || luax_objlen(L, i + 1) == 0)
			{
				colors.push_back(OptionalColorf());
				continue;
			}

			for (int j = 1; j <= 4; j++)
				lua_rawgeti(L, i + 1, j);

			OptionalColorf c;
			c.hasValue = true;
			c.value.r = (float) luaL_checknumber(L, -4);
			c.value.g = (float) luaL_checknumber(L, -3);
			c.value.b = (float) luaL_checknumber(L, -2);
			c.value.a = (float) luaL_optnumber(L, -1, 1.0);
			colors.push_back(c);

			lua_pop(L, 4);
		}
	}
	else if (argtype == LUA_TBOOLEAN)
	{
		color.hasValue = luax_toboolean(L, 1);
		startidx = 2;
	}
	else if (argtype != LUA_TNONE && argtype != LUA_TNIL)
	{
		color.hasValue = true;
		color.value.r = (float) luaL_checknumber(L, 1);
		color.value.g = (float) luaL_checknumber(L, 2);
		color.value.b = (float) luaL_checknumber(L, 3);
		color.value.a = (float) luaL_optnumber(L, 4, 1.0);
		startidx = 5;
	}

	if (startidx >= 0)
	{
		argtype = lua_type(L, startidx);
		if (argtype == LUA_TBOOLEAN)
			stencil.hasValue = luax_toboolean(L, startidx);
		else if (argtype == LUA_TNUMBER)
			stencil.value = (int) luaL_checkinteger(L, startidx);

		argtype = lua_type(L, startidx + 1);
		if (argtype == LUA_TBOOLEAN)
			depth.hasValue = luax_toboolean(L, startidx + 1);
		else if (argtype == LUA_TNUMBER)
			depth.value = luaL_checknumber(L, startidx + 1);
	}

	if (colors.empty())
		luax_catchexcept(L, [&]() { instance()->clear(color, stencil, depth); });
	else
		luax_catchexcept(L, [&]() { instance()->clear(colors, stencil, depth); });

	return 0;
}

int w_discard(lua_State *L)
{
	std::vector<bool> colorbuffers;

	if (lua_istable(L, 1))
	{
		for (size_t i = 1; i <= luax_objlen(L, 1); i++)
		{
			lua_rawgeti(L, 1, i);
			colorbuffers.push_back(luax_optboolean(L, -1, true));
			lua_pop(L, 1);
		}
	}
	else
	{
		bool discardcolor = luax_optboolean(L, 1, true);
		size_t numbuffers = std::max((size_t) 1, instance()->getCanvas().colors.size());
		colorbuffers = std::vector<bool>(numbuffers, discardcolor);
	}

	bool depthstencil = luax_optboolean(L, 2, true);
	instance()->discard(colorbuffers, depthstencil);
	return 0;
}

int w_present(lua_State *L)
{
	luax_catchexcept(L, [&]() { instance()->present(L); });
	return 0;
}

int w_isCreated(lua_State *L)
{
	luax_pushboolean(L, instance()->isCreated());
	return 1;
}

int w_isActive(lua_State *L)
{
	luax_pushboolean(L, instance()->isActive());
	return 1;
}

int w_isGammaCorrect(lua_State *L)
{
	luax_pushboolean(L, graphics::isGammaCorrect());
	return 1;
}

int w_getWidth(lua_State *L)
{
	lua_pushinteger(L, instance()->getWidth());
	return 1;
}

int w_getHeight(lua_State *L)
{
	lua_pushinteger(L, instance()->getHeight());
	return 1;
}

int w_getDimensions(lua_State *L)
{
	lua_pushinteger(L, instance()->getWidth());
	lua_pushinteger(L, instance()->getHeight());
	return 2;
}

int w_getPixelWidth(lua_State *L)
{
	lua_pushinteger(L, instance()->getPixelWidth());
	return 1;
}

int w_getPixelHeight(lua_State *L)
{
	lua_pushinteger(L, instance()->getPixelHeight());
	return 1;
}

int w_getPixelDimensions(lua_State *L)
{
	lua_pushinteger(L, instance()->getPixelWidth());
	lua_pushinteger(L, instance()->getPixelHeight());
	return 2;
}

int w_getPixelDensity(lua_State *L)
{
	lua_pushnumber(L, instance()->getScreenPixelDensity());
	return 1;
}

static Graphics::RenderTarget checkRenderTarget(lua_State *L, int idx)
{
	lua_rawgeti(L, idx, 1);
	Graphics::RenderTarget target(luax_checkcanvas(L, -1), 0);
	lua_pop(L, 1);

	TextureType type = target.canvas->getTextureType();
	if (type == TEXTURE_2D_ARRAY || type == TEXTURE_VOLUME)
		target.slice = luax_checkintflag(L, idx, "layer") - 1;
	else if (type == TEXTURE_CUBE)
		target.slice = luax_checkintflag(L, idx, "face") - 1;

	target.mipmap = luax_intflag(L, idx, "mipmap", 1) - 1;

	return target;
}

int w_setCanvas(lua_State *L)
{
	// Disable stencil writes.
	luax_catchexcept(L, [](){ instance()->stopDrawToStencilBuffer(); });

	// called with none -> reset to default buffer
	if (lua_isnoneornil(L, 1))
	{
		instance()->setCanvas();
		return 0;
	}

	bool is_table = lua_istable(L, 1);
	Graphics::RenderTargets targets;

	if (is_table)
	{
		lua_rawgeti(L, 1, 1);
		bool table_of_tables = lua_istable(L, -1);
		lua_pop(L, 1);

		for (int i = 1; i <= (int) luax_objlen(L, 1); i++)
		{
			lua_rawgeti(L, 1, i);

			if (table_of_tables)
				targets.colors.push_back(checkRenderTarget(L, -1));
			else
			{
				targets.colors.emplace_back(luax_checkcanvas(L, -1), 0);

				if (targets.colors.back().canvas->getTextureType() != TEXTURE_2D)
					return luaL_error(L, "The table-of-tables variant of setCanvas must be used with non-2D Canvases.");
			}

			lua_pop(L, 1);
		}

		lua_getfield(L, 1, "depthstencil");
		if (lua_istable(L, -1))
			targets.depthStencil = checkRenderTarget(L, -1);
		else if (!lua_isnoneornil(L, -1))
			targets.depthStencil.canvas = luax_checkcanvas(L, -1);
		lua_pop(L, 1);
	}
	else
	{
		for (int i = 1; i <= lua_gettop(L); i++)
		{
			Graphics::RenderTarget target(luax_checkcanvas(L, i), 0);
			TextureType type = target.canvas->getTextureType();

			if (i == 1 && type != TEXTURE_2D)
			{
				target.slice = (int) luaL_checknumber(L, i + 1) - 1;
				target.mipmap = (int) luaL_optinteger(L, i + 2, 1) - 1;
				targets.colors.push_back(target);
				break;
			}
			else if (type == TEXTURE_2D && lua_isnumber(L, i + 1))
			{
				target.mipmap = (int) luaL_optinteger(L, i + 1, 1) - 1;
				i++;
			}

			if (i > 1 && type != TEXTURE_2D)
				return luaL_error(L, "This variant of setCanvas only supports 2D texture types.");

			targets.colors.push_back(target);
		}
	}

	luax_catchexcept(L, [&]() {
		if (targets.colors.size() > 0)
			instance()->setCanvas(targets);
		else
			instance()->setCanvas();
	});
	
	return 0;
}

static void pushRenderTarget(lua_State *L, const Graphics::RenderTarget &rt)
{
	lua_createtable(L, 1, 2);

	luax_pushtype(L, rt.canvas);
	lua_rawseti(L, -2, 1);

	TextureType type = rt.canvas->getTextureType();

	if (type == TEXTURE_2D_ARRAY || type == TEXTURE_VOLUME)
	{
		lua_pushnumber(L, rt.slice + 1);
		lua_setfield(L, -2, "layer");
	}
	else if (type == TEXTURE_VOLUME)
	{
		lua_pushnumber(L, rt.slice + 1);
		lua_setfield(L, -2, "face");
	}

	lua_pushnumber(L, rt.mipmap + 1);
	lua_setfield(L, -2, "mipmap");
}

int w_getCanvas(lua_State *L)
{
	Graphics::RenderTargets targets = instance()->getCanvas();
	int ntargets = (int) targets.colors.size();

	if (ntargets == 0)
	{
		lua_pushnil(L);
		return 1;
	}

	bool shouldUseTablesVariant = targets.depthStencil.canvas != nullptr;

	if (!shouldUseTablesVariant)
	{
		for (const auto &rt : targets.colors)
		{
			if (rt.mipmap != 0 || rt.canvas->getTextureType() != TEXTURE_2D)
			{
				shouldUseTablesVariant = true;
				break;
			}
		}
	}

	if (shouldUseTablesVariant)
	{
		lua_createtable(L, ntargets, 0);

		for (int i = 0; i < ntargets; i++)
		{
			pushRenderTarget(L, targets.colors[i]);
			lua_rawseti(L, -2, i + 1);
		}

		if (targets.depthStencil.canvas != nullptr)
		{
			pushRenderTarget(L, targets.depthStencil);
			lua_setfield(L, -2, "depthstencil");
		}

		return 1;
	}
	else
	{
		for (const auto &rt : targets.colors)
			luax_pushtype(L, rt.canvas);

		return ntargets;
	}
}

static void screenshotCallback(love::image::ImageData *i, Reference *ref, void *gd)
{
	if (i != nullptr)
	{
		lua_State *L = (lua_State *) gd;
		ref->push(L);
		delete ref;
		luax_pushtype(L, i);
		lua_call(L, 1, 0);
	}
	else
		delete ref;
}

int w_captureScreenshot(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);

	Graphics::ScreenshotInfo info;
	info.callback = screenshotCallback;

	lua_pushvalue(L, 1);
	info.ref = luax_refif(L, LUA_TFUNCTION);
	lua_pop(L, 1);

	luax_catchexcept(L,
		[&]() { instance()->captureScreenshot(info); },
		[&](bool except) { if (except) delete info.ref; }
	);

	return 0;
}

int w_setScissor(lua_State *L)
{
	int nargs = lua_gettop(L);

	if (nargs == 0 || (nargs == 4 && lua_isnil(L, 1) && lua_isnil(L, 2)
		&& lua_isnil(L, 3) && lua_isnil(L, 4)))
	{
		instance()->setScissor();
		return 0;
	}

	Rect rect;
	rect.x = (int) luaL_checknumber(L, 1);
	rect.y = (int) luaL_checknumber(L, 2);
	rect.w = (int) luaL_checknumber(L, 3);
	rect.h = (int) luaL_checknumber(L, 4);

	if (rect.w < 0 || rect.h < 0)
		return luaL_error(L, "Can't set scissor with negative width and/or height.");

	instance()->setScissor(rect);
	return 0;
}

int w_intersectScissor(lua_State *L)
{
	Rect rect;
	rect.x = (int) luaL_checknumber(L, 1);
	rect.y = (int) luaL_checknumber(L, 2);
	rect.w = (int) luaL_checknumber(L, 3);
	rect.h = (int) luaL_checknumber(L, 4);

	if (rect.w < 0 || rect.h < 0)
		return luaL_error(L, "Can't set scissor with negative width and/or height.");

	instance()->intersectScissor(rect);
	return 0;
}

int w_getScissor(lua_State *L)
{
	Rect rect;
	if (!instance()->getScissor(rect))
		return 0;

	lua_pushinteger(L, rect.x);
	lua_pushinteger(L, rect.y);
	lua_pushinteger(L, rect.w);
	lua_pushinteger(L, rect.h);

	return 4;
}

int w_stencil(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);

	StencilAction action = STENCIL_REPLACE;

	if (!lua_isnoneornil(L, 2))
	{
		const char *actionstr = luaL_checkstring(L, 2);
		if (!getConstant(actionstr, action))
			return luaL_error(L, "Invalid stencil draw action: %s", actionstr);
	}

	int stencilvalue = (int) luaL_optnumber(L, 3, 1);

	// Fourth argument: whether to keep the contents of the stencil buffer.
	int argtype = lua_type(L, 4);
	if (argtype == LUA_TNONE || argtype == LUA_TNIL || (argtype == LUA_TBOOLEAN && luax_toboolean(L, 4) == false))
		instance()->clearStencil(0);
	else if (argtype == LUA_TNUMBER)
		instance()->clearStencil((int) luaL_checkinteger(L, 4));
	else if (argtype != LUA_TBOOLEAN)
		luaL_checktype(L, 4, LUA_TBOOLEAN);

	luax_catchexcept(L, [&](){ instance()->drawToStencilBuffer(action, stencilvalue); });

	// Call stencilfunc()
	lua_pushvalue(L, 1);
	lua_call(L, 0, 0);

	luax_catchexcept(L, [&](){ instance()->stopDrawToStencilBuffer(); });
	return 0;
}

int w_setStencilTest(lua_State *L)
{
	// COMPARE_ALWAYS effectively disables stencil testing.
	CompareMode compare = COMPARE_ALWAYS;
	int comparevalue = 0;

	if (!lua_isnoneornil(L, 1))
	{
		const char *comparestr = luaL_checkstring(L, 1);
		if (!getConstant(comparestr, compare))
			return luaL_error(L, "Invalid compare mode: %s", comparestr);

		comparevalue = (int) luaL_checknumber(L, 2);
	}

	luax_catchexcept(L, [&](){ instance()->setStencilTest(compare, comparevalue); });
	return 0;
}

int w_getStencilTest(lua_State *L)
{
	CompareMode compare = COMPARE_ALWAYS;
	int comparevalue = 1;

	instance()->getStencilTest(compare, comparevalue);

	const char *comparestr;
	if (!getConstant(compare, comparestr))
		return luaL_error(L, "Unknown compare mode.");

	lua_pushstring(L, comparestr);
	lua_pushnumber(L, comparevalue);
	return 2;
}

static void parsePixelDensity(love::filesystem::FileData *d, float *pixeldensity)
{
	// Parse a density scale of 2.0 from "image@2x.png".
	const std::string &fname = d->getName();

	size_t namelen = fname.length();
	size_t atpos = fname.rfind('@');

	if (atpos != std::string::npos && atpos + 2 < namelen
		&& (fname[namelen - 1] == 'x' || fname[namelen - 1] == 'X'))
	{
		char *end = nullptr;
		long density = strtol(fname.c_str() + atpos + 1, &end, 10);
		if (end != nullptr && density > 0 && pixeldensity != nullptr)
			*pixeldensity = (float) density;
	}
}

static Image::Settings w__optImageSettings(lua_State *L, int idx, const Image::Settings &s)
{
	Image::Settings settings = s;

	if (!lua_isnoneornil(L, idx))
	{
		luaL_checktype(L, idx, LUA_TTABLE);
		settings.mipmaps = luax_boolflag(L, idx, "mipmaps", s.mipmaps);
		settings.linear = luax_boolflag(L, idx, "linear", s.linear);
		settings.pixeldensity = (float) luax_numberflag(L, idx, "pixeldensity", s.pixeldensity);
	}

	return settings;
}

static std::pair<StrongRef<love::image::ImageData>, StrongRef<love::image::CompressedImageData>>
getImageData(lua_State *L, int idx, bool allowcompressed, float *density)
{
	StrongRef<love::image::ImageData> idata;
	StrongRef<love::image::CompressedImageData> cdata;

	// Convert to ImageData / CompressedImageData, if necessary.
	if (lua_isstring(L, idx) || luax_istype(L, idx, love::filesystem::File::type) || luax_istype(L, idx, love::filesystem::FileData::type))
	{
		auto imagemodule = Module::getInstance<love::image::Image>(Module::M_IMAGE);
		if (imagemodule == nullptr)
			luaL_error(L, "Cannot load images without the love.image module.");

		StrongRef<love::filesystem::FileData> fdata(love::filesystem::luax_getfiledata(L, idx), Acquire::NORETAIN);

		if (density != nullptr)
			parsePixelDensity(fdata, density);

		if (allowcompressed && imagemodule->isCompressed(fdata))
			luax_catchexcept(L, [&]() { cdata.set(imagemodule->newCompressedData(fdata), Acquire::NORETAIN); });
		else
			luax_catchexcept(L, [&]() { idata.set(imagemodule->newImageData(fdata), Acquire::NORETAIN); });

	}
	else if (luax_istype(L, idx, love::image::CompressedImageData::type))
		cdata.set(love::image::luax_checkcompressedimagedata(L, idx));
	else
		idata.set(love::image::luax_checkimagedata(L, idx));

	return std::make_pair(idata, cdata);
}

static int w__pushNewImage(lua_State *L, Image::Slices &slices, const Image::Settings &settings)
{
	StrongRef<Image> i;
	luax_catchexcept(L,
		[&]() { i.set(instance()->newImage(slices, settings), Acquire::NORETAIN); },
		[&](bool) { slices.clear(); }
	);

	luax_pushtype(L, i);
	return 1;
}

int w_newCubeImage(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Image::Slices slices(TEXTURE_CUBE);
	Image::Settings settings;

	auto imagemodule = Module::getInstance<love::image::Image>(Module::M_IMAGE);

	if (!lua_istable(L, 1))
	{
		auto data = getImageData(L, 1, true, &settings.pixeldensity);

		std::vector<StrongRef<love::image::ImageData>> faces;

		if (data.first.get())
		{
			luax_catchexcept(L, [&](){ faces = imagemodule->newCubeFaces(data.first); });

			for (int i = 0; i < (int) faces.size(); i++)
				slices.set(i, 0, faces[i]);
		}
		else
			slices.add(data.second, 0, 0, true, true);
	}
	else
	{
		int tlen = (int) luax_objlen(L, 1);

		if (luax_isarrayoftables(L, 1))
		{
			if (tlen != 6)
				return luaL_error(L, "Cubemap images must have 6 faces.");

			for (int face = 0; face < tlen; face++)
			{
				lua_rawgeti(L, 1, face + 1);
				luaL_checktype(L, -1, LUA_TTABLE);

				int miplen = std::max(1, (int) luax_objlen(L, -1));

				for (int mip = 0; mip < miplen; mip++)
				{
					lua_rawgeti(L, -1, mip + 1);

					auto data = getImageData(L, -1, true, face == 0 && mip == 0 ? &settings.pixeldensity : nullptr);
					if (data.first.get())
						slices.set(face, mip, data.first);
					else
						slices.set(face, mip, data.second->getSlice(0, 0));

					lua_pop(L, 1);
				}
			}
		}
		else
		{
			bool usemipmaps = false;

			for (int i = 0; i < tlen; i++)
			{
				lua_rawgeti(L, 1, i + 1);

				auto data = getImageData(L, -1, true, i == 0 ? &settings.pixeldensity : nullptr);

				if (data.first.get())
				{
					if (usemipmaps || data.first->getWidth() != data.first->getHeight())
					{
						usemipmaps = true;

						std::vector<StrongRef<love::image::ImageData>> faces;
						luax_catchexcept(L, [&](){ faces = imagemodule->newCubeFaces(data.first); });

						for (int face = 0; face < (int) faces.size(); face++)
							slices.set(face, i, faces[i]);
					}
					else
						slices.set(i, 0, data.first);
				}
				else
					slices.add(data.second, i, 0, false, true);
			}
		}

		lua_pop(L, tlen);
	}

	settings = w__optImageSettings(L, 2, settings);
	return w__pushNewImage(L, slices, settings);
}

int w_newArrayImage(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Image::Slices slices(TEXTURE_2D_ARRAY);
	Image::Settings settings;

	if (lua_istable(L, 1))
	{
		int tlen = std::max(1, (int) luax_objlen(L, 1));

		if (luax_isarrayoftables(L, 1))
		{
			for (int slice = 0; slice < tlen; slice++)
			{
				lua_rawgeti(L, 1, slice + 1);
				luaL_checktype(L, -1, LUA_TTABLE);

				int miplen = std::max(1, (int) luax_objlen(L, -1));

				for (int mip = 0; mip < miplen; mip++)
				{
					lua_rawgeti(L, -1, mip + 1);

					auto data = getImageData(L, -1, true, slice == 0 && mip == 0 ? &settings.pixeldensity : nullptr);
					if (data.first.get())
						slices.set(slice, mip, data.first);
					else
						slices.set(slice, mip, data.second->getSlice(0, 0));

					lua_pop(L, 1);
				}
			}
		}
		else
		{
			for (int slice = 0; slice < tlen; slice++)
			{
				lua_rawgeti(L, 1, slice + 1);
				auto data = getImageData(L, -1, true, slice == 0 ? &settings.pixeldensity : nullptr);
				if (data.first.get())
					slices.set(slice, 0, data.first);
				else
					slices.add(data.second, slice, 0, false, true);
			}
		}

		lua_pop(L, tlen);
	}
	else
	{
		auto data = getImageData(L, 1, true, &settings.pixeldensity);
		if (data.first.get())
			slices.set(0, 0, data.first);
		else
			slices.add(data.second, 0, 0, true, true);
	}

	settings = w__optImageSettings(L, 2, settings);
	return w__pushNewImage(L, slices, settings);
}

int w_newVolumeImage(lua_State *L)
{
	luax_checkgraphicscreated(L);

	auto imagemodule = Module::getInstance<love::image::Image>(Module::M_IMAGE);

	Image::Slices slices(TEXTURE_VOLUME);
	Image::Settings settings;

	if (lua_istable(L, 1))
	{
		int tlen = std::max(1, (int) luax_objlen(L, 1));

		if (luax_isarrayoftables(L, 1))
		{
			for (int mip = 0; mip < tlen; mip++)
			{
				lua_rawgeti(L, 1, mip + 1);
				luaL_checktype(L, -1, LUA_TTABLE);

				int slicelen = std::max(1, (int) luax_objlen(L, -1));

				for (int slice = 0; slice < slicelen; slice++)
				{
					lua_rawgeti(L, -1, mip + 1);

					auto data = getImageData(L, -1, true, slice == 0 && mip == 0 ? &settings.pixeldensity : nullptr);
					if (data.first.get())
						slices.set(slice, mip, data.first);
					else
						slices.set(slice, mip, data.second->getSlice(0, 0));

					lua_pop(L, 1);
				}
			}
		}
		else
		{
			for (int layer = 0; layer < tlen; layer++)
			{
				lua_rawgeti(L, 1, layer + 1);
				auto data = getImageData(L, -1, true, layer == 0 ? &settings.pixeldensity : nullptr);
				if (data.first.get())
					slices.set(layer, 0, data.first);
				else
					slices.add(data.second, layer, 0, false, true);
			}
		}

		lua_pop(L, tlen);
	}
	else
	{
		auto data = getImageData(L, 1, true, &settings.pixeldensity);

		if (data.first.get())
		{
			std::vector<StrongRef<love::image::ImageData>> layers;
			luax_catchexcept(L, [&](){ layers = imagemodule->newVolumeLayers(data.first); });

			for (int i = 0; i < (int) layers.size(); i++)
				slices.set(i, 0, layers[i]);
		}
		else
			slices.add(data.second, 0, 0, true, true);
	}

	settings = w__optImageSettings(L, 2, settings);
	return w__pushNewImage(L, slices, settings);
}

int w_newImage(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Image::Slices slices(TEXTURE_2D);
	Image::Settings settings;

	if (lua_istable(L, 1))
	{
		int n = std::max(1, (int) luax_objlen(L, 1));
		for (int i = 0; i < n; i++)
		{
			lua_rawgeti(L, 1, i + 1);
			auto data = getImageData(L, -1, true, i == 0 ? &settings.pixeldensity : nullptr);
			if (data.first.get())
				slices.set(0, i, data.first);
			else
				slices.set(0, i, data.second->getSlice(0, 0));
		}
		lua_pop(L, n);
	}
	else
	{
		auto data = getImageData(L, 1, true, &settings.pixeldensity);
		if (data.first.get())
			slices.set(0, 0, data.first);
		else
			slices.add(data.second, 0, 0, false, true);
	}

	settings = w__optImageSettings(L, 2, settings);
	return w__pushNewImage(L, slices, settings);
}

int w_newQuad(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Quad::Viewport v;
	v.x = luaL_checknumber(L, 1);
	v.y = luaL_checknumber(L, 2);
	v.w = luaL_checknumber(L, 3);
	v.h = luaL_checknumber(L, 4);

	double sw = 0.0f;
	double sh = 0.0f;
	int layer = 0;

	if (luax_istype(L, 5, Texture::type))
	{
		Texture *texture = luax_checktexture(L, 5);
		sw = texture->getWidth();
		sh = texture->getHeight();
	}
	else if (luax_istype(L, 6, Texture::type))
	{
		layer = (int) luaL_checknumber(L, 5) - 1;
		Texture *texture = luax_checktexture(L, 6);
		sw = texture->getWidth();
		sh = texture->getHeight();
	}
	else if (!lua_isnoneornil(L, 7))
	{
		layer = (int) luaL_checknumber(L, 5) - 1;
		sw = luaL_checknumber(L, 6);
		sh = luaL_checknumber(L, 7);
	}
	else
	{
		sw = luaL_checknumber(L, 5);
		sh = luaL_checknumber(L, 6);
	}

	Quad *quad = instance()->newQuad(v, sw, sh);
	quad->setLayer(layer);

	luax_pushtype(L, quad);
	quad->release();
	return 1;
}

int w_newFont(lua_State *L)
{
	luax_checkgraphicscreated(L);

	graphics::Font *font = nullptr;

	// Convert to Rasterizer, if necessary.
	if (!luax_istype(L, 1, love::font::Rasterizer::type))
	{
		std::vector<int> idxs;
		for (int i = 0; i < lua_gettop(L); i++)
			idxs.push_back(i + 1);

		luax_convobj(L, &idxs[0], (int) idxs.size(), "font", "newRasterizer");
	}

	love::font::Rasterizer *rasterizer = luax_checktype<love::font::Rasterizer>(L, 1);

	luax_catchexcept(L, [&]() {
		font = instance()->newFont(rasterizer, instance()->getDefaultFilter()); }
	);

	// Push the type.
	luax_pushtype(L, font);
	font->release();
	return 1;
}

int w_newImageFont(lua_State *L)
{
	luax_checkgraphicscreated(L);

	// filter for glyphs
	Texture::Filter filter = instance()->getDefaultFilter();

	// Convert to Rasterizer if necessary.
	if (!luax_istype(L, 1, love::font::Rasterizer::type))
	{
		luaL_checktype(L, 2, LUA_TSTRING);

		std::vector<int> idxs;
		for (int i = 0; i < lua_gettop(L); i++)
			idxs.push_back(i + 1);

		luax_convobj(L, &idxs[0], (int) idxs.size(), "font", "newImageRasterizer");
	}

	love::font::Rasterizer *rasterizer = luax_checktype<love::font::Rasterizer>(L, 1);

	// Create the font.
	Font *font = instance()->newFont(rasterizer, filter);

	// Push the type.
	luax_pushtype(L, font);
	font->release();
	return 1;
}

int w_newSpriteBatch(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Texture *texture = luax_checktexture(L, 1);
	int size = (int) luaL_optnumber(L, 2, 1000);
	vertex::Usage usage = vertex::USAGE_DYNAMIC;
	if (lua_gettop(L) > 2)
	{
		const char *usagestr = luaL_checkstring(L, 3);
		if (!vertex::getConstant(usagestr, usage))
			return luaL_error(L, "Invalid usage hint: %s", usagestr);
	}

	SpriteBatch *t = nullptr;
	luax_catchexcept(L,
		[&](){ t = instance()->newSpriteBatch(texture, size, usage); }
	);

	luax_pushtype(L, t);
	t->release();
	return 1;
}

int w_newParticleSystem(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Texture *texture = luax_checktexture(L, 1);
	lua_Number size = luaL_optnumber(L, 2, 1000);
	ParticleSystem *t = nullptr;
	if (size < 1.0 || size > ParticleSystem::MAX_PARTICLES)
		return luaL_error(L, "Invalid ParticleSystem size");

	luax_catchexcept(L,
		[&](){ t = instance()->newParticleSystem(texture, int(size)); }
	);

	luax_pushtype(L, t);
	t->release();
	return 1;
}

int w_newCanvas(lua_State *L)
{
	luax_checkgraphicscreated(L);

	Canvas::Settings settings;

	// check if width and height are given. else default to screen dimensions.
	settings.width  = (int) luaL_optnumber(L, 1, instance()->getWidth());
	settings.height = (int) luaL_optnumber(L, 2, instance()->getHeight());

	// Default to the screen's current pixel density scale.
	settings.pixeldensity = instance()->getScreenPixelDensity();

	int startidx = 3;

	if (lua_isnumber(L, 3))
	{
		settings.layers = (int) luaL_checknumber(L, 3);
		settings.type = TEXTURE_2D_ARRAY;
		startidx = 4;
	}

	if (!lua_isnoneornil(L, startidx))
	{
		settings.pixeldensity = (float) luax_numberflag(L, startidx, "pixeldensity", settings.pixeldensity);
		settings.msaa = luax_intflag(L, startidx, "msaa", settings.msaa);

		lua_getfield(L, startidx, "format");
		if (!lua_isnoneornil(L, -1))
		{
			const char *str = luaL_checkstring(L, -1);
			if (!getConstant(str, settings.format))
				return luaL_error(L, "Invalid pixel format: %s", str);
		}
		lua_pop(L, 1);

		lua_getfield(L, startidx, "type");
		if (!lua_isnoneornil(L, -1))
		{
			const char *str = luaL_checkstring(L, -1);
			if (!Texture::getConstant(str, settings.type))
				return luaL_error(L, "Invalid texture type: %s", str);
		}
		lua_pop(L, 1);

		lua_getfield(L, startidx, "readable");
		if (!lua_isnoneornil(L, -1))
		{
			luaL_checktype(L, -1, LUA_TBOOLEAN);
			settings.readable.hasValue = true;
			settings.readable.value = luax_toboolean(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, startidx, "mipmaps");
		if (!lua_isnoneornil(L, -1))
		{
			const char *str = luaL_checkstring(L, -1);
			if (!Canvas::getConstant(str, settings.mipmaps))
				return luaL_error(L, "Invalid Canvas mipmap mode: %s", str);
		}
		lua_pop(L, 1);
	}

	Canvas *canvas = nullptr;
	luax_catchexcept(L, [&](){ canvas = instance()->newCanvas(settings); });

	luax_pushtype(L, canvas);
	canvas->release();
	return 1;
}

static int w_getShaderSource(lua_State *L, int startidx, bool gles, Shader::ShaderSource &source)
{
	luax_checkgraphicscreated(L);

	// read any filepath arguments
	for (int i = startidx; i < startidx + 2; i++)
	{
		if (!lua_isstring(L, i))
			continue;

		// call love.filesystem.isFile(arg_i)
		luax_getfunction(L, "filesystem", "isFile");
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);

		bool isFile = luax_toboolean(L, -1);
		lua_pop(L, 1);

		if (isFile)
		{
			luax_getfunction(L, "filesystem", "read");
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			lua_replace(L, i);
		}
		else
		{
			// Check if the argument looks like a filepath - we want a nicer
			// error for misspelled filepath arguments.
			size_t slen = 0;
			const char *str = lua_tolstring(L, i, &slen);
			if (slen > 0 && slen < 256 && !strchr(str, '\n'))
			{
				const char *ext = strchr(str, '.');
				if (ext != nullptr && !strchr(ext, ';') && !strchr(ext, ' '))
					return luaL_error(L, "Could not open file %s. Does not exist.", str);
			}
		}
	}

	bool has_arg1 = lua_isstring(L, startidx + 0) != 0;
	bool has_arg2 = lua_isstring(L, startidx + 1) != 0;

	// require at least one string argument
	if (!(has_arg1 || has_arg2))
		luaL_checkstring(L, startidx);

	luax_getfunction(L, "graphics", "_shaderCodeToGLSL");

	// push vertexcode and pixelcode strings to the top of the stack
	lua_pushboolean(L, gles);

	if (has_arg1)
		lua_pushvalue(L, startidx + 0);
	else
		lua_pushnil(L);

	if (has_arg2)
		lua_pushvalue(L, startidx + 1);
	else
		lua_pushnil(L);

	// call effectCodeToGLSL, returned values will be at the top of the stack
	if (lua_pcall(L, 3, 2, 0) != 0)
		return luaL_error(L, "%s", lua_tostring(L, -1));

	// vertex shader code
	if (lua_isstring(L, -2))
		source.vertex = luax_checkstring(L, -2);
	else if (has_arg1 && has_arg2)
		return luaL_error(L, "Could not parse vertex shader code (missing 'position' function?)");

	// pixel shader code
	if (lua_isstring(L, -1))
		source.pixel = luax_checkstring(L, -1);
	else if (has_arg1 && has_arg2)
		return luaL_error(L, "Could not parse pixel shader code (missing 'effect' function?)");

	if (source.vertex.empty() && source.pixel.empty())
	{
		// Original args had source code, but effectCodeToGLSL couldn't translate it
		for (int i = startidx; i < startidx + 2; i++)
		{
			if (lua_isstring(L, i))
				return luaL_argerror(L, i, "missing 'position' or 'effect' function?");
		}
	}

	return 0;
}

int w_newShader(lua_State *L)
{
	bool gles = instance()->getRenderer() == Graphics::RENDERER_OPENGLES;

	Shader::ShaderSource source;
	w_getShaderSource(L, 1, gles, source);

	bool should_error = false;
	try
	{
		Shader *shader = instance()->newShader(source);
		luax_pushtype(L, shader);
		shader->release();
	}
	catch (love::Exception &e)
	{
		luax_getfunction(L, "graphics", "_transformGLSLErrorMessages");
		lua_pushstring(L, e.what());

		// Function pushes the new error string onto the stack.
		lua_pcall(L, 1, 1, 0);
		should_error = true;
	}

	if (should_error)
		return lua_error(L);

	return 1;
}

int w_validateShader(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	bool gles = luax_toboolean(L, 1);

	Shader::ShaderSource source;
	w_getShaderSource(L, 2, gles, source);

	std::string err;
	bool success = instance()->validateShader(gles, source, err);

	luax_pushboolean(L, success);

	if (!success)
	{
		luax_pushstring(L, err);
		return 2;
	}

	return 1;
}

static vertex::Usage luax_optmeshusage(lua_State *L, int idx, vertex::Usage def)
{
	const char *usagestr = lua_isnoneornil(L, idx) ? nullptr : luaL_checkstring(L, idx);

	if (usagestr && !vertex::getConstant(usagestr, def))
		luaL_error(L, "Invalid usage hint: %s", usagestr);

	return def;
}

static Mesh::DrawMode luax_optmeshdrawmode(lua_State *L, int idx, Mesh::DrawMode def)
{
	const char *modestr = lua_isnoneornil(L, idx) ? nullptr : luaL_checkstring(L, idx);

	if (modestr && !Mesh::getConstant(modestr, def))
		luaL_error(L, "Invalid mesh draw mode: %s", modestr);

	return def;
}

static Mesh *newStandardMesh(lua_State *L)
{
	Mesh *t = nullptr;

	Mesh::DrawMode drawmode = luax_optmeshdrawmode(L, 2, Mesh::DRAWMODE_FAN);
	vertex::Usage usage = luax_optmeshusage(L, 3, vertex::USAGE_DYNAMIC);

	// First argument is a table of standard vertices, or the number of
	// standard vertices.
	if (lua_istable(L, 1))
	{
		size_t vertexcount = luax_objlen(L, 1);
		std::vector<Vertex> vertices;
		vertices.reserve(vertexcount);

		// Get the vertices from the table.
		for (size_t i = 1; i <= vertexcount; i++)
		{
			lua_rawgeti(L, 1, (int) i);

			if (lua_type(L, -1) != LUA_TTABLE)
			{
				luax_typerror(L, 1, "table of tables");
				return nullptr;
			}

			for (int j = 1; j <= 8; j++)
				lua_rawgeti(L, -j, j);

			Vertex v;

			v.x = (float) luaL_checknumber(L, -8);
			v.y = (float) luaL_checknumber(L, -7);
			v.s = (float) luaL_optnumber(L, -6, 0.0);
			v.t = (float) luaL_optnumber(L, -5, 0.0);

			v.color.r = (unsigned char) (luaL_optnumber(L, -4, 1.0) * 255.0);
			v.color.g = (unsigned char) (luaL_optnumber(L, -3, 1.0) * 255.0);
			v.color.b = (unsigned char) (luaL_optnumber(L, -2, 1.0) * 255.0);
			v.color.a = (unsigned char) (luaL_optnumber(L, -1, 1.0) * 255.0);

			lua_pop(L, 9);
			vertices.push_back(v);
		}

		luax_catchexcept(L, [&](){ t = instance()->newMesh(vertices, drawmode, usage); });
	}
	else
	{
		int count = (int) luaL_checknumber(L, 1);
		luax_catchexcept(L, [&](){ t = instance()->newMesh(count, drawmode, usage); });
	}

	return t;
}

static Mesh *newCustomMesh(lua_State *L)
{
	Mesh *t = nullptr;

	// First argument is the vertex format, second is a table of vertices or
	// the number of vertices.
	std::vector<Mesh::AttribFormat> vertexformat;

	Mesh::DrawMode drawmode = luax_optmeshdrawmode(L, 3, Mesh::DRAWMODE_FAN);
	vertex::Usage usage = luax_optmeshusage(L, 4, vertex::USAGE_DYNAMIC);

	lua_rawgeti(L, 1, 1);
	if (!lua_istable(L, -1))
	{
		luaL_argerror(L, 1, "table of tables expected");
		return nullptr;
	}
	lua_pop(L, 1);

	// Per-vertex attribute formats.
	for (int i = 1; i <= (int) luax_objlen(L, 1); i++)
	{
		lua_rawgeti(L, 1, i);

		// {name, datatype, components}
		for (int j = 1; j <= 3; j++)
			lua_rawgeti(L, -j, j);

		Mesh::AttribFormat format;
		format.name = luaL_checkstring(L, -3);

		const char *tname = luaL_checkstring(L, -2);
		if (!Mesh::getConstant(tname, format.type))
		{
			luaL_error(L, "Invalid Mesh vertex data type name: %s", tname);
			return nullptr;
		}

		format.components = (int) luaL_checknumber(L, -1);
		if (format.components <= 0 || format.components > 4)
		{
			luaL_error(L, "Number of vertex attribute components must be between 1 and 4 (got %d)", format.components);
			return nullptr;
		}

		lua_pop(L, 4);
		vertexformat.push_back(format);
	}

	if (lua_isnumber(L, 2))
	{
		int vertexcount = (int) luaL_checknumber(L, 2);
		luax_catchexcept(L, [&](){ t = instance()->newMesh(vertexformat, vertexcount, drawmode, usage); });
	}
	else if (luax_istype(L, 2, Data::type))
	{
		// Vertex data comes directly from a Data object.
		Data *data = luax_checktype<Data>(L, 2);
		luax_catchexcept(L, [&](){ t = instance()->newMesh(vertexformat, data->getData(), data->getSize(), drawmode, usage); });
	}
	else
	{
		// Table of vertices.
		lua_rawgeti(L, 2, 1);
		if (!lua_istable(L, -1))
		{
			luaL_argerror(L, 2, "expected table of tables");
			return nullptr;
		}
		lua_pop(L, 1);

		int vertexcomponents = 0;
		for (const Mesh::AttribFormat &format : vertexformat)
			vertexcomponents += format.components;

		size_t numvertices = luax_objlen(L, 2);

		luax_catchexcept(L, [&](){ t = instance()->newMesh(vertexformat, numvertices, drawmode, usage); });

		// Maximum possible data size for a single vertex attribute.
		char data[sizeof(float) * 4];

		for (size_t vertindex = 0; vertindex < numvertices; vertindex++)
		{
			// get vertices[vertindex]
			lua_rawgeti(L, 2, vertindex + 1);
			luaL_checktype(L, -1, LUA_TTABLE);

			int n = 0;
			for (size_t i = 0; i < vertexformat.size(); i++)
			{
				int components = vertexformat[i].components;

				// get vertices[vertindex][n]
				for (int c = 0; c < components; c++)
				{
					n++;
					lua_rawgeti(L, -(c + 1), n);
				}

				// Fetch the values from Lua and store them in data buffer.
				luax_writeAttributeData(L, -components, vertexformat[i].type, components, data);

				lua_pop(L, components);

				luax_catchexcept(L,
					[&](){ t->setVertexAttribute(vertindex, i, data, sizeof(float) * 4); },
					[&](bool diderror){ if (diderror) t->release(); }
				);
			}

			lua_pop(L, 1); // pop vertices[vertindex]
		}

		t->flush();
	}

	return t;
}

int w_newMesh(lua_State *L)
{
	luax_checkgraphicscreated(L);

	// Check first argument: table or number of vertices.
	int arg1type = lua_type(L, 1);
	if (arg1type != LUA_TTABLE && arg1type != LUA_TNUMBER)
		luaL_argerror(L, 1, "table or number expected");

	Mesh *t = nullptr;

	int arg2type = lua_type(L, 2);
	if (arg1type == LUA_TTABLE && (arg2type == LUA_TTABLE || arg2type == LUA_TNUMBER || arg2type == LUA_TUSERDATA))
		t = newCustomMesh(L);
	else
		t = newStandardMesh(L);

	luax_pushtype(L, t);
	t->release();
	return 1;
}

int w_newText(lua_State *L)
{
	luax_checkgraphicscreated(L);

	graphics::Font *font = luax_checkfont(L, 1);
	Text *t = nullptr;

	if (lua_isnoneornil(L, 2))
		luax_catchexcept(L, [&](){ t = instance()->newText(font); });
	else
	{
		std::vector<Font::ColoredString> text;
		luax_checkcoloredstring(L, 2, text);

		luax_catchexcept(L, [&](){ t = instance()->newText(font, text); });
	}

	luax_pushtype(L, t);
	t->release();
	return 1;
}

int w_newVideo(lua_State *L)
{
	luax_checkgraphicscreated(L);

	if (!luax_istype(L, 1, love::video::VideoStream::type))
		luax_convobj(L, 1, "video", "newVideoStream");

	auto stream = luax_checktype<love::video::VideoStream>(L, 1);
	float pixeldensity = (float) luaL_optnumber(L, 2, 1.0);
	Video *video = nullptr;

	luax_catchexcept(L, [&]() { video = instance()->newVideo(stream, pixeldensity); });

	luax_pushtype(L, video);
	video->release();
	return 1;
}

int w_setColor(lua_State *L)
{
	Colorf c;
	if (lua_istable(L, 1))
	{
		for (int i = 1; i <= 4; i++)
			lua_rawgeti(L, 1, i);

		c.r = (float) luaL_checknumber(L, -4);
		c.g = (float) luaL_checknumber(L, -3);
		c.b = (float) luaL_checknumber(L, -2);
		c.a = (float) luaL_optnumber(L, -1, 1.0);

		lua_pop(L, 4);
	}
	else
	{
		c.r = (float) luaL_checknumber(L, 1);
		c.g = (float) luaL_checknumber(L, 2);
		c.b = (float) luaL_checknumber(L, 3);
		c.a = (float) luaL_optnumber(L, 4, 1.0);
	}
	instance()->setColor(c);
	return 0;
}

int w_getColor(lua_State *L)
{
	Colorf c = instance()->getColor();
	lua_pushnumber(L, c.r);
	lua_pushnumber(L, c.g);
	lua_pushnumber(L, c.b);
	lua_pushnumber(L, c.a);
	return 4;
}

int w_setBackgroundColor(lua_State *L)
{
	Colorf c;
	if (lua_istable(L, 1))
	{
		for (int i = 1; i <= 4; i++)
			lua_rawgeti(L, 1, i);

		c.r = (float) luaL_checknumber(L, -4);
		c.g = (float) luaL_checknumber(L, -3);
		c.b = (float) luaL_checknumber(L, -2);
		c.a = (float) luaL_optnumber(L, -1, 1.0);

		lua_pop(L, 4);
	}
	else
	{
		c.r = (float) luaL_checknumber(L, 1);
		c.g = (float) luaL_checknumber(L, 2);
		c.b = (float) luaL_checknumber(L, 3);
		c.a = (float) luaL_optnumber(L, 4, 1.0);
	}
	instance()->setBackgroundColor(c);
	return 0;
}

int w_getBackgroundColor(lua_State *L)
{
	Colorf c = instance()->getBackgroundColor();
	lua_pushnumber(L, c.r);
	lua_pushnumber(L, c.g);
	lua_pushnumber(L, c.b);
	lua_pushnumber(L, c.a);
	return 4;
}

int w_setNewFont(lua_State *L)
{
	int ret = w_newFont(L);
	Font *font = luax_checktype<Font>(L, -1);
	instance()->setFont(font);
	return ret;
}

int w_setFont(lua_State *L)
{
	Font *font = luax_checktype<Font>(L, 1);
	instance()->setFont(font);
	return 0;
}

int w_getFont(lua_State *L)
{
	Font *f = nullptr;
	luax_catchexcept(L, [&](){ f = instance()->getFont(); });

	luax_pushtype(L, f);
	return 1;
}

int w_setColorMask(lua_State *L)
{
	Graphics::ColorMask mask;

	if (lua_gettop(L) <= 1 && lua_isnoneornil(L, 1))
	{
		// Enable all color components if no argument is given.
		mask.r = mask.g = mask.b = mask.a = true;
	}
	else
	{
		mask.r = luax_toboolean(L, 1);
		mask.g = luax_toboolean(L, 2);
		mask.b = luax_toboolean(L, 3);
		mask.a = luax_toboolean(L, 4);
	}

	instance()->setColorMask(mask);

	return 0;
}

int w_getColorMask(lua_State *L)
{
	Graphics::ColorMask mask = instance()->getColorMask();

	luax_pushboolean(L, mask.r);
	luax_pushboolean(L, mask.g);
	luax_pushboolean(L, mask.b);
	luax_pushboolean(L, mask.a);

	return 4;
}

int w_setBlendMode(lua_State *L)
{
	Graphics::BlendMode mode;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, mode))
		return luaL_error(L, "Invalid blend mode: %s", str);

	Graphics::BlendAlpha alphamode = Graphics::BLENDALPHA_MULTIPLY;
	if (!lua_isnoneornil(L, 2))
	{
		const char *alphastr = luaL_checkstring(L, 2);
		if (!Graphics::getConstant(alphastr, alphamode))
			return luaL_error(L, "Invalid blend alpha mode: %s", alphastr);
	}

	luax_catchexcept(L, [&](){ instance()->setBlendMode(mode, alphamode); });
	return 0;
}

int w_getBlendMode(lua_State *L)
{
	const char *str;
	const char *alphastr;

	Graphics::BlendAlpha alphamode;
	Graphics::BlendMode mode = instance()->getBlendMode(alphamode);

	if (!Graphics::getConstant(mode, str))
		return luaL_error(L, "Unknown blend mode");

	if (!Graphics::getConstant(alphamode, alphastr))
		return luaL_error(L, "Unknown blend alpha mode");

	lua_pushstring(L, str);
	lua_pushstring(L, alphastr);
	return 2;
}

int w_setDefaultFilter(lua_State *L)
{
	Texture::Filter f;

	const char *minstr = luaL_checkstring(L, 1);
	const char *magstr = luaL_optstring(L, 2, minstr);

	if (!Texture::getConstant(minstr, f.min))
		return luaL_error(L, "Invalid filter mode: %s", minstr);
	if (!Texture::getConstant(magstr, f.mag))
		return luaL_error(L, "Invalid filter mode: %s", magstr);

	f.anisotropy = (float) luaL_optnumber(L, 3, 1.0);

	instance()->setDefaultFilter(f);

	return 0;
}

int w_getDefaultFilter(lua_State *L)
{
	const Texture::Filter &f = instance()->getDefaultFilter();
	const char *minstr;
	const char *magstr;
	if (!Texture::getConstant(f.min, minstr))
		return luaL_error(L, "Unknown minification filter mode");
	if (!Texture::getConstant(f.mag, magstr))
		return luaL_error(L, "Unknown magnification filter mode");
	lua_pushstring(L, minstr);
	lua_pushstring(L, magstr);
	lua_pushnumber(L, f.anisotropy);
	return 3;
}

int w_setDefaultMipmapFilter(lua_State *L)
{
	Texture::FilterMode filter = Texture::FILTER_NONE;
	if (!lua_isnoneornil(L, 1))
	{
		const char *str = luaL_checkstring(L, 1);
		if (!Texture::getConstant(str, filter))
			return luaL_error(L, "Invalid filter mode: %s", str);
	}

	float sharpness = (float) luaL_optnumber(L, 2, 0);

	instance()->setDefaultMipmapFilter(filter, sharpness);

	return 0;
}

int w_getDefaultMipmapFilter(lua_State *L)
{
	Texture::FilterMode filter;
	float sharpness;

	instance()->getDefaultMipmapFilter(&filter, &sharpness);

	const char *str;
	if (Texture::getConstant(filter, str))
		lua_pushstring(L, str);
	else
		lua_pushnil(L);

	lua_pushnumber(L, sharpness);

	return 2;
}

int w_setLineWidth(lua_State *L)
{
	float width = (float)luaL_checknumber(L, 1);
	instance()->setLineWidth(width);
	return 0;
}

int w_setLineStyle(lua_State *L)
{
	Graphics::LineStyle style;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, style))
		return luaL_error(L, "Invalid line style: %s", str);

	instance()->setLineStyle(style);
	return 0;
}

int w_setLineJoin(lua_State *L)
{
	Graphics::LineJoin join;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, join))
		return luaL_error(L, "Invalid line join mode: %s", str);

	instance()->setLineJoin(join);
	return 0;
}

int w_getLineWidth(lua_State *L)
{
	lua_pushnumber(L, instance()->getLineWidth());
	return 1;
}

int w_getLineStyle(lua_State *L)
{
	Graphics::LineStyle style = instance()->getLineStyle();
	const char *str;
	if (!Graphics::getConstant(style, str))
		return luaL_error(L, "Unknown line style");
	lua_pushstring(L, str);
	return 1;
}

int w_getLineJoin(lua_State *L)
{
	Graphics::LineJoin join = instance()->getLineJoin();
	const char *str;
	if (!Graphics::getConstant(join, str))
		return luaL_error(L, "Unknown line join");
	lua_pushstring(L, str);
	return 1;
}

int w_setPointSize(lua_State *L)
{
	float size = (float)luaL_checknumber(L, 1);
	instance()->setPointSize(size);
	return 0;
}

int w_getPointSize(lua_State *L)
{
	lua_pushnumber(L, instance()->getPointSize());
	return 1;
}

int w_setWireframe(lua_State *L)
{
	instance()->setWireframe(luax_toboolean(L, 1));
	return 0;
}

int w_isWireframe(lua_State *L)
{
	luax_pushboolean(L, instance()->isWireframe());
	return 1;
}

int w_setShader(lua_State *L)
{
	if (lua_isnoneornil(L,1))
	{
		instance()->setShader();
		return 0;
	}

	Shader *shader = luax_checkshader(L, 1);
	instance()->setShader(shader);
	return 0;
}

int w_getShader(lua_State *L)
{
	Shader *shader = instance()->getShader();
	if (shader)
		luax_pushtype(L, shader);
	else
		lua_pushnil(L);

	return 1;
}

int w_setDefaultShaderCode(lua_State *L)
{
	for (int i = 0; i < 2; i++)
	{
		luaL_checktype(L, i + 1, LUA_TTABLE);

		for (int lang = 0; lang < Shader::LANGUAGE_MAX_ENUM; lang++)
		{
			const char *langname;
			if (!Shader::getConstant((Shader::Language) lang, langname))
				continue;

			lua_getfield(L, i + 1, langname);

			lua_getfield(L, -1, "vertex");
			lua_getfield(L, -2, "pixel");
			lua_getfield(L, -3, "videopixel");
			lua_getfield(L, -4, "arraypixel");

			Shader::ShaderSource code;
			code.vertex = luax_checkstring(L, -4);
			code.pixel = luax_checkstring(L, -3);

			Shader::ShaderSource videocode;
			videocode.vertex = luax_checkstring(L, -4);
			videocode.pixel = luax_checkstring(L, -2);

			Shader::ShaderSource arraycode;
			arraycode.vertex = luax_checkstring(L, -4);
			arraycode.pixel = luax_checkstring(L, -1);

			lua_pop(L, 5);

			Graphics::defaultShaderCode[Shader::STANDARD_DEFAULT][lang][i] = code;
			Graphics::defaultShaderCode[Shader::STANDARD_VIDEO][lang][i] = videocode;
			Graphics::defaultShaderCode[Shader::STANDARD_ARRAY][lang][i] = arraycode;
		}
	}

	return 0;
}

int w_getSupported(lua_State *L)
{
	lua_createtable(L, 0, (int) Graphics::FEATURE_MAX_ENUM);

	for (int i = 0; i < (int) Graphics::FEATURE_MAX_ENUM; i++)
	{
		auto feature = (Graphics::Feature) i;
		const char *name = nullptr;

		if (!Graphics::getConstant(feature, name))
			continue;

		luax_pushboolean(L, instance()->isSupported(feature));
		lua_setfield(L, -2, name);
	}

	return 1;
}

static int w__getFormats(lua_State *L, bool (*isFormatSupported)(PixelFormat), bool (*ignore)(PixelFormat))
{
	lua_createtable(L, 0, (int) PIXELFORMAT_MAX_ENUM);

	for (int i = 0; i < (int) PIXELFORMAT_MAX_ENUM; i++)
	{
		PixelFormat format = (PixelFormat) i;
		const char *name = nullptr;

		if (format == PIXELFORMAT_UNKNOWN || !love::getConstant(format, name) || ignore(format))
			continue;

		luax_pushboolean(L, isFormatSupported(format));
		lua_setfield(L, -2, name);
	}

	return 1;
}

int w_getCanvasFormats(lua_State *L)
{
	bool (*supported)(PixelFormat);

	if (!lua_isnoneornil(L, 1))
	{
		luaL_checktype(L, 1, LUA_TBOOLEAN);

		if (luax_toboolean(L, 1))
		{
			supported = [](PixelFormat format) -> bool
			{
				return instance()->isCanvasFormatSupported(format, true);
			};
		}
		else
		{
			supported = [](PixelFormat format) -> bool
			{
				return instance()->isCanvasFormatSupported(format, false);
			};
		}
	}
	else
	{
		supported = [](PixelFormat format) -> bool
		{
			return instance()->isCanvasFormatSupported(format);
		};
	}

	return w__getFormats(L, supported, isPixelFormatCompressed);
}

int w_getImageFormats(lua_State *L)
{
	const auto supported = [](PixelFormat format) -> bool
	{
		return instance()->isImageFormatSupported(format);
	};

	const auto ignore = [](PixelFormat format) -> bool
	{
		return !(image::ImageData::validPixelFormat(format) || isPixelFormatCompressed(format));
	};

	return w__getFormats(L, supported, ignore);
}

int w_getRendererInfo(lua_State *L)
{
	Graphics::RendererInfo info;
	luax_catchexcept(L, [&](){ info = instance()->getRendererInfo(); });

	luax_pushstring(L, info.name);
	luax_pushstring(L, info.version);
	luax_pushstring(L, info.vendor);
	luax_pushstring(L, info.device);
	return 4;
}

int w_getSystemLimits(lua_State *L)
{
	lua_createtable(L, 0, (int) Graphics::LIMIT_MAX_ENUM);

	for (int i = 0; i < (int) Graphics::LIMIT_MAX_ENUM; i++)
	{
		Graphics::SystemLimit limittype = (Graphics::SystemLimit) i;
		const char *name = nullptr;

		if (!Graphics::getConstant(limittype, name))
			continue;

		lua_pushnumber(L, instance()->getSystemLimit(limittype));
		lua_setfield(L, -2, name);
	}

	return 1;
}

int w_getStats(lua_State *L)
{
	Graphics::Stats stats = instance()->getStats();

	lua_createtable(L, 0, 7);

	lua_pushinteger(L, stats.drawCalls);
	lua_setfield(L, -2, "drawcalls");

	lua_pushinteger(L, stats.canvasSwitches);
	lua_setfield(L, -2, "canvasswitches");

	lua_pushinteger(L, stats.shaderSwitches);
	lua_setfield(L, -2, "shaderswitches");

	lua_pushinteger(L, stats.canvases);
	lua_setfield(L, -2, "canvases");

	lua_pushinteger(L, stats.images);
	lua_setfield(L, -2, "images");

	lua_pushinteger(L, stats.fonts);
	lua_setfield(L, -2, "fonts");

	lua_pushinteger(L, stats.textureMemory);
	lua_setfield(L, -2, "texturememory");

	return 1;
}

int w_draw(lua_State *L)
{
	Drawable *drawable = nullptr;
	Texture *texture = nullptr;
	Quad *quad = nullptr;
	int startidx = 2;

	if (luax_istype(L, 2, Quad::type))
	{
		texture = luax_checktexture(L, 1);
		quad = luax_totype<Quad>(L, 2);
		startidx = 3;
	}
	else if (lua_isnil(L, 2) && !lua_isnoneornil(L, 3))
	{
		return luax_typerror(L, 2, "Quad");
	}
	else
	{
		drawable = luax_checktype<Drawable>(L, 1);
		startidx = 2;
	}

	luax_checkstandardtransform(L, startidx, [&](const Matrix4 &m)
	{
		luax_catchexcept(L, [&]()
		{
			if (texture && quad)
				instance()->draw(texture, quad, m);
			else
				instance()->draw(drawable, m);
		});
	});

	return 0;
}

int w_drawLayer(lua_State *L)
{
	Texture *texture = luax_checktexture(L, 1);
	Quad *quad = nullptr;
	int layer = (int) luaL_checknumber(L, 2) - 1;
	int startidx = 3;

	if (luax_istype(L, startidx, Quad::type))
	{
		texture = luax_checktexture(L, 1);
		quad = luax_totype<Quad>(L, startidx);
		startidx++;
	}
	else if (lua_isnil(L, startidx) && !lua_isnoneornil(L, startidx + 1))
	{
		return luax_typerror(L, startidx, "Quad");
	}

	luax_checkstandardtransform(L, startidx, [&](const Matrix4 &m)
	{
		luax_catchexcept(L, [&]()
		{
			if (quad)
				instance()->drawLayer(texture, layer, quad, m);
			else
				instance()->drawLayer(texture, layer, m);
		});
	});

	return 0;
}

int w_drawInstanced(lua_State *L)
{
	Mesh *t = luax_checkmesh(L, 1);
	int instancecount = (int) luaL_checkinteger(L, 2);

	luax_checkstandardtransform(L, 3, [&](const Matrix4 &m)
	{
		luax_catchexcept(L, [&]() { instance()->drawInstanced(t, m, instancecount); });
	});

	return 0;
}

int w_print(lua_State *L)
{
	std::vector<Font::ColoredString> str;
	luax_checkcoloredstring(L, 1, str);

	if (luax_istype(L, 2, Font::type))
	{
		Font *font = luax_checkfont(L, 2);

		luax_checkstandardtransform(L, 3, [&](const Matrix4 &m)
		{
			luax_catchexcept(L, [&](){ instance()->print(str, font, m); });
		});
	}
	else
	{
		luax_checkstandardtransform(L, 2, [&](const Matrix4 &m)
		{
			luax_catchexcept(L, [&](){ instance()->print(str, m); });
		});
	}

	return 0;
}

int w_printf(lua_State *L)
{
	std::vector<Font::ColoredString> str;
	luax_checkcoloredstring(L, 1, str);

	Font *font = nullptr;
	int startidx = 2;

	if (luax_istype(L, startidx, Font::type))
	{
		font = luax_checkfont(L, startidx);
		startidx++;
	}

	Font::AlignMode align = Font::ALIGN_LEFT;
	Matrix4 m;

	int formatidx = startidx + 2;

	if (luax_istype(L, startidx, math::Transform::type))
	{
		math::Transform *tf = luax_totype<math::Transform>(L, startidx);
		m = tf->getMatrix();
		formatidx = startidx + 1;
	}
	else
	{
		float x = (float)luaL_checknumber(L, startidx + 0);
		float y = (float)luaL_checknumber(L, startidx + 1);

		float angle = (float) luaL_optnumber(L, startidx + 4, 0.0f);
		float sx = (float) luaL_optnumber(L, startidx + 5, 1.0f);
		float sy = (float) luaL_optnumber(L, startidx + 6, sx);
		float ox = (float) luaL_optnumber(L, startidx + 7, 0.0f);
		float oy = (float) luaL_optnumber(L, startidx + 8, 0.0f);
		float kx = (float) luaL_optnumber(L, startidx + 9, 0.0f);
		float ky = (float) luaL_optnumber(L, startidx + 10, 0.0f);

		m = Matrix4(x, y, angle, sx, sy, ox, oy, kx, ky);
	}

	float wrap = (float)luaL_checknumber(L, formatidx);

	const char *astr = lua_isnoneornil(L, formatidx + 1) ? nullptr : luaL_checkstring(L, formatidx + 1);
	if (astr != nullptr && !Font::getConstant(astr, align))
		return luaL_error(L, "Incorrect alignment: %s", astr);

	if (font != nullptr)
		luax_catchexcept(L, [&](){ instance()->printf(str, font, wrap, align, m); });
	else
		luax_catchexcept(L, [&](){ instance()->printf(str, wrap, align, m); });

	return 0;
}

int w_points(lua_State *L)
{
	// love.graphics.points has 3 variants:
	// - points(x1, y1, x2, y2, ...)
	// - points({x1, y1, x2, y2, ...})
	// - points({{x1, y1 [, r, g, b, a]}, {x2, y2 [, r, g, b, a]}, ...})

	int args = lua_gettop(L);
	bool is_table = false;
	bool is_table_of_tables = false;
	if (args == 1 && lua_istable(L, 1))
	{
		is_table = true;
		args = (int) luax_objlen(L, 1);

		lua_rawgeti(L, 1, 1);
		is_table_of_tables = lua_istable(L, -1);
		lua_pop(L, 1);
	}

	if (args % 2 != 0 && !is_table_of_tables)
		return luaL_error(L, "Number of vertex components must be a multiple of two");

	int numpoints = args / 2;
	if (is_table_of_tables)
		numpoints = args;

	float *coords = nullptr;
	Colorf *colors = nullptr;

	if (is_table_of_tables)
	{
		size_t datasize = (sizeof(float) * 2 + sizeof(Colorf)) * numpoints;
		uint8 *data = instance()->getScratchBuffer<uint8>(datasize);

		coords = (float *) data;
		colors = (Colorf *) (data + sizeof(float) * numpoints * 2);
	}
	else
		coords = instance()->getScratchBuffer<float>(numpoints * 2);

	if (is_table)
	{
		if (is_table_of_tables)
		{
			// points({{x1, y1 [, r, g, b, a]}, {x2, y2 [, r, g, b, a]}, ...})
			for (int i = 0; i < args; i++)
			{
				lua_rawgeti(L, 1, i + 1);
				for (int j = 1; j <= 6; j++)
					lua_rawgeti(L, -j, j);

				coords[i * 2 + 0] = luax_tofloat(L, -6);
				coords[i * 2 + 1] = luax_tofloat(L, -5);

				colors[i].r = luaL_optnumber(L, -4, 1.0);
				colors[i].g = luaL_optnumber(L, -3, 1.0);
				colors[i].b = luaL_optnumber(L, -2, 1.0);
				colors[i].a = luaL_optnumber(L, -1, 1.0);

				lua_pop(L, 7);
			}
		}
		else
		{
			// points({x1, y1, x2, y2, ...})
			for (int i = 0; i < args; i++)
			{
				lua_rawgeti(L, 1, i + 1);
				coords[i] = luax_tofloat(L, -1);
				lua_pop(L, 1);
			}
		}
	}
	else
	{
		for (int i = 0; i < args; i++)
			coords[i] = luax_tofloat(L, i + 1);
	}

	luax_catchexcept(L, [&](){ instance()->points(coords, colors, numpoints); });
	return 0;
}

int w_line(lua_State *L)
{
	int args = lua_gettop(L);
	bool is_table = false;
	if (args == 1 && lua_istable(L, 1))
	{
		args = (int) luax_objlen(L, 1);
		is_table = true;
	}

	if (args % 2 != 0)
		return luaL_error(L, "Number of vertex components must be a multiple of two");
	else if (args < 4)
		return luaL_error(L, "Need at least two vertices to draw a line");

	float *coords = instance()->getScratchBuffer<float>(args);
	if (is_table)
	{
		for (int i = 0; i < args; ++i)
		{
			lua_rawgeti(L, 1, i + 1);
			coords[i] = luax_tofloat(L, -1);
			lua_pop(L, 1);
		}
	}
	else
	{
		for (int i = 0; i < args; ++i)
			coords[i] = luax_tofloat(L, i + 1);
	}

	luax_catchexcept(L,
		[&](){ instance()->polyline(coords, args); }
	);

	return 0;
}

int w_rectangle(lua_State *L)
{
	Graphics::DrawMode mode;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, mode))
		return luaL_error(L, "Invalid draw mode: %s", str);

	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	float w = (float)luaL_checknumber(L, 4);
	float h = (float)luaL_checknumber(L, 5);

	if (lua_isnoneornil(L, 6))
	{
		instance()->rectangle(mode, x, y, w, h);
		return 0;
	}

	float rx = (float)luaL_optnumber(L, 6, 0.0);
	float ry = (float)luaL_optnumber(L, 7, rx);

	if (lua_isnoneornil(L, 8))
		luax_catchexcept(L, [&](){ instance()->rectangle(mode, x, y, w, h, rx, ry); });
	else
	{
		int points = (int) luaL_checknumber(L, 8);
		luax_catchexcept(L, [&](){ instance()->rectangle(mode, x, y, w, h, rx, ry, points); });
	}

	return 0;
}

int w_circle(lua_State *L)
{
	Graphics::DrawMode mode;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, mode))
		return luaL_error(L, "Invalid draw mode: %s", str);

	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	float radius = (float)luaL_checknumber(L, 4);

	if (lua_isnoneornil(L, 5))
		luax_catchexcept(L, [&](){ instance()->circle(mode, x, y, radius); });
	else
	{
		int points = (int) luaL_checknumber(L, 5);
		luax_catchexcept(L, [&](){ instance()->circle(mode, x, y, radius, points); });
	}

	return 0;
}

int w_ellipse(lua_State *L)
{
	Graphics::DrawMode mode;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, mode))
		return luaL_error(L, "Invalid draw mode: %s", str);

	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	float a = (float)luaL_checknumber(L, 4);
	float b = (float)luaL_optnumber(L, 5, a);

	if (lua_isnoneornil(L, 6))
		luax_catchexcept(L, [&](){ instance()->ellipse(mode, x, y, a, b); });
	else
	{
		int points = (int) luaL_checknumber(L, 6);
		luax_catchexcept(L, [&](){ instance()->ellipse(mode, x, y, a, b, points); });
	}

	return 0;
}

int w_arc(lua_State *L)
{
	Graphics::DrawMode drawmode;
	const char *drawstr = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(drawstr, drawmode))
		return luaL_error(L, "Invalid draw mode: %s", drawstr);

	int startidx = 2;

	Graphics::ArcMode arcmode = Graphics::ARC_PIE;

	if (lua_type(L, 2) == LUA_TSTRING)
	{
		const char *arcstr = luaL_checkstring(L, 2);
		if (!Graphics::getConstant(arcstr, arcmode))
			return luaL_error(L, "Invalid arc mode: %s", arcstr);

		startidx = 3;
	}

	float x = (float) luaL_checknumber(L, startidx + 0);
	float y = (float) luaL_checknumber(L, startidx + 1);
	float radius = (float) luaL_checknumber(L, startidx + 2);
	float angle1 = (float) luaL_checknumber(L, startidx + 3);
	float angle2 = (float) luaL_checknumber(L, startidx + 4);

	if (lua_isnoneornil(L, startidx + 5))
		luax_catchexcept(L, [&](){ instance()->arc(drawmode, arcmode, x, y, radius, angle1, angle2); });
	else
	{
		int points = (int) luaL_checknumber(L, startidx + 5);
		luax_catchexcept(L, [&](){ instance()->arc(drawmode, arcmode, x, y, radius, angle1, angle2, points); });
	}

	return 0;
}

int w_polygon(lua_State *L)
{
	int args = lua_gettop(L) - 1;

	Graphics::DrawMode mode;
	const char *str = luaL_checkstring(L, 1);
	if (!Graphics::getConstant(str, mode))
		return luaL_error(L, "Invalid draw mode: %s", str);

	bool is_table = false;
	if (args == 1 && lua_istable(L, 2))
	{
		args = (int) luax_objlen(L, 2);
		is_table = true;
	}

	if (args % 2 != 0)
		return luaL_error(L, "Number of vertex components must be a multiple of two");
	else if (args < 6)
		return luaL_error(L, "Need at least three vertices to draw a polygon");

	// fetch coords
	float *coords = instance()->getScratchBuffer<float>(args + 2);
	if (is_table)
	{
		for (int i = 0; i < args; ++i)
		{
			lua_rawgeti(L, 2, i + 1);
			coords[i] = luax_tofloat(L, -1);
			lua_pop(L, 1);
		}
	}
	else
	{
		for (int i = 0; i < args; ++i)
			coords[i] = luax_tofloat(L, i + 2);
	}

	// make a closed loop
	coords[args]   = coords[0];
	coords[args+1] = coords[1];

	luax_catchexcept(L, [&](){ instance()->polygon(mode, coords, args+2); });
	return 0;
}

int w_flushBatch(lua_State *)
{
	instance()->flushStreamDraws();
	return 0;
}

int w_push(lua_State *L)
{
	Graphics::StackType stype = Graphics::STACK_TRANSFORM;
	const char *sname = lua_isnoneornil(L, 1) ? nullptr : luaL_checkstring(L, 1);
	if (sname && !Graphics::getConstant(sname, stype))
		return luaL_error(L, "Invalid graphics stack type: %s", sname);

	luax_catchexcept(L, [&](){ instance()->push(stype); });

	if (luax_istype(L, 2, math::Transform::type))
	{
		math::Transform *t = luax_totype<math::Transform>(L, 2);
		luax_catchexcept(L, [&]() { instance()->applyTransform(t); });
	}

	return 0;
}

int w_pop(lua_State *L)
{
	luax_catchexcept(L, [&](){ instance()->pop(); });
	return 0;
}

int w_rotate(lua_State *L)
{
	float angle = (float)luaL_checknumber(L, 1);
	instance()->rotate(angle);
	return 0;
}

int w_scale(lua_State *L)
{
	float sx = (float)luaL_optnumber(L, 1, 1.0f);
	float sy = (float)luaL_optnumber(L, 2, sx);
	instance()->scale(sx, sy);
	return 0;
}

int w_translate(lua_State *L)
{
	float x = (float)luaL_checknumber(L, 1);
	float y = (float)luaL_checknumber(L, 2);
	instance()->translate(x, y);
	return 0;
}

int w_shear(lua_State *L)
{
	float kx = (float)luaL_checknumber(L, 1);
	float ky = (float)luaL_checknumber(L, 2);
	instance()->shear(kx, ky);
	return 0;
}

int w_origin(lua_State * /*L*/)
{
	instance()->origin();
	return 0;
}

int w_applyTransform(lua_State *L)
{
	math::Transform *t = math::luax_checktransform(L, 1);
	luax_catchexcept(L, [&]() { instance()->applyTransform(t); });
	return 0;
}

int w_replaceTransform(lua_State *L)
{
	math::Transform *t = math::luax_checktransform(L, 1);
	luax_catchexcept(L, [&]() { instance()->replaceTransform(t); });
	return 0;
}

int w_transformPoint(lua_State *L)
{
	Vector p;
	p.x = (float) luaL_checknumber(L, 1);
	p.y = (float) luaL_checknumber(L, 2);
	p = instance()->transformPoint(p);
	lua_pushnumber(L, p.x);
	lua_pushnumber(L, p.y);
	return 2;
}

int w_inverseTransformPoint(lua_State *L)
{
	Vector p;
	p.x = (float) luaL_checknumber(L, 1);
	p.y = (float) luaL_checknumber(L, 2);
	p = instance()->inverseTransformPoint(p);
	lua_pushnumber(L, p.x);
	lua_pushnumber(L, p.y);
	return 2;
}


// List of functions to wrap.
static const luaL_Reg functions[] =
{
	{ "reset", w_reset },
	{ "clear", w_clear },
	{ "discard", w_discard },
	{ "present", w_present },

	{ "newImage", w_newImage },
	{ "newArrayImage", w_newArrayImage },
	{ "newVolumeImage", w_newVolumeImage },
	{ "newCubeImage", w_newCubeImage },
	{ "newQuad", w_newQuad },
	{ "newFont", w_newFont },
	{ "newImageFont", w_newImageFont },
	{ "newSpriteBatch", w_newSpriteBatch },
	{ "newParticleSystem", w_newParticleSystem },
	{ "newCanvas", w_newCanvas },
	{ "newShader", w_newShader },
	{ "newMesh", w_newMesh },
	{ "newText", w_newText },
	{ "_newVideo", w_newVideo },

	{ "validateShader", w_validateShader },

	{ "setCanvas", w_setCanvas },
	{ "getCanvas", w_getCanvas },

	{ "setColor", w_setColor },
	{ "getColor", w_getColor },
	{ "setBackgroundColor", w_setBackgroundColor },
	{ "getBackgroundColor", w_getBackgroundColor },

	{ "setNewFont", w_setNewFont },
	{ "setFont", w_setFont },
	{ "getFont", w_getFont },

	{ "setColorMask", w_setColorMask },
	{ "getColorMask", w_getColorMask },
	{ "setBlendMode", w_setBlendMode },
	{ "getBlendMode", w_getBlendMode },
	{ "setDefaultFilter", w_setDefaultFilter },
	{ "getDefaultFilter", w_getDefaultFilter },
	{ "setDefaultMipmapFilter", w_setDefaultMipmapFilter },
	{ "getDefaultMipmapFilter", w_getDefaultMipmapFilter },
	{ "setLineWidth", w_setLineWidth },
	{ "setLineStyle", w_setLineStyle },
	{ "setLineJoin", w_setLineJoin },
	{ "getLineWidth", w_getLineWidth },
	{ "getLineStyle", w_getLineStyle },
	{ "getLineJoin", w_getLineJoin },
	{ "setPointSize", w_setPointSize },
	{ "getPointSize", w_getPointSize },
	{ "setWireframe", w_setWireframe },
	{ "isWireframe", w_isWireframe },

	{ "setShader", w_setShader },
	{ "getShader", w_getShader },
	{ "_setDefaultShaderCode", w_setDefaultShaderCode },

	{ "getSupported", w_getSupported },
	{ "getCanvasFormats", w_getCanvasFormats },
	{ "getImageFormats", w_getImageFormats },
	{ "getRendererInfo", w_getRendererInfo },
	{ "getSystemLimits", w_getSystemLimits },
	{ "getStats", w_getStats },

	{ "captureScreenshot", w_captureScreenshot },

	{ "draw", w_draw },
	{ "drawLayer", w_drawLayer },
	{ "drawInstanced", w_drawInstanced },

	{ "print", w_print },
	{ "printf", w_printf },

	{ "isCreated", w_isCreated },
	{ "isActive", w_isActive },
	{ "isGammaCorrect", w_isGammaCorrect },
	{ "getWidth", w_getWidth },
	{ "getHeight", w_getHeight },
	{ "getDimensions", w_getDimensions },
	{ "getPixelWidth", w_getPixelWidth },
	{ "getPixelHeight", w_getPixelHeight },
	{ "getPixelDimensions", w_getPixelDimensions },
	{ "getPixelDensity", w_getPixelDensity },

	{ "setScissor", w_setScissor },
	{ "intersectScissor", w_intersectScissor },
	{ "getScissor", w_getScissor },

	{ "stencil", w_stencil },
	{ "setStencilTest", w_setStencilTest },
	{ "getStencilTest", w_getStencilTest },

	{ "points", w_points },
	{ "line", w_line },
	{ "rectangle", w_rectangle },
	{ "circle", w_circle },
	{ "ellipse", w_ellipse },
	{ "arc", w_arc },
	{ "polygon", w_polygon },

	{ "flushBatch", w_flushBatch },

	{ "push", w_push },
	{ "pop", w_pop },
	{ "rotate", w_rotate },
	{ "scale", w_scale },
	{ "translate", w_translate },
	{ "shear", w_shear },
	{ "origin", w_origin },
	{ "applyTransform", w_applyTransform },
	{ "replaceTransform", w_replaceTransform },
	{ "transformPoint", w_transformPoint },
	{ "inverseTransformPoint", w_inverseTransformPoint },

	{ 0, 0 }
};

static int luaopen_drawable(lua_State *L)
{
	return luax_register_type(L, &Drawable::type, nullptr);
}

// Types for this module.
static const lua_CFunction types[] =
{
	luaopen_drawable,
	luaopen_texture,
	luaopen_font,
	luaopen_image,
	luaopen_quad,
	luaopen_spritebatch,
	luaopen_particlesystem,
	luaopen_canvas,
	luaopen_shader,
	luaopen_mesh,
	luaopen_text,
	luaopen_video,
	0
};

extern "C" int luaopen_love_graphics(lua_State *L)
{
	Graphics *instance = instance();
	if (instance == nullptr)
	{
		luax_catchexcept(L, [&](){ instance = new love::graphics::opengl::Graphics(); });
	}
	else
		instance->retain();

	WrappedModule w;
	w.module = instance;
	w.name = "graphics";
	w.type = &Graphics::type;
	w.functions = functions;
	w.types = types;

	int n = luax_register_module(L, w);

	if (luaL_loadbuffer(L, (const char *)graphics_lua, sizeof(graphics_lua), "wrap_Graphics.lua") == 0)
		lua_call(L, 0, 0);
	else
		lua_error(L);

	return n;
}

} // graphics
} // love