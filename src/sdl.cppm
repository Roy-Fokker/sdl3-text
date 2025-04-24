module;

export module sdl;

import std;
import io;

namespace rg = std::ranges;
namespace vw = std::views;

export namespace sdl
{
	// If we are building in DEBUG mode, use this variable to enable extra messages from SDL
	constexpr auto IS_DEBUG = bool{
#ifdef DEBUG
		true
#endif
	};

	constexpr auto MAX_ANISOTROPY = float{ 16 };

	class sdl_base
	{
	public:
		sdl_base()
		{
			auto result = SDL_Init(SDL_INIT_VIDEO);
			assert(result and "SDL could not be initialized.");

			result = TTF_Init();
			assert(result and "SDL TTF could not be initialized.");
		}

		~sdl_base()
		{
			TTF_Quit();
			SDL_Quit();
		}
	};

	namespace type
	{
		// Deleter template, for use with SDL objects.
		// Allows use of SDL Objects with C++'s smart pointers, using SDL's destroy function
		template <auto fn>
		struct sdl_deleter
		{
			constexpr void operator()(auto *arg)
			{
				fn(arg);
			}
		};
		// Define SDL type with std::unique_ptr and custom deleter
		using window_ptr = std::unique_ptr<SDL_Window, sdl_deleter<SDL_DestroyWindow>>;

		// Define SDL TTF types with std::unique_ptr and custom deleter
		using ttf_font_ptr       = std::unique_ptr<TTF_Font, sdl_deleter<TTF_CloseFont>>;
		using ttf_textengine_ptr = std::unique_ptr<TTF_TextEngine, sdl_deleter<TTF_DestroyGPUTextEngine>>;
		using ttf_text_ptr       = std::unique_ptr<TTF_Text, sdl_deleter<TTF_DestroyText>>;

		// Special deleter for gpu.
		// it will release window on destruction
		struct gpu_window_deleter
		{
			SDL_Window *window;
			constexpr void operator()(auto *gpu)
			{
				SDL_ReleaseWindowFromGPUDevice(gpu, window);
				SDL_DestroyGPUDevice(gpu);
			}
		};
		// Define GPU type with std::unique_ptr and custom deleter
		using gpu_ptr = std::unique_ptr<SDL_GPUDevice, gpu_window_deleter>;

		// Deleter for all gpu objects in SDL
		template <auto fn>
		struct gpu_deleter
		{
			SDL_GPUDevice *gpu = nullptr;
			constexpr void operator()(auto *arg)
			{
				fn(gpu, arg);
			}
		};
		// Define SDL GPU types with std::unique_ptr and custom deleter
		using free_gfx_pipeline  = gpu_deleter<SDL_ReleaseGPUGraphicsPipeline>;
		using gfx_pipeline_ptr   = std::unique_ptr<SDL_GPUGraphicsPipeline, free_gfx_pipeline>;
		using free_comp_pipeline = gpu_deleter<SDL_ReleaseGPUComputePipeline>;
		using comp_pipeline_ptr  = std::unique_ptr<SDL_GPUComputePipeline, free_comp_pipeline>;
		using free_gfx_shader    = gpu_deleter<SDL_ReleaseGPUShader>;
		using gfx_shader_ptr     = std::unique_ptr<SDL_GPUShader, free_gfx_shader>;
		using free_gpu_buffer    = gpu_deleter<SDL_ReleaseGPUBuffer>;
		using gpu_buffer_ptr     = std::unique_ptr<SDL_GPUBuffer, free_gpu_buffer>;
		using free_gpu_texture   = gpu_deleter<SDL_ReleaseGPUTexture>;
		using gpu_texture_ptr    = std::unique_ptr<SDL_GPUTexture, free_gpu_texture>;
		using free_gpu_sampler   = gpu_deleter<SDL_ReleaseGPUSampler>;
		using gfx_sampler_ptr    = std::unique_ptr<SDL_GPUSampler, free_gpu_sampler>;
	}

	auto make_ttf_font(const std::filesystem::path &ttf_font_file, bool use_sdf) -> type::ttf_font_ptr
	{
		auto font = TTF_OpenFont(ttf_font_file.string().c_str(), 50);
		assert(font != nullptr and "Failed to open font file");

		TTF_SetFontSDF(font, use_sdf);
		TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_CENTER);

		return type::ttf_font_ptr{ font };
	}

	auto make_ttf_textengine(SDL_GPUDevice *gpu) -> type::ttf_textengine_ptr
	{
		auto engine = TTF_CreateGPUTextEngine(gpu);
		assert(engine != nullptr and "Failed to create GPU TTF Text Engine");

		return type::ttf_textengine_ptr{ engine };
	}

	auto make_ttf_text(TTF_TextEngine *engine, TTF_Font *font, const std::string_view text) -> type::ttf_text_ptr
	{
		auto txt = TTF_CreateText(engine, font, text.data(), text.size());
		assert(txt != nullptr and "Failed to create TTF text object");

		return type::ttf_text_ptr{ txt };
	}

	auto make_window(uint32_t width, uint32_t height, std::string_view title, SDL_WindowFlags flags = {}) -> type::window_ptr
	{
		auto window = SDL_CreateWindow(title.data(), static_cast<int>(width), static_cast<int>(height), flags);
		assert(window != nullptr and "Window could not be created.");

		// enable relative mouse movement
		SDL_SetWindowRelativeMouseMode(window, true);

		return type::window_ptr{ window };
	}

	// make_gpu does not take ownership of *wnd. *wnd must stay alive for duration of gpu_ptr.
	auto make_gpu(SDL_Window *wnd, SDL_GPUShaderFormat preferred_shader_format) -> type::gpu_ptr
	{
		auto gpu = SDL_CreateGPUDevice(preferred_shader_format, IS_DEBUG, NULL);
		assert(gpu != nullptr and "GPU device could not be created.");

		auto result = SDL_ClaimWindowForGPUDevice(gpu, wnd);
		assert(result == true and "Could not claim window for GPU.");

		return type::gpu_ptr{ gpu, { wnd } };
	}

	auto next_swapchain_image(SDL_Window *wnd, SDL_GPUCommandBuffer *cmd_buf) -> SDL_GPUTexture *
	{
		auto sc_tex = (SDL_GPUTexture *)nullptr;

		auto res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buf, wnd, &sc_tex, NULL, NULL);
		assert(res == true and "Wait and acquire GPU swapchain texture failed.");
		assert(sc_tex != nullptr and "Swapchain texture is null. Is window minimized?");

		return sc_tex;
	}

	auto make_gpu_buffer(SDL_GPUDevice *gpu, SDL_GPUBufferUsageFlags usage, uint32_t size, std::string_view debug_name) -> type::gpu_buffer_ptr
	{
		auto buffer_info = SDL_GPUBufferCreateInfo{
			.usage = usage,
			.size  = size,
		};

		auto buffer = SDL_CreateGPUBuffer(gpu, &buffer_info);
		assert(buffer != nullptr and "Failed to create gpu buffer");

		if (IS_DEBUG and debug_name.size() > 0)
		{
			SDL_SetGPUBufferName(gpu, buffer, debug_name.data());
		}

		return { buffer, { gpu } };
	}

	enum class shader_stage : uint8_t
	{
		invalid,
		vertex,
		fragment,
		compute,
	};

	// Does not convert shader_stage::compute
	// as graphics pipeline cannot use that value, and SDL does not provide SDL_GPUShaderStage value for it.
	auto to_sdl(shader_stage stage) -> SDL_GPUShaderStage
	{
		switch (stage)
		{
		case shader_stage::vertex:
			return SDL_GPU_SHADERSTAGE_VERTEX;
		case shader_stage::fragment:
			return SDL_GPU_SHADERSTAGE_FRAGMENT;
		}
		assert(false and "Unhandled shader stage");
		return {};
	}

	enum class raster_type : uint8_t
	{
		none_fill,
		none_wire,
		front_ccw_fill,
		front_ccw_wire,
		back_ccw_fill,
		back_ccw_wire,
		front_cw_fill,
		front_cw_wire,
		back_cw_fill,
		back_cw_wire,
	};

	auto to_sdl(raster_type type) -> SDL_GPURasterizerState
	{
		switch (type)
		{
			using enum raster_type;
		case none_fill:
			return {
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_NONE,
			};
		case none_wire:
			return {
				.fill_mode = SDL_GPU_FILLMODE_LINE,
				.cull_mode = SDL_GPU_CULLMODE_NONE,
			};
		case front_ccw_fill:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_FILL,
				.cull_mode  = SDL_GPU_CULLMODE_FRONT,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			};
		case front_ccw_wire:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_LINE,
				.cull_mode  = SDL_GPU_CULLMODE_FRONT,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			};
		case back_ccw_fill:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_FILL,
				.cull_mode  = SDL_GPU_CULLMODE_BACK,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			};
		case back_ccw_wire:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_LINE,
				.cull_mode  = SDL_GPU_CULLMODE_BACK,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			};
		case front_cw_fill:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_FILL,
				.cull_mode  = SDL_GPU_CULLMODE_FRONT,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
			};
		case front_cw_wire:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_LINE,
				.cull_mode  = SDL_GPU_CULLMODE_FRONT,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
			};
		case back_cw_fill:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_FILL,
				.cull_mode  = SDL_GPU_CULLMODE_BACK,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
			};
		case back_cw_wire:
			return {
				.fill_mode  = SDL_GPU_FILLMODE_LINE,
				.cull_mode  = SDL_GPU_CULLMODE_BACK,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
			};
		}
		assert(false and "Unhandled raster type");
		return {};
	}

	enum class blend_type : uint8_t
	{
		none,
		opaque,
		alpha,
		additive,
		non_premultiplied,
	};

	auto to_sdl(blend_type type) -> SDL_GPUColorTargetBlendState
	{
		// TODO: verify and fix blend ops for different types.

		auto src    = SDL_GPUBlendFactor{ SDL_GPU_BLENDFACTOR_ONE };
		auto dst    = SDL_GPUBlendFactor{ SDL_GPU_BLENDFACTOR_ONE };
		auto op     = SDL_GPUBlendOp{ SDL_GPU_BLENDOP_ADD };
		auto enable = false;

		switch (type)
		{
			using enum blend_type;
		case opaque:
			src = SDL_GPU_BLENDFACTOR_ONE;
			dst = SDL_GPU_BLENDFACTOR_ZERO;
			break;
		case alpha:
			src = SDL_GPU_BLENDFACTOR_ONE;
			dst = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case additive:
			src = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			dst = SDL_GPU_BLENDFACTOR_ONE;
			break;
		case non_premultiplied:
			src = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			dst = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		}

		enable = ((src != SDL_GPU_BLENDFACTOR_ONE) or (dst != SDL_GPU_BLENDFACTOR_ONE));

		return {
			.src_color_blendfactor = src,
			.dst_color_blendfactor = dst,
			.color_blend_op        = op,
			.src_alpha_blendfactor = src,
			.dst_alpha_blendfactor = dst,
			.alpha_blend_op        = op,
			.enable_blend          = enable,
		};
	}

	auto get_gpu_supported_shader_format(SDL_GPUDevice *gpu) -> SDL_GPUShaderFormat
	{
		auto backend_formats = SDL_GetGPUShaderFormats(gpu);

		if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL)
		{
			return SDL_GPU_SHADERFORMAT_DXIL;
		}

		if (backend_formats & SDL_GPU_SHADERFORMAT_SPIRV)
		{
			return SDL_GPU_SHADERFORMAT_SPIRV;
		}

		// TODO: add other backends

		return SDL_GPU_SHADERFORMAT_INVALID;
	}

	auto get_gpu_supported_depth_stencil_format(SDL_GPUDevice *gpu) -> SDL_GPUTextureFormat
	{
		// Order Matters, biggest to smallest
		constexpr auto depth_formats = std::array{
			SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
			SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
			SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
			SDL_GPU_TEXTUREFORMAT_D24_UNORM,
			SDL_GPU_TEXTUREFORMAT_D16_UNORM,
		};

		auto rng = depth_formats | vw::filter([&](const auto fmt) {
			return SDL_GPUTextureSupportsFormat(gpu, fmt, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
		}) | vw::take(1);

		assert(rng.begin() != rng.end() and "None of the depth formats are supported.");

		return *rng.begin();
	}

	struct shader_builder
	{
		io::byte_array shader_binary   = {};
		shader_stage stage             = shader_stage::invalid;
		uint32_t sampler_count         = 0;
		uint32_t uniform_buffer_count  = 0;
		uint32_t storage_uniform_count = 0;
		uint32_t storage_texture_count = 0;

		auto build(SDL_GPUDevice *gpu) const -> type::gfx_shader_ptr
		{
			assert(shader_binary.size() != 0 and "Shader binary is empty.");

			auto shader_info = SDL_GPUShaderCreateInfo{
				.code_size            = shader_binary.size(),
				.code                 = reinterpret_cast<const uint8_t *>(shader_binary.data()),
				.entrypoint           = "main",
				.format               = get_gpu_supported_shader_format(gpu),
				.stage                = to_sdl(stage),
				.num_samplers         = sampler_count,
				.num_storage_textures = storage_texture_count,
				.num_storage_buffers  = storage_uniform_count,
				.num_uniform_buffers  = uniform_buffer_count,
			};

			auto shader = SDL_CreateGPUShader(gpu, &shader_info);
			assert(shader != nullptr and "Failed to create shader.");

			return { shader, { gpu } };
		}
	};

	enum class topology_type : uint8_t
	{
		triangle_list,
		triangle_strip,
		line_list,
		line_strip,
		point_list,
	};

	auto to_sdl(topology_type type) -> SDL_GPUPrimitiveType
	{
		switch (type)
		{
			using enum topology_type;
		case triangle_list:
			return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		case triangle_strip:
			return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
		case line_list:
			return SDL_GPU_PRIMITIVETYPE_LINELIST;
		case line_strip:
			return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
		case point_list:
			return SDL_GPU_PRIMITIVETYPE_POINTLIST;
		}
		assert(false and "Unhandled topology type");
		return {};
	}

	struct gfx_pipeline_builder
	{
		type::gfx_shader_ptr vertex_shader   = nullptr;
		type::gfx_shader_ptr fragment_shader = nullptr;

		std::span<const SDL_GPUVertexAttribute> vertex_attributes                  = {};
		std::span<const SDL_GPUVertexBufferDescription> vertex_buffer_descriptions = {};

		SDL_GPUTextureFormat color_format = {};

		bool enable_depth_stencil = false;

		raster_type raster     = {};
		blend_type blend       = {};
		topology_type topology = {};

		auto build(SDL_GPUDevice *gpu) -> type::gfx_pipeline_ptr
		{
			auto vertex_input_state = SDL_GPUVertexInputState{
				.vertex_buffer_descriptions = vertex_buffer_descriptions.data(),
				.num_vertex_buffers         = static_cast<uint32_t>(vertex_buffer_descriptions.size()),
				.vertex_attributes          = vertex_attributes.data(),
				.num_vertex_attributes      = static_cast<uint32_t>(vertex_attributes.size()),
			};

			auto depth_stencil_state  = SDL_GPUDepthStencilState{};
			auto depth_stencil_format = SDL_GPUTextureFormat{};

			// Surpringly AMD doesn't support D24 on Vulkan, it does on DirectX??
			if (enable_depth_stencil)
			{
				depth_stencil_state = SDL_GPUDepthStencilState{
					.compare_op          = SDL_GPU_COMPAREOP_LESS,
					.write_mask          = std::numeric_limits<uint8_t>::max(),
					.enable_depth_test   = true,
					.enable_depth_write  = true,
					.enable_stencil_test = false, // TODO: figure out how to enable stencil under current api
				};

				depth_stencil_format = get_gpu_supported_depth_stencil_format(gpu);
			}

			auto color_targets = std::array{
				SDL_GPUColorTargetDescription{
				  .format      = color_format,
				  .blend_state = to_sdl(blend),
				},
			};

			auto target_info = SDL_GPUGraphicsPipelineTargetInfo{
				.color_target_descriptions = color_targets.data(),
				.num_color_targets         = static_cast<uint32_t>(color_targets.size()),
				.depth_stencil_format      = depth_stencil_format,
				.has_depth_stencil_target  = enable_depth_stencil,
			};

			auto pipeline_info = SDL_GPUGraphicsPipelineCreateInfo{
				.vertex_shader       = vertex_shader.get(),
				.fragment_shader     = fragment_shader.get(),
				.vertex_input_state  = vertex_input_state,
				.primitive_type      = to_sdl(topology),
				.rasterizer_state    = to_sdl(raster),
				.depth_stencil_state = depth_stencil_state,
				.target_info         = target_info,
			};

			auto pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipeline_info);
			assert(pipeline != nullptr and "Failed to create graphics pipeline.");

			return { pipeline, { gpu } };
		}
	};

	enum class sampler_type : uint8_t
	{
		point_clamp,
		point_wrap,
		linear_clamp,
		linear_wrap,
		anisotropic_clamp,
		anisotropic_wrap,
	};

	auto to_sdl(sampler_type type) -> SDL_GPUSamplerCreateInfo
	{
		switch (type)
		{
			using enum sampler_type;
		case point_clamp:
			return {
				.min_filter        = SDL_GPU_FILTER_NEAREST,
				.mag_filter        = SDL_GPU_FILTER_NEAREST,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.max_anisotropy    = 0,
				.enable_anisotropy = false,
			};
		case point_wrap:
			return {
				.min_filter        = SDL_GPU_FILTER_NEAREST,
				.mag_filter        = SDL_GPU_FILTER_NEAREST,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.max_anisotropy    = 0,
				.enable_anisotropy = false,
			};
		case linear_clamp:
			return {
				.min_filter        = SDL_GPU_FILTER_LINEAR,
				.mag_filter        = SDL_GPU_FILTER_LINEAR,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.max_anisotropy    = 0,
				.enable_anisotropy = false,
			};
		case linear_wrap:
			return {
				.min_filter        = SDL_GPU_FILTER_LINEAR,
				.mag_filter        = SDL_GPU_FILTER_LINEAR,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.max_anisotropy    = 0,
				.enable_anisotropy = false,
			};
		case anisotropic_clamp:
			return {
				.min_filter        = SDL_GPU_FILTER_LINEAR,
				.mag_filter        = SDL_GPU_FILTER_LINEAR,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
				.max_anisotropy    = MAX_ANISOTROPY,
				.enable_anisotropy = true,
			};
		case anisotropic_wrap:
			return {
				.min_filter        = SDL_GPU_FILTER_LINEAR,
				.mag_filter        = SDL_GPU_FILTER_LINEAR,
				.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
				.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
				.max_anisotropy    = MAX_ANISOTROPY,
				.enable_anisotropy = true,
			};
		}

		assert(false and "Unhandled Sampler mode.");
		return {};
	}

	auto make_sampler(SDL_GPUDevice *gpu, sampler_type type) -> type::gfx_sampler_ptr
	{
		auto sampler_info = to_sdl(type);

		auto sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
		assert(sampler != nullptr and "Failed to create sampler.");

		return { sampler, { gpu } };
	}
}