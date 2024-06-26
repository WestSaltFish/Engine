//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include "Globals.h"

#define MIPMAP_BASE_LEVEL 0
#define MIPMAP_MAX_LEVEL 4

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
	GLchar  infoLogBuffer[1024] = {};
	GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
	GLsizei infoLogSize;
	GLint   success;

	char versionString[] = "#version 430\n";
	char shaderNameDefine[128];
	sprintf(shaderNameDefine, "#define %s\n", shaderName);
	char vertexShaderDefine[] = "#define VERTEX\n";
	char fragmentShaderDefine[] = "#define FRAGMENT\n";

	const GLchar* vertexShaderSource[] = {
		versionString,
		shaderNameDefine,
		vertexShaderDefine,
		programSource.str
	};
	const GLint vertexShaderLengths[] = {
		(GLint)strlen(versionString),
		(GLint)strlen(shaderNameDefine),
		(GLint)strlen(vertexShaderDefine),
		(GLint)programSource.len
	};
	const GLchar* fragmentShaderSource[] = {
		versionString,
		shaderNameDefine,
		fragmentShaderDefine,
		programSource.str
	};
	const GLint fragmentShaderLengths[] = {
		(GLint)strlen(versionString),
		(GLint)strlen(shaderNameDefine),
		(GLint)strlen(fragmentShaderDefine),
		(GLint)programSource.len
	};

	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
	glCompileShader(vshader);
	glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
	glCompileShader(fshader);
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	GLuint programHandle = glCreateProgram();
	glAttachShader(programHandle, vshader);
	glAttachShader(programHandle, fshader);
	glLinkProgram(programHandle);
	glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	glUseProgram(0);

	glDetachShader(programHandle, vshader);
	glDetachShader(programHandle, fshader);
	glDeleteShader(vshader);
	glDeleteShader(fshader);

	return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
	String programSource = ReadTextFile(filepath);

	Program program = {};
	program.handle = CreateProgramFromSource(programSource, programName);
	program.filepath = filepath;
	program.programName = programName;
	program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);

	GLint attributeCount = 0;
	glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &attributeCount);

	for (GLuint i = 0; i < attributeCount; i++)
	{
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		GLchar name[256];
		glGetActiveAttrib(program.handle, i,
			ARRAY_COUNT(name),
			&length,
			&size,
			&type,
			name);

		u8 location = glGetAttribLocation(program.handle, name);
		program.shaderLayout.attributes.push_back(VertexShaderAttribute{ location, (u8)size });
	}

	app->programs.push_back(program);

	return app->programs.size() - 1;
}

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program)
{
	GLuint ReturnValue = 0;

	SubMesh& Submesh = mesh.submeshes[submeshIndex];
	for (u32 i = 0; i < (u32)Submesh.vaos.size(); ++i)
	{
		if (Submesh.vaos[i].programHandle == program.handle)
		{
			ReturnValue = Submesh.vaos[i].handle;
			break;
		}
	}

	if (ReturnValue == 0)
	{
		glGenVertexArrays(1, &ReturnValue);
		glBindVertexArray(ReturnValue);

		glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

		auto& ShaderLayout = program.shaderLayout.attributes;
		for (auto ShaderIt = ShaderLayout.cbegin(); ShaderIt != ShaderLayout.cend(); ++ShaderIt)
		{
			bool attributeWasLinked = false;
			auto SubmeshLayout = Submesh.vertexBufferLayout.attributes;
			for (auto SubmeshIt = SubmeshLayout.cbegin(); SubmeshIt != SubmeshLayout.cend(); ++SubmeshIt)
			{
				if (ShaderIt->location == SubmeshIt->location)
				{
					const u32 index = SubmeshIt->location;
					const u32 ncomp = SubmeshIt->componentCount;
					const u32 offset = SubmeshIt->offset + Submesh.vertexOffset;
					const u32 stride = Submesh.vertexBufferLayout.stride;

					glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)(offset));
					glEnableVertexAttribArray(index);

					attributeWasLinked = true;
					break;
				}
			}
			assert(attributeWasLinked);
		}
		glBindVertexArray(0);

		VAO vao = { ReturnValue, program.handle };
		Submesh.vaos.push_back(vao);
	}

	return ReturnValue;
}

glm::mat4 TransformScale(const vec3& scaleFactos)
{
	return scale(scaleFactos);
}

glm::mat4 TransformPositionScale(const vec3& pos, const vec3& scaleFactors)
{
	glm::mat4 transform = translate(pos);
	transform = scale(transform, scaleFactors);

	return transform;
}

void Init(App* app)
{
	// TODO: Initialize your resources here!
	// - vertex buffers
	// - element/index buffers
	// - vaos
	// - programs (and retrieve uniform indices)
	// - textures

	//Get OPENGL info.
	app->openglDebugInfo += "OpeGL version:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));

	glGenBuffers(1, &app->embeddedVertices);
	glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &app->embeddedElements);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenVertexArrays(1, &app->vao);
	glBindVertexArray(app->vao);
	glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Load shaders
	app->renderToBackBufferShader = LoadProgram(app, "Shaders/RENDER_TO_BB.glsl", "RENDER_TO_BB");
	app->renderToFrameBufferShader = LoadProgram(app, "Shaders/RENDER_TO_FB.glsl", "RENDER_TO_FB");
	app->framebufferToQuadShader = LoadProgram(app, "Shaders/FB_TO_BB.glsl", "FB_TO_BB");
	app->gridRenderShader = LoadProgram(app, "Shader/PRGrid.glsl", "GRID_SHADER");

	// Load bloom shaders
	app->blitBrightestPixelsShader = LoadProgram(app, "Shader/PASS_BLIT_BRIGHT.glsl", "PASS_BLIT_BRIGHT");
	app->blurShader = LoadProgram(app, "Shader/BLUR.glsl", "BLUR");
	app->bloomShader = LoadProgram(app, "Shader/BLOOM.glsl", "BLOOM");

	const Program& texturedMeshProgram = app->programs[app->renderToBackBufferShader];
	app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");

	// Load models
	u32 ModelIndex = ModelLoader::LoadModel(app, "Models/Substitute/ob0226_00.obj");
	u32 GroundModelIndex = ModelLoader::LoadModel(app, "Models/Ground.obj");

	glEnable(GL_DEPTH_TEST);

	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

	app->localUniformBuffer = BufferManager::CreateConstantBuffer(app->maxUniformBufferSize);

	app->entities.push_back({ TransformPositionScale(vec3(-10.0, 0.0, -2.0), vec3(1.0, 1.0, 1.0)), ModelIndex, 0, 0 });
	app->entities.push_back({ TransformPositionScale(vec3(-0.0, 0.0, -2.0), vec3(1.0, 1.0, 1.0)), ModelIndex, 0, 0 });
	app->entities.push_back({ TransformPositionScale(vec3(-5.0, 0.0, -2.0), vec3(1.0, 1.0, 1.0)), ModelIndex, 0, 0 });

	app->entities.push_back({ TransformPositionScale(vec3(0.0, 0.0, 0.0), vec3(10.0, 1.0, 10.0)), GroundModelIndex, 0, 0 });

	app->lights.push_back({ LightType::LightType_Directional, vec3(1.0, 1.0, 1.0),vec3(1.0, -1.0, 1.0),vec3(0, 0, 0) });
	app->lights.push_back({ LightType::LighthType_point, vec3(0.0, 1.0, 0.0),vec3(1.0, 1.0, 1.0),vec3(0, 0, 0) });

	app->ConfigureFrameBuffer(app->deferredFrameBuffer);

	// config frame buffer
	app->ConfigureFrameBuffer(app->bloom.fbBloom1);
	app->ConfigureFrameBuffer(app->bloom.fbBloom2);
	app->ConfigureFrameBuffer(app->bloom.fbBloom3);
	app->ConfigureFrameBuffer(app->bloom.fbBloom4);
	app->ConfigureFrameBuffer(app->bloom.fbBloom5);

	// Init camera
	app->camera.pos = glm::vec3(0.0f, 5.0f, 15.0f);
	app->camera.front = glm::vec3(0.0f, 0.0f, -1.0f);
	app->camera.up = glm::vec3(0.0f, 1.0f, 0.0f);

	app->mode = Mode::Mode_Deferred;
}

void Gui(App* app)
{
	ImGui::Begin("Info");
	ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
	ImGui::Text("%s", app->openglDebugInfo.c_str());

	//ImGui::ShowDemoWindow();

	if (ImGui::CollapsingHeader("Camera"))
	{
		ImGui::SliderFloat("movement speed", &app->camera.moveSpeed, 0.0, 100.0);
		ImGui::SliderFloat("rotation sensitive", &app->camera.rotationSensitive, 0.0, 1.0);
	}
	if (ImGui::CollapsingHeader("Lights"))
	{
		if (app->lights.size() > 0)
			ImGui::ColorEdit3("Directional light color", (float*)&app->lights[0].color);

		if (app->lights.size() > 1)
			ImGui::ColorEdit3("Point light color", (float*)&app->lights[1].color);
		//ImGui::SliderFloat("movement speed", &app->camera.moveSpeed, 0.0, 100.0);
		//ImGui::SliderFloat("rotation sensitive", &app->camera.rotationSensitive, 0.0, 1.0);
	}

	ImGui::Spacing();

	const char* RenderModes[] = { "FORWARD", "DEFERRED" };
	if (ImGui::BeginCombo("Render Mode", RenderModes[app->mode]))
	{
		for (int i = 0; i < ARRAY_COUNT(RenderModes); ++i)
		{
			bool isSelected = (i == app->mode);
			if (ImGui::Selectable(RenderModes[i], isSelected))
			{
				app->mode = static_cast<Mode>(i);
			}
		}

		ImGui::EndCombo();
	}

	if (app->mode == Mode::Mode_Deferred)
	{
		for (int i = 0; i < app->deferredFrameBuffer.colorAttachments.size(); i++)
		{
			ImGui::Image((ImTextureID)app->deferredFrameBuffer.colorAttachments[i],
				ImVec2(250, 150), ImVec2(0, 1), ImVec2(1, 0));
		}
	}

	ImGui::End();
}

void Update(App* app)
{
	// You can handle app->input keyboard/mouse here
	UpdateCamera(app);
}

void UpdateCamera(App* app)
{
	float moveSpeed = app->camera.moveSpeed * app->deltaTime;

	// camera movement
	if (app->input.keys[Key::K_W] == ButtonState::BUTTON_PRESSED)
	{
		app->camera.pos += app->camera.front * moveSpeed;
	}
	if (app->input.keys[Key::K_S] == ButtonState::BUTTON_PRESSED)
	{
		app->camera.pos -= app->camera.front * moveSpeed;
	}
	if (app->input.keys[Key::K_A] == ButtonState::BUTTON_PRESSED)
	{
		app->camera.pos -= glm::normalize(glm::cross(app->camera.front, app->camera.up)) * moveSpeed;
	}
	if (app->input.keys[Key::K_D] == ButtonState::BUTTON_PRESSED)
	{
		app->camera.pos += glm::normalize(glm::cross(app->camera.front, app->camera.up)) * moveSpeed;
	}

	// camera rotation
	if (app->input.mouseButtons[MouseButton::RIGHT] == ButtonState::BUTTON_PRESS)
	{
		app->input.mouseLastPos = app->input.mousePos;
	}
	else if (app->input.mouseButtons[MouseButton::RIGHT] == ButtonState::BUTTON_PRESSED)
	{
		float xoffset = app->input.mousePos.x - app->input.mouseLastPos.x;
		float yoffset = app->input.mouseLastPos.y - app->input.mousePos.y;

		app->input.mouseLastPos = app->input.mousePos;

		xoffset *= app->camera.rotationSensitive;
		yoffset *= app->camera.rotationSensitive;

		app->camera.yaw += xoffset;
		app->camera.pitch += yoffset;

		app->camera.pitch = app->camera.pitch > 89.0 ? 89.0 : app->camera.pitch < -89.0 ? -89.0 : app->camera.pitch;

		glm::vec3 direction;
		direction.x = cos(glm::radians(app->camera.yaw)) * cos(glm::radians(app->camera.pitch));
		direction.y = sin(glm::radians(app->camera.pitch));
		direction.z = sin(glm::radians(app->camera.yaw)) * cos(glm::radians(app->camera.pitch));
		app->camera.front = glm::normalize(direction);
	}
}

void InitBloomEffect(App* app)
{
	// Vertical
	if (app->bloom.rtBright != 0)
		glDeleteTextures(1, &app->bloom.rtBright);
	
	glGenTextures(1, &app->bloom.rtBright);
	glBindTexture(GL_TEXTURE_2D, app->bloom.rtBright);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, MIPMAP_BASE_LEVEL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MIPMAP_MAX_LEVEL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x / 2, app->displaySize.y / 2, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, app->displaySize.x / 4, app->displaySize.y / 4, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA16F, app->displaySize.x / 8, app->displaySize.y / 8, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA16F, app->displaySize.x / 16, app->displaySize.y / 16, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 4, GL_RGBA16F, app->displaySize.x / 32, app->displaySize.y / 32, 0, GL_RGBA, GL_FLOAT, nullptr);
	glGenerateMipmap(GL_TEXTURE_2D);

	// Horizontal
	if (app->bloom.rtBloomH != 0)
		glDeleteTextures(1, &app->bloom.rtBloomH);

	glGenTextures(1, &app->bloom.rtBloomH);
	glBindTexture(GL_TEXTURE_2D, app->bloom.rtBloomH);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, MIPMAP_BASE_LEVEL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MIPMAP_MAX_LEVEL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x / 2, app->displaySize.y / 2, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, app->displaySize.x / 4, app->displaySize.y / 4, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA16F, app->displaySize.x / 8, app->displaySize.y / 8, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA16F, app->displaySize.x / 16, app->displaySize.y / 16, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexImage2D(GL_TEXTURE_2D, 4, GL_RGBA16F, app->displaySize.x / 32, app->displaySize.y / 32, 0, GL_RGBA, GL_FLOAT, nullptr);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void PassBlitBrightPixels(App* app)
{
	float threshold = 1.0;

	glBindFramebuffer(GL_FRAMEBUFFER, app->bloom.fbBloom1.fbHandle);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0, 0, app->displaySize.x, app->displaySize.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	const Program& blitBrightestProgram = app->programs[app->blitBrightestPixelsShader];
	glUseProgram(blitBrightestProgram.handle);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->bloom.fbBloom1.colorAttachments[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glUniform1f(glGetUniformLocation(blitBrightestProgram.handle, "threshold"), threshold);
	glUniform1i(glGetUniformLocation(blitBrightestProgram.handle, "colorTexture"), 0);

	app->RenderGeometry(blitBrightestProgram);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glUseProgram(0);
}

void Render(App* app)
{
	switch (app->mode)
	{
	case Mode_Forward:
	{
		app->UpdateEntityBuffer();

		glViewport(0, 0, app->displaySize.x, app->displaySize.y);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const Program& forwardProgram = app->programs[app->renderToBackBufferShader];
		glUseProgram(forwardProgram.handle);
		app->RenderGeometry(forwardProgram);

		// try to do bloom here
	}
	break;

	case Mode_Deferred:
	{
		app->UpdateEntityBuffer();

		// Render to FB ColorAtt.
		glViewport(0, 0, app->displaySize.x, app->displaySize.y);
		glBindFramebuffer(GL_FRAMEBUFFER, app->deferredFrameBuffer.fbHandle);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//glDrawBuffers(app->deferredFrameBuffer.colorAttachments.size(), app->deferredFrameBuffer.colorAttachments.data());

		//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const Program& deferredProgram = app->programs[app->renderToFrameBufferShader];
		glUseProgram(deferredProgram.handle);
		app->RenderGeometry(deferredProgram);

		// Render Grid To CA
		/*
		GLuint drawBuffers[] = { GL_COLOR_ATTACHMENT4 };
		glDrawBuffers(1, drawBuffers);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		GLuint gridHandle = app->programs[app->gridRenderShader].handle;

		glUseProgram(gridHandle);

		vec4 tbrl = app->camera.getTopBottomLeftRight();

		glUniform1f(glGetUniformLocation(gridHandle, "top"), tbrl.x);
		glUniform1f(glGetUniformLocation(gridHandle, "bottom"), tbrl.y);
		glUniform1f(glGetUniformLocation(gridHandle, "right"), tbrl.z);
		glUniform1f(glGetUniformLocation(gridHandle, "left"), tbrl.w);
		glUniform1f(glGetUniformLocation(gridHandle, "znear"), app->camera.znear);

		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), app->camera.pos);
		glm::mat4 yawMatrix = glm::rotate(glm::mat4(1.0), glm::radians(app->camera.yaw), glm::vec3(0.0, 1.0, 0.0));
		glm::mat4 pitchMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(app->camera.pitch), glm::vec3(1.0, 0.0, 0.0));
		glm::mat4 rotationMatrix = yawMatrix * pitchMatrix;
		glm::mat4 cameraWorldMatrix = translationMatrix * rotationMatrix;

		glUniformMatrix4fv(glGetUniformLocation(gridHandle, "worldMatrix"), 1, GL_FALSE, &cameraWorldMatrix[0][0]);

		glBindVertexArray(app->vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
		*/

		glBindVertexArray(0);
		glUseProgram(0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_BLEND);

		// Render to BB from ColorAtt.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, app->displaySize.x, app->displaySize.y);

		const Program& FBToBB = app->programs[app->framebufferToQuadShader];
		glUseProgram(FBToBB.handle);

		// Render Quad
		glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->localUniformBuffer.handle, app->globalParamsOffset, app->globalParamsSize);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachments[0]);
		glUniform1i(glGetUniformLocation(FBToBB.handle, "uAlbedo"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachments[1]);
		glUniform1i(glGetUniformLocation(FBToBB.handle, "uNormals"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachments[2]);
		glUniform1i(glGetUniformLocation(FBToBB.handle, "uPosition"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachments[3]);
		glUniform1i(glGetUniformLocation(FBToBB.handle, "uViewDir"), 3);

		glBindVertexArray(app->vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

		// Release source
		glBindVertexArray(0);
		glUseProgram(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	break;

	case Mode_Bloom:
	{
		const vec2 horizontal(1.0, 0.0);
		const vec2 vertical(0.0, 1.0);

		const float w = app->displaySize.x;
		const float h = app->displaySize.y;

		float threshold = 1.0;	

	}
	break;

	default:;
	}
}

void App::UpdateEntityBuffer()
{
	// camera
	camera.aspecRatio = (float)displaySize.x / (float)displaySize.y;
	camera.fovYRad = glm::radians(60.0f);
	glm::mat4 projection = glm::perspective(camera.fovYRad, camera.aspecRatio, camera.znear, camera.zfar);
	glm::mat4 view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);

	BufferManager::MapBuffer(localUniformBuffer, GL_WRITE_ONLY);

	//globalParamsOffset - localUniformBuffer.head;
	PushVec3(localUniformBuffer, camera.pos);
	PushUInt(localUniformBuffer, lights.size());

	// Lights
	for (int i = 0; i < lights.size(); ++i)
	{
		BufferManager::AlignHead(localUniformBuffer, sizeof(vec4));

		Light& light = lights[i];
		PushUInt(localUniformBuffer, light.type);
		PushVec3(localUniformBuffer, light.color);
		PushVec3(localUniformBuffer, light.direction);
		PushVec3(localUniformBuffer, light.position);
	}
	globalParamsSize = localUniformBuffer.head - globalParamsOffset;

	u32 iteration = 0;

	for (auto it = entities.begin(); it != entities.end(); ++it)
	{
		glm::mat4 world = it->worldMatrix;
		glm::mat4 WVP = projection * view * world;

		Buffer& localBuffer = localUniformBuffer;
		BufferManager::AlignHead(localBuffer, uniformBlockAligment);
		it->localParamOffset = localBuffer.head;
		PushMat4(localBuffer, world);
		PushMat4(localBuffer, WVP);
		it->localParamSize = localBuffer.head - it->localParamOffset;
		++iteration;
	}

	BufferManager::UnmapBuffer(localUniformBuffer);
}

void App::ConfigureFrameBuffer(FrameBuffer& aConfigFB)
{
	aConfigFB.colorAttachments.push_back(CreateTexture());
	aConfigFB.colorAttachments.push_back(CreateTexture(true));
	aConfigFB.colorAttachments.push_back(CreateTexture(true));
	aConfigFB.colorAttachments.push_back(CreateTexture(true));

	aConfigFB.colorAttachments.push_back(CreateTexture(true)); // oMetallic
	aConfigFB.colorAttachments.push_back(CreateTexture(true)); // oRoughness
	aConfigFB.colorAttachments.push_back(CreateTexture(true)); // oAo
	aConfigFB.colorAttachments.push_back(CreateTexture(true)); // oEmissive

	glGenTextures(1, &aConfigFB.depthHandle);
	glBindTexture(GL_TEXTURE_2D, aConfigFB.depthHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, displaySize.x, displaySize.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &aConfigFB.fbHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, aConfigFB.fbHandle);

	std::vector<GLuint> drawBuffers;
	for (size_t i = 0; i < aConfigFB.colorAttachments.size(); ++i)
	{
		GLuint position = GL_COLOR_ATTACHMENT0 + i;
		glFramebufferTexture(GL_FRAMEBUFFER, position, aConfigFB.colorAttachments[i], 0);
		drawBuffers.push_back(position);
	}

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, aConfigFB.depthHandle, 0);
	glDrawBuffers(drawBuffers.size(), drawBuffers.data());

	GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		int i = 0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::RenderGeometry(const Program& aBindedProgram)
{
	glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), localUniformBuffer.handle, globalParamsOffset, globalParamsSize);

	for (auto it = entities.begin(); it != entities.end(); ++it)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), localUniformBuffer.handle, it->localParamOffset, it->localParamSize);

		Model& model = models[it->modelIndex];
		Mesh& mesh = meshes[model.meshIdx];

		for (u32 i = 0; i < mesh.submeshes.size(); ++i)
		{
			GLuint vao = FindVAO(mesh, i, aBindedProgram);
			glBindVertexArray(vao);

			u32 subMeshmaterialIdx = model.materialIdx[i];
			Material& subMeshMaterial = materials[subMeshmaterialIdx];

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[subMeshMaterial.albedoTextureIdx].handle);
			glUniform1i(texturedMeshProgram_uTexture, 0);

			glUniform3fv(glGetUniformLocation(aBindedProgram.handle, "uAlbedo"), 1, glm::value_ptr(subMeshMaterial.albedo));
			glUniform1i(glGetUniformLocation(aBindedProgram.handle, "useTexture"), subMeshMaterial.useTexture);

			SubMesh& submesh = mesh.submeshes[i];
			glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
		}
	}
}

const GLuint App::CreateTexture(const bool isFloating)
{
	GLuint textureHandle;

	GLenum internalFormat = isFloating ? GL_RGBA16F : GL_RGBA8;
	GLenum format = GL_RGBA;
	GLenum dataType = isFloating ? GL_FLOAT : GL_UNSIGNED_BYTE;

	glGenTextures(1, &textureHandle);
	glBindTexture(GL_TEXTURE_2D, textureHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, displaySize.x, displaySize.y, 0, format, dataType, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureHandle;
}
