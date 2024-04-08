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

	//glGenBuffers(1, &app->embeddedVertices);
	//glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	//glGenBuffers(1, &app->embeddedElements);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//glGenVertexArrays(1, &app->vao);
	//glBindVertexArray(app->vao);
	//glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);

	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
	//glEnableVertexAttribArray(1);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	//glBindVertexArray(0);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//app->texturedGeometryProgramIdx = LoadProgram(app, "shaders.glsl", "TEXTURED_GEOMETRY");
	//const Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];
	//app->programUniformTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

	app->texturedMeshProgramIdx = LoadProgram(app, "base_model.glsl", "BASE_MODEL");
	const Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];
	app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");
	u32 PatricModelIndex = ModelLoader::LoadModel(app, "Patrick/Patrick.obj");

	glEnable(GL_DEPTH_TEST);

	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

	app->localUniformBuffer = BufferManager::CreateConstantBuffer(app->maxUniformBufferSize);

	app->entities.push_back({ glm::identity<glm::mat4>(), PatricModelIndex, 0, 0 });
	app->entities.push_back({ glm::identity<glm::mat4>(), PatricModelIndex, 0, 0 });
	app->entities.push_back({ glm::identity<glm::mat4>(), PatricModelIndex, 0, 0 });
	//app->diceTexIdx = ModelLoader::LoadTexture2D(app, "dice.png");

	glGenTextures(1, &app->colorAttachmentHandle);
	glBindTexture(GL_TEXTURE_2D, app->colorAttachmentHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint depthAttachmentHandle;
	glGenTextures(1, &depthAttachmentHandle);
	glBindTexture(GL_TEXTURE_2D, depthAttachmentHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &app->framebufferHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, app->framebufferHandle);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, app->colorAttachmentHandle, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthAttachmentHandle, 0);
	
	GLuint drawBuffer[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(ARRAY_COUNT(drawBuffer), drawBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	app->mode = Mode::Mode_Deferred;
}

void Gui(App* app)
{
	ImGui::Begin("Info");
	ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
	ImGui::Text("%s", app->openglDebugInfo.c_str());
	ImGui::End();
}

void Update(App* app)
{
	// You can handle app->input keyboard/mouse here
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

void Render(App* app)
{
	switch (app->mode)
	{
		case Mode_Deferred:
		{
			app->UpdateEntityBuffer();

			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, app->displaySize.x, app->displaySize.y);

			glBindFramebuffer(GL_FRAMEBUFFER, app->framebufferHandle);
			
			GLuint drawBuffers[] = { app->colorAttachmentHandle };
			glDrawBuffers(ARRAY_COUNT(drawBuffers), drawBuffers);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			const Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];
			glUseProgram(texturedMeshProgram.handle);

			//glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->localUniformBuffer.handle, app->globalParamsOffset, app->globals);
			
			BufferManager::BindBuffer(app->localUniformBuffer);

			for (auto it = app->entities.begin(); it != app->entities.end(); ++it)
			{
				glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->localUniformBuffer.handle, it->localParamOffset, it->localParamSize);

				Model& model = app->models[app->patricioModel];
				Mesh& mesh = app->meshes[model.meshIdx];

				for (u32 i = 0; i < mesh.submeshes.size(); ++i)
				{
					GLuint vao = FindVAO(mesh, i, texturedMeshProgram);
					glBindVertexArray(vao);

					u32 subMeshmaterialIdx = model.materialIdx[i];
					Material& subMeshMaterial = app->materials[subMeshmaterialIdx];

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, app->textures[subMeshMaterial.albedoTextureIdx].handle);
					glUniform1i(app->texturedMeshProgram_uTexture, 0);

					SubMesh& submesh = mesh.submeshes[i];
					glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// Mix color attachment to plane.
		}
	break;
	default:;
	}
}

void App::UpdateEntityBuffer()
{
	// temporal camera
	float aspecRatio = (float)displaySize.x / (float)displaySize.y;
	float znear = 0.1f;
	float zfar = 1000.0f;
	glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspecRatio, znear, zfar);

	vec3 target = vec3(0, 0, 0);
	vec3 camPos = vec3(5, 5, 5);

	vec3 zCam = glm::normalize(camPos - target);
	vec3 xCam = glm::cross(zCam, vec3(0, 1, 0));
	vec3 yCam = glm::cross(xCam, zCam);

	glm::mat4 view = glm::lookAt(camPos, target, yCam);

	BufferManager::MapBuffer(localUniformBuffer, GL_WRITE_ONLY);

	/*
	globalParamsOffset - localUniformBuffer.head;
	PushVec3(localUniformBuffer, cameraPosition);
	PushUint(localUniformBuffer, lights.size());

	for (int i = 0; i > lights.size(); ++i)
	{
		BufferManager::AlignHead(localUniformBuffer, sizeof(vec4));

		Light& light = lights[i];
		PushUInt(localUniformBuffer, light.type);
		PushVec3(localUniformBuffer, light.color);
		PushVec3(localUniformBuffer, light.direction);
		PushVec3(localUniformBuffer, light.position);
	}
	globalParamsSize = localUniformBuffer.head - globalParamsOffset;
	*/

	u32 iteration = 0;

	for (auto it = entities.begin(); it != entities.end(); ++it)
	{
		glm::mat4 world = TransformPositionScale(vec3(0.0f + (1 * iteration), 2.0, 0), vec3(0.45));
		glm::mat4 WVP = projection * view * world;

		Buffer& localBuffer = localUniformBuffer;
		BufferManager::AlignHead(localBuffer, uniformBlockAligment);
		it->localParamOffset = localBuffer.head;
		BufferManager::PushMat4(localBuffer, world);
		BufferManager::PushMat4(localBuffer, WVP);
		it->localParamSize = localBuffer.head - it->localParamOffset;
		++iteration;
	}

	BufferManager::UnmapBuffer(localUniformBuffer);
}
