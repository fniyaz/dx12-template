#include "renderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void Renderer::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

void Renderer::OnUpdate()
{
	ud -= dud;
	auto almost_half_pi = XM_PI / 2 * 0.99999;
	if (ud > almost_half_pi)
		ud = almost_half_pi;
	if (ud < -almost_half_pi)
		ud = -almost_half_pi;

	lr += dlr;

	XMVECTOR fwd { 0, 0, 1 };
	fwd = XMVector4Transform(fwd, XMMatrixRotationRollPitchYaw(ud, lr, 0));

	auto fz = fwd * dz + XMVector3Cross(fwd, XMVECTOR{ 0.f, 1.f, 0.f }) * dx;
	auto size = sqrtf(fwd.m128_f32[0] * fwd.m128_f32[0] + fwd.m128_f32[2] * fwd.m128_f32[2]);
	if (size > 0)
		fz /= size;
	
	x += fz.m128_f32[0];
	y += dy;
	z += fz.m128_f32[2];

	XMMATRIX world = XMMatrixScaling(.5f, .5f, .5f);

	XMMATRIX view = XMMatrixLookAtLH(XMVECTOR{ x, y, z }, XMVECTOR{ x, y, z } + fwd, XMVECTOR{ 0.f, 1.f, 0.f });
	XMMATRIX projection = XMMatrixPerspectiveFovLH(60.f*XM_PI/180.f, aspect_ratio, 0.001, 100.f);
		

	mvp = world * view * projection;

	memcpy(const_data_begin, &mvp, sizeof(mvp));
}

void Renderer::OnRender()
{
	PopulateCommandList();
	ID3D12CommandList* command_lists[] = { command_list.Get() };

	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);
	ThrowIfFailed(swap_chain->Present(0, 0));

	WaitForPreviousFrame();
}

void Renderer::OnDestroy()
{
	WaitForPreviousFrame();
	CloseHandle(fence_event);
}

void Renderer::OnKeyDown(UINT8 key)
{
	switch (key)
	{
	case 0x41 - 'a' + 'w':
		dz = 0.001;
		break;
	case 0x41 - 'a' + 's':
		dz = -0.001;
		break;
	case 0x41 - 'a' + 'a':
		dx = 0.001;
		break;
	case 0x41 - 'a' + 'd':
		dx = -0.001;
		break;
	case VK_SPACE:
		dy = 0.001;
		break;
	case VK_SHIFT:
		dy = -0.001;
		break;
	case VK_UP:
		dud = 0.001;
		break;
	case VK_DOWN:
		dud = -0.001;
		break;
	case VK_LEFT:
		dlr = -0.001;
		break;
	case VK_RIGHT:
		dlr = 0.001;
		break;
	default:
		break;
	}
}

void Renderer::OnKeyUp(UINT8 key)
{
	switch (key)
	{
	case 0x41 - 'a' + 'w':
		dz = 0;
		break;
	case 0x41 - 'a' + 's':
		dz = 0;
		break;
	case 0x41 - 'a' + 'a':
		dx = 0;
		break;
	case 0x41 - 'a' + 'd':
		dx = 0;
	case VK_SPACE:
		dy = 0;
		break;
	case VK_SHIFT:
		dy = 0;
		break;
	case VK_UP:
		dud = 0;
		break;
	case VK_DOWN:
		dud = 0;
		break;
	case VK_LEFT:
		dlr = 0;
		break;
	case VK_RIGHT:
		dlr = 0;
		break;
	default:
		break;
	}
}

void Renderer::LoadPipeline()
{
	UINT dxgifactory_flag = 0;

	// Create debug layer
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debg_controller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debg_controller)))) {
		debg_controller->EnableDebugLayer();
	}

	dxgifactory_flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	// Create device
	ComPtr<IDXGIFactory4> dxgifactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgifactory_flag, IID_PPV_ARGS(&dxgifactory)));

	ComPtr<IDXGIAdapter1> hardware_adapter;
	ThrowIfFailed(dxgifactory->EnumAdapters1(0, &hardware_adapter));
	ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

	// Create a direct command queue
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.BufferCount = frame_number;
	swap_chain_desc.Width = GetWidth();
	swap_chain_desc.Height = GetHeight();
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> temp_swap_chain;
	ThrowIfFailed(dxgifactory->CreateSwapChainForHwnd(
		command_queue.Get(),
		Win32Window::GetHwnd(),
		&swap_chain_desc,
		nullptr,
		nullptr,
		&temp_swap_chain
		));
	ThrowIfFailed(dxgifactory->MakeWindowAssociation(Win32Window::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(temp_swap_chain.As(&swap_chain));

	frame_index = swap_chain->GetCurrentBackBufferIndex();

	// Create descriptor heap for render target view
	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = frame_number;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)));
	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create constant buffer
	D3D12_DESCRIPTOR_HEAP_DESC cbv_heap_desc = {};
	cbv_heap_desc.NumDescriptors = 1;
	cbv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbv_heap_desc, IID_PPV_ARGS(&cbv_heap)));


	// Create render target view for each frame
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (INT i = 0; i < frame_number; i++) {
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
		rtv_handle.Offset(1, rtv_descriptor_size);
	}

	// Create command allocator
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
}

void Renderer::LoadAssets()
{
	// Create a root signature

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rs_feature_data = {};
	rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rs_feature_data, sizeof(rs_feature_data)))) {
		rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	CD3DX12_ROOT_PARAMETER1 root_parameters[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX );

	D3D12_ROOT_SIGNATURE_FLAGS rs_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signatre_desc;
	root_signatre_desc.Init_1_1(_countof(root_parameters), root_parameters, 0, nullptr, rs_flags);


	/*
	CD3DX12_ROOT_SIGNATURE_DESC root_signatre_desc;
	root_signatre_desc.Init(0, nullptr, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	*/

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signatre_desc, rs_feature_data.HighestVersion, &signature, &error));
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&root_signature)));

	// Create full PSO
	ComPtr<ID3DBlob> ver_shader;
	ComPtr<ID3DBlob> frag_shader;

	UINT compile_flags = 0;

#ifdef _DEBUG
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG


	std::wstring shader_path = GetBinPath(std::wstring(L"shaders.hlsl"));
	ThrowIfFailed(D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr,
		"VSMain", "vs_5_0", compile_flags, 0, &ver_shader, &error));
	ThrowIfFailed(D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr,
		"PSMain", "ps_5_0", compile_flags, 0, &frag_shader, &error));

	D3D12_INPUT_ELEMENT_DESC input_element_desc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 3 * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.InputLayout = { input_element_desc, _countof(input_element_desc) };
	pso_desc.pRootSignature = root_signature.Get();
	pso_desc.VS = CD3DX12_SHADER_BYTECODE(ver_shader.Get());
	pso_desc.PS = CD3DX12_SHADER_BYTECODE(frag_shader.Get());
	pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	//pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; //todo remove
	//pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; //todo remove
	pso_desc.RasterizerState.DepthClipEnable = false; //todo remove
	pso_desc.DepthStencilState.DepthEnable = FALSE;
	pso_desc.DepthStencilState.StencilEnable = FALSE;
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.SampleDesc.Count = 1;

	ThrowIfFailed(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));


	// Create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), 
		pipeline_state.Get(), IID_PPV_ARGS(&command_list)));
	ThrowIfFailed(command_list->Close());

	// Create and upload vertex buffer
	std::wstring obj_directory = GetBinPath(L"");
	std::string obj_path(obj_directory.begin(), obj_directory.end());
	std::string inputfile = obj_path+"\CornellBox-Original.obj";
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str(), obj_path.c_str());

	if (!warn.empty()) {
		std::wstring wide_warn(warn.begin(), warn.end());
		wide_warn = L"TinyObj reader warning: " + wide_warn + L"\n";
		OutputDebugString(wide_warn.c_str());
	}

	if (!err.empty()) {
		std::wstring wide_err(err.begin(), err.end());
		wide_err = L"TinyObj reader error: " + wide_err + L"\n";
		OutputDebugString(wide_err.c_str());
	}

	if (!ret) {
		ThrowIfFailed(-1);
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			// per-face material
			int material_ids = shapes[s].mesh.material_ids[f];
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				auto color = materials[material_ids].diffuse;
				
				vertices.push_back(ColorVertex{ { vx, vy, vz }, {color[0], color[1], color[2], 1.f} });
			}
			index_offset += fv;			
		}
	}

	/*ColorVertex triangle_verteces[] = {
		{{0.f, 0.25f *aspect_ratio, 0.f}, {1.f, 0.f, 0.f, 1.f}},
		{{0.25f * std::sqrt(2.f), -0.25f * aspect_ratio, 0.f}, {0.f, 1.f, 0.f, 1.f}},
		{{-0.25f * std::sqrt(2.f), -0.25f * aspect_ratio, 0.f}, {0.f, 0.f, 1.f, 1.f}}
	};*/

	const UINT ver_buff_size = vertices.size() * sizeof(ColorVertex);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(ver_buff_size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertex_buffer)
	));

	UINT8* vertex_data_begin;
	CD3DX12_RANGE read_range(0, 0);
	ThrowIfFailed(vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin)));
	memcpy(vertex_data_begin, vertices.data(), sizeof(ColorVertex) * vertices.size());
	vertex_buffer->Unmap(0, nullptr);

	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(ColorVertex);
	vertex_buffer_view.SizeInBytes = ver_buff_size;

	// Constant buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(16 * 4 * 1024),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constant_buffer)
	));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
	cbv_desc.BufferLocation = constant_buffer->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = (sizeof(mvp) + 255) & ~255; // Black Magic
	device->CreateConstantBufferView(&cbv_desc, cbv_heap->GetCPUDescriptorHandleForHeapStart());

	ThrowIfFailed(constant_buffer->Map(0, &read_range, reinterpret_cast<void**>(&const_data_begin)));
	memcpy(const_data_begin, &mvp, sizeof(mvp));

	// Create synchronization objects
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fence_value = 1;
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Renderer::PopulateCommandList()
{
	// Reset allocators and lists
	ThrowIfFailed(command_allocator->Reset());
	ThrowIfFailed(command_list->Reset(command_allocator.Get(), pipeline_state.Get()));

	// Set initial state
	command_list->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* heaps[] = {cbv_heap.Get()};
	command_list->SetDescriptorHeaps(_countof(heaps), heaps);
	command_list->SetGraphicsRootDescriptorTable(0, cbv_heap->GetGPUDescriptorHandleForHeapStart());
	command_list->RSSetViewports(1, &view_port);
	command_list->RSSetScissorRects(1, &scissor_rect);

	// Resource barrier from present to RT
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(), 
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
		));

	// Record commands
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart(),
		frame_index, rtv_descriptor_size);
	command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
	const float clear_color[3] = { 0.f, 0.f, 0.f };
	command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	command_list->DrawInstanced(vertices.size(), 1, 0, 0);

	// Resource barrier from RT to present
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
		));

	// Close command list
	ThrowIfFailed(command_list->Close());
}

void Renderer::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// Signal and increment the fence value.
	const UINT64 prev_fence_vale = fence_value;
	ThrowIfFailed(command_queue->Signal(fence.Get(), prev_fence_vale));
	fence_value++;

	if (fence->GetCompletedValue() < prev_fence_vale) {
		ThrowIfFailed(fence->SetEventOnCompletion(prev_fence_vale, fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}

	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

std::wstring Renderer::GetBinPath(std::wstring shader_file) const
{
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring module_path = buffer;
	std::wstring::size_type pos = module_path.find_last_of(L"\\/");

	return module_path.substr(0, pos + 1) + shader_file;
}
