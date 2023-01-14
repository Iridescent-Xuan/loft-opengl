#pragma once

#include <memory>
#include <vector>

#include "./base/application.h"
#include "./base/glsl_program.h"
#include "./base/camera.h"
#include "./base/light.h"
#include "./base/texture2d.h"
#include "./base/framebuffer.h"
#include "./base/fullscreen_quad.h"

#include "model.h"
#include "six_basic.h"

class LOFT : public Application {
public:
	LOFT(const Options& options);

	~LOFT();

	void handleInput() override;

	void renderFrame() override;

private:
	std::unique_ptr<PerspectiveCamera> _camera;

	std::unique_ptr<Model> _loft;
	std::vector<std::unique_ptr<BaseGeo> > _six_basic;

	std::unique_ptr<GLSLProgram> _six_basic_shader;
	bool _show_six_basic = false;
	std::unique_ptr<GLSLProgram> _loft_shader;

	std::unique_ptr<AmbientLight> _ambientLight;
	std::unique_ptr<DirectionalLight> _directionalLight;
	std::unique_ptr<SpotLight> _spotLight;

	std::vector< std::shared_ptr<Texture2D> > _paintingsTexture;
	int _current_texture = 0;

	// depth mapping resources
	std::unique_ptr<Framebuffer> _depthMapFbo;
	std::unique_ptr<Texture2D> _depthMap;
	glm::mat4 _lightProjection;
	glm::mat4 _lightView;
	std::unique_ptr<GLSLProgram> _depthMapShader;
	bool _shadow = false;
	std::unique_ptr<FullscreenQuad> _fullscreenQuad;
	std::unique_ptr<GLSLProgram> _depthMapTestShader;

	void initShader();
};