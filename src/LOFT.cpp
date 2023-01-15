#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "LOFT.h"
#include "print_screen.h"

const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

const std::string obj_save_name = "six_basic.obj";
const std::string modelRelPath = "obj/Bedroom.obj";
const std::vector<std::string> paintingsTexturePath = { "texture/paintings1.png",
								"texture/paintings2.png", "texture/paintings3.png" };
const std::string reference_bmp = "bmp/dummy.bmp";
const std::string print_screen = "print_screen.bmp";

LOFT::LOFT(const Options& options) : Application(options) {

	// init model
	_loft.reset(new Model(getAssetFullPath(modelRelPath)));
	BoundingBox box = _loft->getBoundingBox();
	box.min = glm::vec3(_loft->transform.getLocalMatrix() * glm::vec4(box.min, 1.0f));
	box.max = glm::vec3(_loft->transform.getLocalMatrix() * glm::vec4(box.max, 1.0f));

	// init lights
	_ambientLight.reset(new AmbientLight);
	_ambientLight->intensity = 0.3f;

	_directionalLight.reset(new DirectionalLight);
	_directionalLight->transform.position = glm::vec3(1.0f);
	_directionalLight->intensity = 1.0f;

	_spotLight.reset(new SpotLight);
	_spotLight->intensity = 1.0f;
	_spotLight->transform.position = glm::vec3(0.0f, 0.0f, -10.0f);
	_spotLight->transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	_spotLight->kq = 0.5f;

	// init texture
	for (auto texture : paintingsTexturePath) {
		_paintingsTexture.push_back(std::make_shared<ImageTexture2D>(getAssetFullPath(texture)));
	}

	// init cameras
	const float aspect = 1.0f * _windowWidth / _windowHeight;
	constexpr float znear = 0.1f;
	constexpr float zfar = 10000.0f;

	// perspective camera
	_camera.reset(new PerspectiveCamera(
		glm::radians(60.0f), aspect, 0.1f, 10000.0f));
	_camera->transform.position = glm::vec3((box.min.x + box.max.x) / 2.0f, (box.min.y + box.max.y) / 2.0f, 5.0f);

	// init six basic elements
	_six_basic.resize(6);
	_six_basic[0].reset(new Cube(_camera->transform.position + glm::vec3(-1.0f, 0.0f, 1.73f) * 0.1f, 0.05f));
	_six_basic[1].reset(new Cone(_camera->transform.position + glm::vec3(1.0f, 0.0f, 1.73f) * 0.1f, 0.025f, 0.05f));
	_six_basic[2].reset(new Cylinder(_camera->transform.position + glm::vec3(2.0f, 0.0f, 0.0f) * 0.1f, 0.025f, 0.05f));
	_six_basic[3].reset(new Sphere(_camera->transform.position + glm::vec3(1.0f, 0.0f, -1.73f) * 0.1f, 0.025f));
	_six_basic[4].reset(new Prism(_camera->transform.position + glm::vec3(-1.0f, 0.0f, -1.73f) * 0.1f, 3, 0.025f, 0.05f));
	_six_basic[5].reset(new Frust(_camera->transform.position + glm::vec3(-2.0f, 0.0f, 0.0f) * 0.1f, 3, 0.025f, 0.05f, 0.05f));
	for (int i = 0; i < _six_basic.size(); ++i) {
		glm::mat4 translation = glm::mat4(1.0f);
		translation = glm::translate(translation, _six_basic[i]->_global_position);
		glm::mat4 rotation_self = glm::mat4(1.0f);
		rotation_self = glm::rotate(rotation_self, _six_basic[i]->_rotate_angle_self, glm::vec3(-1.0f)); // resolve around z-axis
		glm::mat4 scale = glm::mat4(1.0f);
		scale = glm::scale(scale, _six_basic[i]->_scale);
		glm::mat4 six_model = translation * rotation_self * scale;
		_six_basic[i]->_model = six_model;
	}

	// init shader
	initShader();

	// init depth map resources
	_depthMapFbo.reset(new Framebuffer);
	_depthMap.reset(new Texture2D(GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, GL_DEPTH_COMPONENT, GL_FLOAT));
	_depthMapFbo->bind();
	_depthMapFbo->attachTexture(*_depthMap, GL_DEPTH_ATTACHMENT);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	_depthMapFbo->unbind();

	// init fullscreen quad
	_fullscreenQuad.reset(new FullscreenQuad);

	// transformation matrix for the light
	_lightProjection = glm::ortho(-4.0f * aspect, 4.0f * aspect, -4.0f, 4.0f, znear, zfar);
	_lightView = glm::lookAt(_directionalLight->transform.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// init NURBS
	_NURBS.reset(new NURBS());

	// init imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(_window, true);
	ImGui_ImplOpenGL3_Init();
}

LOFT::~LOFT() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void LOFT::handleInput() {
	constexpr float cameraMoveSpeed = 0.05f;
	constexpr float cameraRotateSpeed = 0.02f;
	constexpr float cameraZoomRate = 0.05f;

	if (_input.keyboard.keyStates[GLFW_KEY_ESCAPE] != GLFW_RELEASE) {
		glfwSetWindowShouldClose(_window, true);
		return;
	}

	if (_input.mouse.scroll.yOffset) {
		// zoom in / out
		if (_camera->fovy >= glm::radians(1.0f) && _camera->fovy <= glm::radians(90.0f))
			_camera->fovy -= _input.mouse.scroll.yOffset * cameraZoomRate;
		if (_camera->fovy <= glm::radians(1.0f))
			_camera->fovy = glm::radians(1.0f);
		if (_camera->fovy >= glm::radians(90.0f))
			_camera->fovy = glm::radians(90.0f);
	}

	if (_input.keyboard.keyStates[GLFW_KEY_W] != GLFW_RELEASE) {
		// move the camera in its front direction
		_camera->transform.position += glm::normalize(_camera->transform.getFront()) * cameraMoveSpeed;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_A] != GLFW_RELEASE) {
		// move the camera in its left direction
		_camera->transform.position -= glm::normalize(_camera->transform.getRight()) * cameraMoveSpeed;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_S] != GLFW_RELEASE) {
		// move the camera in its back direction
		_camera->transform.position -= glm::normalize(_camera->transform.getFront()) * cameraMoveSpeed;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_D] != GLFW_RELEASE) {
		// move the camera in its right direction
		_camera->transform.position += glm::normalize(_camera->transform.getRight()) * cameraMoveSpeed;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_T] == GLFW_PRESS) {
		// change the texture of the paintings
		std::cout << "change texture" << std::endl;
		_current_texture = (_current_texture + 1) % _paintingsTexture.size();
		_input.keyboard.keyStates[GLFW_KEY_T] = GLFW_RELEASE;
		return;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_P] == GLFW_PRESS) {
		// print screen
		std::string err;
		if (!printScreen(getAssetFullPath(reference_bmp), getAssetFullPath(print_screen),
			_windowHeight, _windowWidth, &err)) {
			throw std::runtime_error("print screen failure: " + err);
		}
		std::cout << "screen shot has been saved to " << getAssetFullPath(print_screen) << std::endl;
		_input.keyboard.keyStates[GLFW_KEY_P] = GLFW_RELEASE;
		return;
	}

	if (_input.keyboard.keyStates[GLFW_KEY_O] == GLFW_PRESS) {
		// save the obj
		std::string err;
		std::string obj_save_path = getAssetFullPath(obj_save_name);
		for (int i = 0; i < _six_basic.size(); ++i) {
			if (!_six_basic[i]->SaveObj(obj_save_path, _camera->getViewMatrix(), &err))
			{
				throw std::runtime_error("save obj " + obj_save_path + " failure: " + err);
			}
		}
		std::cout << "obj has been saved to " << obj_save_path << std::endl;
		_input.keyboard.keyStates[GLFW_KEY_O] = GLFW_RELEASE;
		return;
	}

	if (_input.mouse.press.right == true) {

		if (_input.mouse.move.xNow != _input.mouse.move.xOld) {
			// rotate the camera around world up: glm::vec3(0.0f, 1.0f, 0.0f)
			float theta = (_input.mouse.move.xNow - _input.mouse.move.xOld) * cameraRotateSpeed;
			// q = cos(theta/2) + u¡¤sin(theta/2)
			glm::vec3 tmp = glm::normalize(_camera->transform.getUp())
				* sin(glm::radians(theta / 2.0f));
			glm::quat q(cos(glm::radians(theta / 2.0f)), tmp.x, tmp.y, tmp.z);
			_camera->transform.rotation = q * _camera->transform.rotation;
		}

		if (_input.mouse.move.yNow != _input.mouse.move.yOld) {
			// rotate the camera around its local right
			float theta = (_input.mouse.move.yNow - _input.mouse.move.yOld) * cameraRotateSpeed;
			// q = cos(theta/2) + u¡¤sin(theta/2)
			glm::vec3 tmp = glm::normalize(_camera->transform.getRight())
				* sin(glm::radians(theta / 2.0f));
			glm::quat q(cos(glm::radians(theta / 2.0f)), tmp.x, tmp.y, tmp.z);
			_camera->transform.rotation = q * _camera->transform.rotation;
		}
	}

	const float angulerVelocity = 1.0f;
	const float scaleRate = 0.25f;
	static float flag = 1.0f;

	if (_input.keyboard.keyStates[GLFW_KEY_6] != GLFW_RELEASE) {
		_show_six_basic = true;
		for (int i = 0; i < _six_basic.size(); ++i)
		{
			switch (i) {
			case 0:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(-1.0f, 0.0f, 1.73f) * 0.1f;
				break;
			case 1:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(1.0f, 0.0f, 1.73f) * 0.1f;
				break;
			case 2:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(2.0f, 0.0f, 0.0f) * 0.1f;
				break;
			case 3:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(1.0f, 0.0f, -1.73f) * 0.1f;
				break;
			case 4:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(-1.0f, 0.0f, -1.73f) * 0.1f;
				break;
			case 5:
				_six_basic[i]->_global_position = _camera->transform.position + glm::vec3(-2.0f, 0.0f, 0.0f) * 0.1f;
				break;
			default:;
			}
			if (_input.keyboard.keyStates[GLFW_KEY_R] != GLFW_RELEASE) {
				if (_six_basic[i]->_scale.x < 0.8f)
					flag = 1.0f;
				else if (_six_basic[i]->_scale.x > 1.0f)
					flag = -1.0f;
				_six_basic[i]->_scale += flag * _deltaTime * scaleRate;
			}
			else {
				_six_basic[i]->_rotate_angle_self += angulerVelocity * _deltaTime;
				_six_basic[i]->_rotate_angle_camera += angulerVelocity * _deltaTime;
			}
		}
	}
	else _show_six_basic = false;

	static bool pointMove = false;
	static int pointToMove = -1;
	if (_drawNURBS) {
		float x = _input.mouse.move.xNow;
		x = x * 2.f / _windowWidth - 1;
		float y = _input.mouse.move.yNow;
		y = -(y * 2.f / _windowHeight - 1);

		if (_input.mouse.press.right == true) {
			if (!pointMove) {
				float pointSize = 0.04f;

				int i = 0;
				for (i; i < _NURBS->_controlPoints.size(); ++i) {
					if (glm::distance(glm::vec2(x, y), _NURBS->_controlPoints[i])
						<= pointSize * std::max(0.5f, _NURBS->_weights[i])) {
						// move the existing point
						pointMove = true;
						pointToMove = i;
						break;
					}
				}
				if (i == _NURBS->_controlPoints.size()) {
					// add a new point
					_NURBS->_controlPoints.push_back(glm::vec2(x, y));
					_NURBS->_weights.push_back(1.f);
				}
			}
			else {
				// update point position
				_NURBS->_controlPoints[pointToMove] = glm::vec2(x, y);
				pointMove = false;
				pointToMove = -1;
			}

			_input.mouse.press.right = false;
		}

		// update data buffers
		if (_NURBS->_controlPoints.size() >= 1)
			_NURBS->generateControlPointsBuffers();
		if (_NURBS->_controlPoints.size() >= 2)
			_NURBS->generateSplineBuffers();
	}

	_input.forwardState();
}

void LOFT::renderFrame() {
	showFpsInWindowTitle();

	// the 1st pass: generate depth map
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	_depthMapFbo->bind();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	// configure view and projection matrix
	_depthMapShader->use();
	_depthMapShader->setUniformMat4("projection", _lightProjection);
	_depthMapShader->setUniformMat4("view", _lightView);
	_depthMapShader->setUniformMat4("model", _loft->transform.getLocalMatrix());

	glCullFace(GL_FRONT);
	_loft->draw();
	glCullFace(GL_BACK);

	// the 2nd pass: apply depth map
	_depthMapFbo->unbind();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, _windowWidth, _windowHeight);
	glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	
	// glDisable(GL_DEPTH_TEST);
	// _depthMapTestShader->use();
	// _depthMap->bind();
	// _fullscreenQuad->draw();
	
	glm::mat4 projection = _camera->getProjectionMatrix();
	glm::mat4 view = _camera->getViewMatrix();

	// draw the loft
	_loft_shader->use();
	_loft_shader->setUniformMat4("projection", projection);
	_loft_shader->setUniformMat4("view", view);
	_loft_shader->setUniformMat4("model", _loft->transform.getLocalMatrix());
	_loft_shader->setUniformMat4("lightSpaceMatrix", _lightProjection * _lightView);
	_loft_shader->setUniformBool("mode", _shadow);
	
	for (int i = 0; i < _loft->_materials.size(); ++i) {
		glm::vec3 vec;
		vec = glm::vec3(_loft->_materials[i].ka[0], _loft->_materials[i].ka[1], _loft->_materials[i].ka[2]);
		_loft_shader->setUniformVec3("materials[" + std::to_string(i) + "].ka", vec);
		vec = glm::vec3(_loft->_materials[i].kd[0], _loft->_materials[i].kd[1], _loft->_materials[i].kd[2]);
		_loft_shader->setUniformVec3("materials[" + std::to_string(i) + "].kd", vec);
		vec = glm::vec3(_loft->_materials[i].ks[0], _loft->_materials[i].ks[1], _loft->_materials[i].ks[2]);
		_loft_shader->setUniformVec3("materials[" + std::to_string(i) + "].ks", vec);

		_loft_shader->setUniformFloat("materials[" + std::to_string(i) + "].ns", _loft->_materials[i].ns);
	}

	// light attributes
	_loft_shader->setUniformVec3("spotLight.position", _spotLight->transform.position);
	_loft_shader->setUniformVec3("spotLight.direction", _spotLight->transform.getFront());
	_loft_shader->setUniformFloat("spotLight.intensity", _spotLight->intensity);
	_loft_shader->setUniformVec3("spotLight.color", _spotLight->color);
	_loft_shader->setUniformFloat("spotLight.angle", _spotLight->angle);
	_loft_shader->setUniformFloat("spotLight.kc", _spotLight->kc);
	_loft_shader->setUniformFloat("spotLight.kl", _spotLight->kl);
	_loft_shader->setUniformFloat("spotLight.kq", _spotLight->kq);
	_loft_shader->setUniformVec3("directionalLight.direction", -_directionalLight->transform.position);
	_loft_shader->setUniformFloat("directionalLight.intensity", _directionalLight->intensity);
	_loft_shader->setUniformVec3("directionalLight.color", _directionalLight->color);
	_loft_shader->setUniformVec3("ambientLight.color", _ambientLight->color);
	_loft_shader->setUniformFloat("ambientLight.intensity", _ambientLight->intensity);

	// enable textures
	_paintingsTexture[_current_texture]->bind(0);
	_loft_shader->setUniformInt("mapKd", 0);
	_depthMap->bind(1);
	_loft_shader->setUniformInt("shadowMap", 1);

	_loft->draw();

	if (_show_six_basic) { // draw six basics

		_six_basic_shader->use();

		for (int i = 0; i < _six_basic.size(); ++i) {
			_six_basic_shader->setUniformMat4("projection", projection);
			_six_basic_shader->setUniformMat4("view", view);

			glm::mat4 rotation_cam = glm::mat4(1.0f);
			rotation_cam = glm::rotate(rotation_cam, _six_basic[i]->_rotate_angle_camera, _camera->transform.getUp());
			_six_basic_shader->setUniformMat4("rotation", rotation_cam); // revolve round the camera

			glm::mat4 translation = glm::mat4(1.0f);
			translation = glm::translate(translation, _six_basic[i]->_global_position);
			glm::mat4 rotation_self = glm::mat4(1.0f);
			rotation_self = glm::rotate(rotation_self, _six_basic[i]->_rotate_angle_self, glm::vec3(-1.0f)); // resolve around z-axis
			glm::mat4 scale = glm::mat4(1.0f);
			scale = glm::scale(scale, _six_basic[i]->_scale);
			glm::mat4 six_model = translation * rotation_self * scale;
			_six_basic[i]->_model = six_model;
			_six_basic_shader->setUniformMat4("model", six_model);
			_six_basic[i]->draw();
		}
	}

	if (_drawNURBS) {
		_NURBS->draw();
	}

	// draw ui elements
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	const auto flags =
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::Begin("Control Panel", nullptr, flags)) {
		ImGui::End();
	}
	else {
		ImGui::Checkbox("shadow mapping", (bool*)&_shadow);
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::Text("ambient light");
		ImGui::Separator();
		ImGui::SliderFloat("intensity##1", &_ambientLight->intensity, 0.0f, 1.0f);
		ImGui::ColorEdit3("color##1", (float*)&_ambientLight->color);
		ImGui::NewLine();

		ImGui::Text("directional light");
		ImGui::Separator();
		ImGui::SliderFloat("intensity##2", &_directionalLight->intensity, 0.0f, 1.0f);
		ImGui::ColorEdit3("color##2", (float*)&_directionalLight->color);
		ImGui::SliderFloat("position.x##2", (float*)&_directionalLight->transform.position.x, -10.0f, 10.0f, "%f");
		ImGui::SliderFloat("position.y##2", (float*)&_directionalLight->transform.position.y, -10.0f, 10.0f, "%f");
		ImGui::SliderFloat("position.z##2", (float*)&_directionalLight->transform.position.z, -10.0f, 10.0f, "%f");
		ImGui::NewLine();

		ImGui::Text("spot light");
		ImGui::Separator();
		ImGui::SliderFloat("intensity##3", &_spotLight->intensity, 0.0f, 1.0f);
		ImGui::ColorEdit3("color##3", (float*)&_spotLight->color);
		ImGui::SliderFloat("angle##3", (float*)&_spotLight->angle, 0.0f, glm::radians(180.0f), "%f rad");
		ImGui::NewLine();

		ImGui::Checkbox("NURBS##4", (bool*)&_drawNURBS);
		ImGui::SliderInt("order##4", (int*)&_NURBS->_order, 2, std::max(2, static_cast<int>(_NURBS->_controlPoints.size())), "%d");
		ImGui::NewLine();

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	_lightView = glm::lookAt(_directionalLight->transform.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void LOFT::initShader() {
	const char* six_basics_vs = 
		"#version 330 core\n"
		"layout(location = 0) in vec3 aPosition;\n"
		"uniform mat4 projection;\n"
		"uniform mat4 view;\n"
		"uniform mat4 model;\n"
		"uniform mat4 rotation;\n"
		"void main() {\n"
		"	gl_Position = projection * rotation * view * model * vec4(aPosition, 1.0f);\n"
		"}\n";

	const char* six_basics_fs =
		"#version 330 core\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = vec4(0.941f, 1.0f, 0.941f, 1.0f);\n"
		"}\n";

	_six_basic_shader.reset(new GLSLProgram);
	_six_basic_shader->attachVertexShader(six_basics_vs);
	_six_basic_shader->attachFragmentShader(six_basics_fs);
	_six_basic_shader->link();

	const char* loft_vs =
		"#version 330 core\n"
		"layout(location = 0) in vec3 aPosition;\n"
		"layout(location = 1) in vec3 aNormal;\n"
		"layout(location = 2) in vec2 aTexCoord;\n"
		"layout(location = 3) in int aMaterial;\n"

		"out vec3 fPosition;\n"
		"out vec3 fNormal;\n"
		"out vec2 fTexCoord;\n"
		"out vec4 fPositionLightSpace;\n"
		"flat out int material_id;\n"

		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"uniform mat4 lightSpaceMatrix;\n"

		"void main() {\n"
		"	fPosition = vec3(model * vec4(aPosition, 1.0f));\n"
		"	fPositionLightSpace = lightSpaceMatrix * vec4(fPosition, 1.0f);\n"
		"	fNormal = mat3(transpose(inverse(model))) * aNormal;\n"
		"	fTexCoord = aTexCoord;\n"
		"	gl_Position = projection * view * model * vec4(aPosition, 1.0f);\n"
		"	material_id = aMaterial;\n"
		"}\n";

	const char* loft_fs = 
		"#version 330 core\n"
		"in vec3 fPosition;\n"
		"in vec3 fNormal;\n"
		"in vec2 fTexCoord;\n"
		"in vec4 fPositionLightSpace;\n"
		"flat in int material_id;\n"
		"out vec4 color;\n"

		"// ambient light data structure declaration\n"
		"struct AmbientLight {\n"
		"	vec3 color;\n"
		"	float intensity;\n"
		"};\n"

		"// directional light data structure declaration\n"
		"struct DirectionalLight {\n"
		"	vec3 direction;\n"
		"	float intensity;\n"
		"	vec3 color;\n"
		"};\n"

		"// spot light data structure declaration\n"
		"struct SpotLight {\n"
		"	vec3 position;\n"
		"	vec3 direction;\n"
		"	float intensity;\n"
		"	vec3 color;\n"
		"	float angle;\n"
		"	float kc;\n"
		"	float kl;\n"
		"	float kq;\n"
		"};\n"

		"// material data structure declaration\n"
		"struct Material {\n"
		"	vec3 ka;\n"
		"	vec3 kd;\n"
		"	vec3 ks;\n"
		"	float ns;\n"
		"};\n"

		"// uniform variables\n"
		"uniform DirectionalLight directionalLight;\n"
		"uniform SpotLight spotLight;\n"
		"uniform AmbientLight ambientLight;\n"
		"uniform vec3 cameraPosition;\n"
		"uniform Material materials[20];\n"
		"uniform sampler2D mapKd;\n"
		"uniform sampler2D shadowMap;\n"
		"uniform bool mode;\n"

		"vec3 calcDirectionalLight_diffuse(vec3 normal) {\n"
		"	vec3 lightDir = normalize(-directionalLight.direction);\n"
		"	vec3 diffuse = directionalLight.color * max(dot(lightDir, normal), 0.0f) * materials[material_id].kd;\n"
		"	return directionalLight.intensity * diffuse ;\n"
		"}\n"

		"vec3 calcDirectionalLight_specular(vec3 normal) {\n"
		"	vec3 lightDir = normalize(-directionalLight.direction);\n"
		"	vec3 reflectDir = reflect(-lightDir, normal);\n"
		"	vec3 viewDir = cameraPosition - fPosition;\n"
		"	float spec = pow(max(dot(normalize(viewDir), normalize(reflectDir)), 0.0), materials[material_id].ns);\n"
		"	return directionalLight.intensity * directionalLight.color * spec * materials[material_id].ks;\n"
		"}\n"

		"vec3 calcSpotLight_diffuse(vec3 normal) {\n"
		"	vec3 lightDir = normalize(spotLight.position - fPosition);\n"
		"	float theta = acos(-dot(lightDir, normalize(spotLight.direction)));\n"
		"	if (theta > spotLight.angle) {\n"
		"		return vec3(0.0f, 0.0f, 0.0f);\n"
		"	}\n"
		"	vec3 diffuse = spotLight.color * max(dot(lightDir, normal), 0.0f) * materials[material_id].kd;\n"
		"	float distance = length(spotLight.position - fPosition);\n"
		"	float attenuation = 1.0f / (spotLight.kc + spotLight.kl * distance + spotLight.kq * distance * distance);\n"
		"	return spotLight.intensity * attenuation * diffuse;\n"
		"}\n"

		"vec3 calcSpotLight_specular(vec3 normal) {\n"
		"	vec3 lightDir = normalize(spotLight.position - fPosition);\n"
		"	float theta = acos(-dot(lightDir, normalize(spotLight.direction)));\n"
		"	if (theta > spotLight.angle) {\n"
		"		return vec3(0.0f, 0.0f, 0.0f);\n"
		"	}\n"
		"	vec3 reflectDir = reflect(-lightDir, normal);\n"
		"	vec3 viewDir = cameraPosition - fPosition;\n"
		"	float spec = pow(max(dot(normalize(viewDir), normalize(reflectDir)), 0.0), materials[material_id].ns);\n"
		"	float distance = length(spotLight.position - fPosition);\n"
		"	float attenuation = 1.0f / (spotLight.kc + spotLight.kl * distance + spotLight.kq * distance * distance);\n"
		"	return spotLight.intensity * distance * attenuation * spotLight.color * spec * materials[material_id].ks;\n"
		"}\n"

		"float shadowCalculation(vec4 fragPosLightSpace) {\n"
		"	// perspective division\n"
		"	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;\n"
		"	// from [-1,1] to [0,1]\n"
		"	projCoords = projCoords * 0.5 + 0.5;\n"
		"	float closestDepth = texture(shadowMap, projCoords.xy).r;\n"
		"	float currentDepth = projCoords.z;\n"
		"	float bias = 0.000005;\n"
		"	float shadow = 0.0;\n"
		"	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);\n"
		"	for(int x = -1; x <= 1; ++x) {\n"
		"		for(int y = -1; y <= 1; ++y) {\n"
		"			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;\n"
		"			shadow += currentDepth - bias > pcfDepth ? 0.9 : 0.0;\n"
		"		}\n"
		"	}\n"
		"	shadow /= 9.0;\n"
		"	return shadow;\n"
		"}\n"
		
		"void main() {\n"
		"	vec3 ambient = materials[material_id].ka * ambientLight.color * ambientLight.intensity;\n"
		"	vec3 normal = normalize(fNormal);\n"
		"	vec3 diffuse = calcDirectionalLight_diffuse(normal) + calcSpotLight_diffuse(normal);\n"
		"	vec3 specular = calcDirectionalLight_specular(normal) + calcSpotLight_specular(normal);\n"
		"	float shadow = shadowCalculation(fPositionLightSpace);\n"
		"	vec4 coef;\n"
		"	if(mode)\n"
		"		coef = vec4(ambient + (1.0 - shadow) * (diffuse + specular), 1.0f);\n"
		"	else\n"
		"		coef = vec4(ambient + diffuse + specular, 1.0f);\n"
		"	color = material_id == 11 ? coef * texture(mapKd, fTexCoord) : coef;\n"
		"}\n";

	_loft_shader.reset(new GLSLProgram);
	_loft_shader->attachVertexShader(loft_vs);
	_loft_shader->attachFragmentShader(loft_fs);
	_loft_shader->link();

	// shader for depth mapping
	const char* shadow_vs =
		"#version 330 core\n"
		"layout(location = 0) in vec3 aPosition;\n"

		"uniform mat4 projection;\n"
		"uniform mat4 view;\n"
		"uniform mat4 model;\n"

		"void main() {\n"
		"	gl_Position = projection * view * model * vec4(aPosition, 1.0f);\n"
		"}\n";

	const char* shadow_fs = 
		"#version 330 core\n"
		"void main() {\n"
		"	// gl_FragDepth = gl_FragCoord.z;\n"
		"}\n";

	_depthMapShader.reset(new GLSLProgram);
	_depthMapShader->attachVertexShader(shadow_vs);
	_depthMapShader->attachFragmentShader(shadow_fs);
	_depthMapShader->link();

	const char* quad_vs =
		"#version 330 core\n"
		"layout(location = 0) in vec2 aPosition;\n"
		"layout(location = 1) in vec2 aTexCoords;\n"

		"out vec2 fTexCoords;\n"

		"void main() {\n"
		"	fTexCoords = aTexCoords;\n"
		"	gl_Position = vec4(aPosition, 0.0f, 1.0f);\n"
		"}\n";

	const char* quad_fs = 
		"#version 330 core\n"
		"in vec2 fTexCoords;\n"
		"out vec4 color;\n"

		"uniform sampler2D depthMap;\n"

		"void main() {\n"
		"	float depthValue = texture(depthMap, fTexCoords).r;\n"
		"	color = vec4(vec3(depthValue), 1.0);\n"
		"}\n";

	_depthMapTestShader.reset(new GLSLProgram);
	_depthMapTestShader->attachVertexShader(quad_vs);
	_depthMapTestShader->attachFragmentShader(quad_fs);
	_depthMapTestShader->link();
}