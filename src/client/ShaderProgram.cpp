#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderProgram.h"
#include "OpenGL.h"
#include "Asset.h"

CommonShaderProgram::CommonShaderProgram(
		std::string name,
		const std::initializer_list<GL::Shader> &shaders
): ShaderProgram(std::move(name), shaders) {
	m_modelLocation = uniformLocation("model", true);
	m_viewLocation = uniformLocation("view", true);
	m_projectionLocation = uniformLocation("projection", true);
	m_texImageLocation = uniformLocation("texImage");
	m_colorUniformLocation = uniformLocation("uColor");

	m_positionLocation = attribLocation("position", true);
	m_lightLevelLocation = attribLocation("lightLevel");
	m_texCoordLocation = attribLocation("texCoord");
	m_colorLocation = attribLocation("color");
}

void CommonShaderProgram::setModel(const glm::mat4 &model) const {
	if (m_modelLocation < 0) return;
	glUniformMatrix4fv(m_modelLocation, 1, GL_FALSE, glm::value_ptr(model));
}

void CommonShaderProgram::setView(const glm::mat4 &view) const {
	if (m_viewLocation < 0) return;
	glUniformMatrix4fv(m_viewLocation, 1, GL_FALSE, glm::value_ptr(view));
}

void CommonShaderProgram::setProjection(const glm::mat4 &projection) const {
	if (m_projectionLocation < 0) return;
	glUniformMatrix4fv(m_projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));
}

void CommonShaderProgram::setTexImage(const GL::Texture &texImage) const {
	if (m_texImageLocation < 0) return;
	glActiveTexture(GL_TEXTURE0);
	texImage.bind();
	glUniform1i(m_texImageLocation, 0);
}

void CommonShaderProgram::setColorUniform(const glm::vec4 &color) const {
	if (m_colorUniformLocation < 0) return;
	glUniform4fv(m_colorUniformLocation, 1, glm::value_ptr(color));
}

void CommonShaderProgram::setPositions(const GL::BufferPointer &pointer) const {
	pointer.bind(m_positionLocation, 3);
}

void CommonShaderProgram::setLightLevels(const GL::BufferPointer &pointer) const {
	pointer.bind(m_lightLevelLocation, 1, true);
}

void CommonShaderProgram::setTexCoords(const GL::BufferPointer &pointer) const {
	pointer.bind(m_texCoordLocation, 2);
}

void CommonShaderProgram::setColors(const GL::BufferPointer &pointer) const {
	pointer.bind(m_colorLocation, 4);
}

CommonShaderPrograms::CommonShaderPrograms(AssetLoader &loader): texture("texture", {
		GL::Shader(GL_VERTEX_SHADER, loader.load("assets/shaders/texture_vertex.glsl")),
		GL::Shader(GL_FRAGMENT_SHADER, loader.load("assets/shaders/texture_fragment.glsl"))
}), color("color", {
		GL::Shader(GL_VERTEX_SHADER, loader.load("assets/shaders/color_vertex.glsl")),
		GL::Shader(GL_FRAGMENT_SHADER, loader.load("assets/shaders/color_fragment.glsl"))
}), font("font", {
		GL::Shader(GL_VERTEX_SHADER, loader.load("assets/shaders/font_vertex.glsl")),
		GL::Shader(GL_FRAGMENT_SHADER, loader.load("assets/shaders/font_fragment.glsl"))
}) {
}
