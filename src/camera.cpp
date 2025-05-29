#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix()
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 cameraRotaion = getRotationMatrix();

	return glm::inverse(cameraTranslation * cameraRotaion);
}

glm::mat4 Camera::getRotationMatrix()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3(0.0f, -1.0f, 0.0f));

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::processSDLEvent(SDL_Event& e)
{
	if (e.type == SDL_KEYDOWN)
	{
		if (e.key.keysym.sym == SDLK_w) { velocity.z = -1; }
		if (e.key.keysym.sym == SDLK_s) { velocity.z = 1; }
		if (e.key.keysym.sym == SDLK_a) { velocity.x = -1; }
		if (e.key.keysym.sym == SDLK_d) { velocity.x = 1; }
	}

	if (e.type == SDL_KEYUP)
	{
		if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
		if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
		if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
		if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
	}

	if (e.type == SDL_MOUSEMOTION)
	{
		yaw += (float)e.motion.xrel / 200.0f;
		pitch -= (float)e.motion.yrel / 200.0f;
	}
}

void Camera::update()
{
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.0));
}
