/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d10.hpp"
#include "render_d3d10_utils.hpp"
#include <algorithm>

reshade::d3d10::device_impl::device_impl(ID3D10Device1 *device) :
	api_object_impl(device)
{
#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::d3d10::device_impl::~device_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif
}

bool reshade::d3d10::device_impl::check_format_support(uint32_t format, api::resource_usage usage) const
{
	if ((usage & api::resource_usage::unordered_access) != 0)
		return false;

	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(static_cast<DXGI_FORMAT>(format), &support)))
		return false;

	if ((usage & api::resource_usage::render_target) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::depth_stencil) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_SHADER_SAMPLE) == 0)
		return false;
	if ((usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d10::device_impl::check_resource_handle_valid(api::resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D10Resource *>(resource.handle));
}
bool reshade::d3d10::device_impl::check_resource_view_handle_valid(api::resource_view_handle view) const
{
	return view.handle != 0 && _views.has_object(reinterpret_cast<ID3D10View *>(view.handle));
}

bool reshade::d3d10::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler_handle *out)
{
	D3D10_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	ID3D10SamplerState *object = nullptr;
	if (SUCCEEDED(_orig->CreateSamplerState(&internal_desc, &object)))
	{
		*out = { reinterpret_cast<uintptr_t>(object) };
		return true;
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d10::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource_handle *out)
{
	static_assert(sizeof(api::subresource_data) == sizeof(D3D10_SUBRESOURCE_DATA));

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			D3D10_BUFFER_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			ID3D10Buffer *object = nullptr;
			if (SUCCEEDED(_orig->CreateBuffer(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		{
			D3D10_TEXTURE1D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			ID3D10Texture1D *object = nullptr;
			if (SUCCEEDED(_orig->CreateTexture1D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_2d:
		{
			D3D10_TEXTURE2D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			ID3D10Texture2D *object = nullptr;
			if (SUCCEEDED(_orig->CreateTexture2D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			D3D10_TEXTURE3D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			ID3D10Texture3D *object = nullptr;
			if (SUCCEEDED(_orig->CreateTexture3D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d10::device_impl::create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out)
{
	assert(resource.handle != 0);

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			ID3D10DepthStencilView *object = nullptr;
			if (SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_usage::render_target:
		{
			D3D10_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			ID3D10RenderTargetView *object = nullptr;
			if (SUCCEEDED(_orig->CreateRenderTargetView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_usage::shader_resource:
		{
			D3D10_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			ID3D10ShaderResourceView *object = nullptr;
			if (SUCCEEDED(_orig->CreateShaderResourceView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}

void reshade::d3d10::device_impl::destroy_sampler(api::sampler_handle sampler)
{
	assert(sampler.handle != 0);
	reinterpret_cast<ID3D10SamplerState *>(sampler.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_resource(api::resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D10Resource *>(resource.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_resource_view(api::resource_view_handle view)
{
	assert(view.handle != 0);
	reinterpret_cast<ID3D10View *>(view.handle)->Release();
}

void reshade::d3d10::device_impl::get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const
{
	assert(view.handle != 0);
	com_ptr<ID3D10Resource> resource;
	reinterpret_cast<ID3D10View *>(view.handle)->GetResource(&resource);

	*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
}

reshade::api::resource_desc reshade::d3d10::device_impl::get_resource_desc(api::resource_handle resource) const
{
	assert(resource.handle != 0);
	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
		default:
		{
			assert(false); // Not implemented
			return api::resource_desc {};
		}
		case D3D10_RESOURCE_DIMENSION_BUFFER:
		{
			D3D10_BUFFER_DESC internal_desc;
			static_cast<ID3D10Buffer *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D10_TEXTURE1D_DESC internal_desc;
			static_cast<ID3D10Texture1D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D10_TEXTURE2D_DESC internal_desc;
			static_cast<ID3D10Texture2D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D10_TEXTURE3D_DESC internal_desc;
			static_cast<ID3D10Texture3D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
	}
}

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}

void reshade::d3d10::device_impl::blit(api::resource_handle, uint32_t, const int32_t[6], api::resource_handle, uint32_t, const int32_t[6], api::texture_filter)
{
	assert(false);
}
void reshade::d3d10::device_impl::resolve(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset == nullptr && dst_offset == nullptr && size == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D10Resource *>(src.handle), src_subresource, static_cast<DXGI_FORMAT>(format));
}
void reshade::d3d10::device_impl::copy_resource(api::resource_handle src, api::resource_handle dst)
{
	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D10Resource *>(dst.handle), reinterpret_cast<ID3D10Resource *>(src.handle));
}
void reshade::d3d10::device_impl::copy_buffer_region(api::resource_handle src, uint64_t src_offset, api::resource_handle dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d10::device_impl::copy_buffer_to_texture(api::resource_handle, uint64_t, uint32_t, uint32_t, api::resource_handle, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d10::device_impl::copy_texture_region(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	assert(src.handle != 0 && dst.handle != 0);

	D3D10_BOX src_box;
	if (src_offset != nullptr)
	{
		src_box.left = src_offset[0];
		src_box.top = src_offset[1];
		src_box.front = src_offset[2];
	}
	else
	{
		src_box.left = 0;
		src_box.top = 0;
		src_box.front = 0;
	}

	if (size != nullptr)
	{
		src_box.right = src_box.left + size[0];
		src_box.bottom = src_box.top + size[1];
		src_box.back = src_box.front + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(src);
		src_box.right = src_box.left + std::max(1u, desc.width >> (src_subresource % desc.levels));
		src_box.bottom = src_box.top + std::max(1u, desc.height >> (src_subresource % desc.levels));
		src_box.back = src_box.front + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.depth_or_layers) >> (src_subresource % desc.levels)) : 1u);
	}

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), src_subresource, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0, dst_offset != nullptr ? dst_offset[2] : 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), dst_subresource, &src_box);
}
void reshade::d3d10::device_impl::copy_texture_to_buffer(api::resource_handle, uint32_t, const int32_t[6], api::resource_handle, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d10::device_impl::clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
}
void reshade::d3d10::device_impl::clear_render_target_views(uint32_t count, const api::resource_view_handle *rtvs, const float color[4])
{
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ClearRenderTargetView(reinterpret_cast<ID3D10RenderTargetView *>(rtvs[i].handle), color);
	}
}
