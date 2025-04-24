module;

export module application;

import std;
import sdl;
import clock;
import io;

namespace st = sdl::type;

export namespace project
{
	class application
	{
	public:
		// Rule of 5
		application(const application &)                     = default; // defaulted copy c'tor
		auto operator=(const application &) -> application & = default; // defaulted copy c'tor
		application(application &&)                          = default; // defaulted move c'tor
		auto operator=(application &&) -> application &      = default; // defaulted move c'tor

		// Public structures to start the application
		struct window_info
		{
			uint32_t width;
			uint32_t height;
			std::string_view title;
			SDL_WindowFlags flags = {};
		};

		struct gpu_info
		{
			SDL_GPUShaderFormat shader_format;
		};

		// Public API
		application(const window_info &wnd_info, const gpu_info &gpu_info)
		{
			wnd = sdl::make_window(wnd_info.width, wnd_info.height, wnd_info.title, wnd_info.flags);
			gpu = sdl::make_gpu(wnd.get(), gpu_info.shader_format);

			std::println("GPU Driver API: {}", SDL_GetGPUDeviceDriver(gpu.get()));
		}
		~application() = default;

		auto run() -> int
		{
			prepare_scene();

			clk.reset();
			while (not quit)
			{
				handle_sdl_events();

				update_state();

				draw();

				clk.tick();
			}

			std::println("Elapsed Time: {:.4f}s", clk.get_elapsed<clock::s>());

			scn = {};

			return 0;
		}

	private:
		// functions to handle SDL Events and Inputs
		void handle_sdl_events();
		void handle_sdl_input();

		// Scene and Application State
		void prepare_scene();
		void update_state();

		// Show on screen
		void draw();

		// Structure to hold scene objects
		struct scene
		{
			st::ttf_text_ptr text;
			st::ttf_font_ptr font;
			st::ttf_textengine_ptr text_engine;

			SDL_FColor clear_color;
			st::gfx_pipeline_ptr basic_pipeline;
			st::gpu_buffer_ptr vertex_buffer;
			uint32_t vertex_count;
			st::gpu_buffer_ptr index_buffer;
			uint32_t index_count;
			SDL_GPUTexture *text_atlas;
			st::gfx_sampler_ptr text_sampler;

			struct uniform_data
			{
				glm::mat4 projection;
				glm::mat4 model;
			};
			uniform_data mvp;
		};

		// Private members
		sdl::sdl_base base{}; // to handle init and quit for SDL
		st::window_ptr wnd = nullptr;
		st::gpu_ptr gpu    = nullptr;

		clock clk = {};

		bool quit     = false;
		SDL_Event evt = {};

		scene scn = {};
	};
}

using namespace project;
using namespace sdl;
using namespace sdl::type;

namespace rg = std::ranges;
namespace vw = std::views;

namespace
{
	// Structures to hold geometry data
	struct vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec4 color;
	};

	struct mesh
	{
		std::vector<vertex> vertices;
		std::vector<uint32_t> indices;
	};

	auto make_square() -> mesh
	{
		return {
			.vertices = {
			  { { +0.5f, +0.5f, 1.f }, { 1.f, 0.f }, { 1.f, 0.f, 0.f, 1.f } },
			  { { -0.5f, +0.5f, 1.f }, { 0.f, 0.f }, { 0.f, 1.f, 0.f, 1.f } },
			  { { -0.5f, -0.5f, 1.f }, { 0.f, 1.f }, { 0.f, 0.f, 1.f, 1.f } },
			  { { +0.5f, -0.5f, 1.f }, { 1.f, 1.f }, { 1.f, 1.f, 0.f, 1.f } },
			},
			.indices = {
			  0, 1, 2, // tri 1
			  2, 3, 0, // tri 2
			},
		};
	}

	auto text_to_mesh(TTF_Text *text) -> mesh
	{
		auto sequence = TTF_GetGPUTextDrawData(text);
		assert(sequence->next == nullptr and "Multiple text sequences. Need to handle differently.");

		auto msh = mesh{};

		msh.vertices.resize(sequence->num_vertices);
		for (auto i = 0; i < sequence->num_vertices; i++)
		{
			msh.vertices[i] = {
				.pos = {
				  sequence->xy[i].x,
				  sequence->xy[i].y,
				  0.f,
				},
				.uv = {
				  sequence->uv[i].x,
				  sequence->uv[i].y,
				},
				.color = { 1.f, 0.f, 1.f, 1.f },
			};
		}

		msh.indices.resize(sequence->num_indices);
		std::memcpy(msh.indices.data(),
		            sequence->indices,
		            sizeof(uint32_t) * sequence->num_indices);

		return msh;
	}

	auto get_text_atlas(TTF_Text *text) -> SDL_GPUTexture *
	{
		auto sequence = TTF_GetGPUTextDrawData(text);
		assert(sequence->next == nullptr and "Multiple text sequences. Need to handle differently.");

		return sequence->atlas_texture;
	}

	auto make_pipeline(SDL_GPUDevice *gpu, SDL_Window *wnd) -> gfx_pipeline_ptr
	{
		auto vs_shdr = shader_builder{
			.shader_binary        = io::read_file("shaders/basic.vs_6_4.cso"),
			.stage                = shader_stage::vertex,
			.uniform_buffer_count = 1,
		};

		auto ps_shdr = shader_builder{
			.shader_binary = io::read_file("shaders/basic.ps_6_4.cso"),
			.stage         = shader_stage::fragment,
			.sampler_count = 1,
		};

		using VA = SDL_GPUVertexAttribute;
		auto va  = std::array{
            VA{
			   .location    = 0,
			   .buffer_slot = 0,
			   .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			   .offset      = offsetof(vertex, vertex::pos),
            },
            VA{
			   .location    = 1,
			   .buffer_slot = 0,
			   .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
			   .offset      = offsetof(vertex, vertex::uv),
            },
            VA{
			   .location    = 2,
			   .buffer_slot = 0,
			   .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			   .offset      = offsetof(vertex, vertex::color),
            },
		};

		using VBD = SDL_GPUVertexBufferDescription;
		auto vbd  = std::array{
            VBD{
			   .slot       = 0,
			   .pitch      = sizeof(vertex),
			   .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            },
		};

		auto pl = gfx_pipeline_builder{
			.vertex_shader              = vs_shdr.build(gpu),
			.fragment_shader            = ps_shdr.build(gpu),
			.vertex_attributes          = va,
			.vertex_buffer_descriptions = vbd,
			.color_format               = SDL_GetGPUSwapchainTextureFormat(gpu, wnd),
			.enable_depth_stencil       = false,
			.raster                     = raster_type::none_fill,
			.blend                      = blend_type::non_premultiplied,
			.topology                   = topology_type::triangle_list,
		};

		return pl.build(gpu);
	}

	void upload_to_gpu(SDL_GPUDevice *gpu, io::byte_span src_data, SDL_GPUBuffer *dst_buffer)
	{
		auto src_size = static_cast<uint32_t>(src_data.size());
		assert(src_size > 0 and "Source data size is 0.");

		auto transfer_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = src_size,
		};
		auto transfer_buffer = SDL_CreateGPUTransferBuffer(gpu, &transfer_info);
		assert(transfer_buffer != nullptr and "Failed to create transfer buffer.");

		auto dst_data = SDL_MapGPUTransferBuffer(gpu, transfer_buffer, false);
		std::memcpy(dst_data, src_data.data(), src_size);
		SDL_UnmapGPUTransferBuffer(gpu, transfer_buffer);

		auto copy_cmd = SDL_AcquireGPUCommandBuffer(gpu);
		{
			auto copy_pass = SDL_BeginGPUCopyPass(copy_cmd);
			{
				auto src = SDL_GPUTransferBufferLocation{
					.transfer_buffer = transfer_buffer,
					.offset          = 0,
				};

				auto dst = SDL_GPUBufferRegion{
					.buffer = dst_buffer,
					.offset = 0,
					.size   = src_size,
				};

				SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
			}
			SDL_EndGPUCopyPass(copy_pass);
			SDL_SubmitGPUCommandBuffer(copy_cmd);
		}
		SDL_ReleaseGPUTransferBuffer(gpu, transfer_buffer);
	}
}

void application::handle_sdl_events()
{
	while (SDL_PollEvent(&evt))
	{
		switch (evt.type)
		{
		case SDL_EVENT_QUIT:
			quit = true;
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_WHEEL:
			handle_sdl_input();
			break;
		}
	}
}

void application::handle_sdl_input()
{
	auto handle_keyboard = [&]([[maybe_unused]] const SDL_KeyboardEvent &key_evt) {
		quit = true;
	};

	auto handle_mouse_motion = [&]([[maybe_unused]] const SDL_MouseMotionEvent &mouse_evt) {

	};

	auto handle_mouse_wheel = [&]([[maybe_unused]] const SDL_MouseWheelEvent &wheel_evt) {

	};

	switch (evt.type)
	{
	case SDL_EVENT_KEY_DOWN:
		handle_keyboard(evt.key);
		break;

	case SDL_EVENT_MOUSE_MOTION:
		handle_mouse_motion(evt.motion);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		handle_mouse_wheel(evt.wheel);
		break;
	}
}

void application::prepare_scene()
{
	auto gpu_ = gpu.get();

	scn.clear_color    = { 0.2f, 0.2f, 0.4f, 1.0f };
	scn.basic_pipeline = make_pipeline(gpu_, wnd.get());

	constexpr auto MAX_VERTEX_COUNT = 4000u;
	constexpr auto MAX_INDEX_COUNT  = 6000u;

	scn.vertex_buffer = sdl::make_gpu_buffer(gpu_, SDL_GPU_BUFFERUSAGE_VERTEX, sizeof(vertex) * MAX_VERTEX_COUNT, "Vertex Buffer");
	scn.index_buffer  = sdl::make_gpu_buffer(gpu_, SDL_GPU_BUFFERUSAGE_INDEX, sizeof(uint32_t) * MAX_INDEX_COUNT, "Index buffer");

	scn.text_engine = sdl::make_ttf_textengine(gpu_);
	scn.font        = sdl::make_ttf_font("c:/windows/fonts/NotoSans-Regular.ttf", false);
	scn.text        = sdl::make_ttf_text(scn.text_engine.get(), scn.font.get(), "Hello World!");

	// auto sqr_mesh = make_square();
	// upload_to_gpu(gpu_, io::as_byte_span(sqr_mesh.vertices), scn.vertex_buffer.get());
	// scn.vertex_count = static_cast<uint32_t>(sqr_mesh.vertices.size());
	// upload_to_gpu(gpu_, io::as_byte_span(sqr_mesh.indices), scn.index_buffer.get());
	// scn.index_count = static_cast<uint32_t>(sqr_mesh.indices.size());

	auto txt      = scn.text.get();
	auto txt_mesh = text_to_mesh(txt);
	upload_to_gpu(gpu_, io::as_byte_span(txt_mesh.vertices), scn.vertex_buffer.get());
	scn.vertex_count = static_cast<uint32_t>(txt_mesh.vertices.size());
	upload_to_gpu(gpu_, io::as_byte_span(txt_mesh.indices), scn.index_buffer.get());
	scn.index_count  = static_cast<uint32_t>(txt_mesh.indices.size());
	scn.text_atlas   = get_text_atlas(txt);
	scn.text_sampler = sdl::make_sampler(gpu_, sampler_type::linear_clamp);

	constexpr float fovy         = glm::radians(90.f);
	constexpr float aspect_ratio = 5.f / 4.f;
	constexpr float near_plane   = 0.1f;
	constexpr float far_plane    = 100.f;

	scn.mvp.projection = glm::perspective(fovy, aspect_ratio, near_plane, far_plane);

	auto &model = scn.mvp.model;

	model = glm::identity<glm::mat4>();
	model = glm::translate(model, { 0.f, 0.f, 80.f });
	model = glm::scale(model, { 0.3f, 0.3f, 0.3f });
}

void application::update_state()
{
}

void application::draw()
{
	auto cmd_buf = SDL_AcquireGPUCommandBuffer(gpu.get());
	assert(cmd_buf != nullptr and "Failed to acquire command buffer.");

	auto sc_img = sdl::next_swapchain_image(wnd.get(), cmd_buf);

	// Push Uniform buffer
	auto uniform_data = io::as_byte_span(scn.mvp);
	SDL_PushGPUVertexUniformData(cmd_buf, 0, uniform_data.data(), static_cast<uint32_t>(uniform_data.size()));

	auto color_target = SDL_GPUColorTargetInfo{
		.texture     = sc_img,
		.clear_color = scn.clear_color,
		.load_op     = SDL_GPU_LOADOP_CLEAR,
		.store_op    = SDL_GPU_STOREOP_STORE,
	};

	auto render_pass = SDL_BeginGPURenderPass(cmd_buf, &color_target, 1, nullptr);
	{
		SDL_BindGPUGraphicsPipeline(render_pass, scn.basic_pipeline.get());

		auto vertex_bindings = std::array{
			SDL_GPUBufferBinding{
			  .buffer = scn.vertex_buffer.get(),
			  .offset = 0,
			},
		};
		SDL_BindGPUVertexBuffers(render_pass, 0, vertex_bindings.data(), static_cast<uint32_t>(vertex_bindings.size()));

		auto index_binding = SDL_GPUBufferBinding{
			.buffer = scn.index_buffer.get(),
			.offset = 0,
		};
		SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		auto sampler_binding = SDL_GPUTextureSamplerBinding{
			.texture = scn.text_atlas,
			.sampler = scn.text_sampler.get(),
		};
		SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

		SDL_DrawGPUIndexedPrimitives(render_pass, scn.index_count, 1, 0, 0, 0);
	}
	SDL_EndGPURenderPass(render_pass);
	SDL_SubmitGPUCommandBuffer(cmd_buf);
}