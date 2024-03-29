#include <stdexcept>
#include <sstream>
#include <easylogging++.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>
#include <GL/glew.h>
#include "GameEngine.h"
#include "world/VoxelTypes.h"
#include "world/VoxelWorldUtils.h"

GameEngine *GameEngine::s_instance = nullptr;

GameEngine &GameEngine::instance() {
	if (s_instance == nullptr) {
		throw std::runtime_error("Attempt to obtain GameEngine instance without active engine running");
	}
	return *s_instance;
}

GameEngine::GameEngine() {
	if (s_instance != nullptr) {
		throw std::runtime_error("Attempt to create more than one GameEngine instance");
	}
	s_instance = this;
}

GameEngine::~GameEngine() {
	if (m_transport) {
		m_transport->shutdown();
	}
	s_instance = nullptr;
}

bool GameEngine::init() {
	if (!platformInit()) {
		return false;
	}
	if (glewInit() != GLEW_OK) {
		LOG(ERROR) << "Failed to initialize GLEW";
		return false;
	}

	m_assetLoader = std::make_unique<AssetLoader>(prefix());
	m_commonShaderPrograms = std::make_unique<CommonShaderPrograms>(*m_assetLoader);
	m_font = std::make_unique<BitmapFont>(*m_assetLoader, "assets/fonts/ter-u32n.png");
	m_debugTextRenderer = std::make_unique<BitmapFontRenderer>(*m_font);
	m_userInterface = std::make_unique<UserInterface>();

	m_voxelTypeRegistry = std::make_unique<VoxelTypeRegistry>(*m_assetLoader);
	registerVoxelTypes(*m_voxelTypeRegistry, *m_assetLoader);
	m_voxelWorld = std::make_unique<VoxelWorld>(static_cast<VoxelChunkListener*>(this));
	m_voxelWorldRenderer = std::make_unique<VoxelWorldRenderer>(*m_voxelWorld);
	m_voxelOutline = std::make_unique<VoxelOutline>();
	
	m_playerType = std::make_unique<PlayerEntityType>();
	m_player = m_playerType->invokeNew(
			VoxelLocation(1, 1, -1),
			EntityOrientation { 45.0f, 0.0f, 0.0f }
	);
	m_player->mutableChunk(*m_voxelWorld, true).addEntity(m_player);
	
	/* m_cowTexture = std::make_unique<GL::Texture>(m_assetLoader->load("assets/textures/cow.png"));
	m_cowModel = std::make_unique<Model>(
			m_assetLoader->load("assets/models/cow.obj"),
			commonShaderPrograms().entity.texture, m_cowTexture.get()
	);
	m_cowEntity = std::make_unique<Entity>(
			*m_voxelWorld,
			glm::vec3(2.0f, 0.0f, 0.0f),
			0.0f,
			0.0f,
			2,
			2,
			0.25f,
			0.05f,
			m_cowModel.get()
	); */
	
	LOG(INFO) << "Game engine initialized";
	return true;
}

void GameEngine::quit() {
	m_running = false;
	LOG(INFO) << "Quitting...";
}

float GameEngine::viewportWidthOverHeight() const {
	return viewportHeight() > 0 ? (float) viewportWidth() / (float) viewportHeight() : 1.0f;
}

void GameEngine::handleResize(int width, int height) {
	m_viewportWidth = width;
	m_viewportHeight = height;
	LOG(INFO) << "Viewport set to " << width << "x" << height;
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	glDepthFunc(GL_LEQUAL);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	m_projection = glm::perspective(
			glm::radians(70.0f),
			viewportWidthOverHeight(),
			0.05f,
			100.0f
	);
}

void GameEngine::render() {
	m_userInterface->inventory().update();
	updatePlayerPosition();
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_viewportWidth, m_viewportHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	auto chunk = m_player->chunk(*m_voxelWorld, false);
	auto playerDirection = m_player->direction(true);
	auto playerPosition = m_player->position() + glm::vec3(
			0.0f,
			(float) m_player->physics().height() - 0.75f - m_player->physics().paddingY(),
			0.0f
	);
	auto view = glm::lookAt(
			playerPosition,
			playerPosition + playerDirection,
			m_player->upDirection()
	);
	if (chunk) {
		chunk.unlock();
	}
	
	m_voxelWorldRenderer->render(playerPosition, 2, view, m_projection);
	updatePointingAt(view);
	
	//glEnable(GL_DEPTH_TEST);
	
	m_voxelOutline->render(view, m_projection);
	
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	
	m_userInterface->render();
	if (m_showDebugInfo) {
		m_debugTextRenderer->render(-1.0f, 1.0f, 0.05f);
	}
	
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
	);
	auto renderTime = now - m_lastRenderAt;
	m_lastRenderAt = now;
	m_framePerSecond = 1000.0f / renderTime.count();
	
	updateDebugInfo();
}

void GameEngine::updatePointingAt(const glm::mat4 &view) {
	m_debugStr.clear();
	auto chunk = m_player->extendedChunk(*m_voxelWorld, false);
	if (!chunk) return;
	auto playerPosition = m_player->position() + glm::vec3(
			0.0f,
			(float) m_player->physics().height() - 0.75f - m_player->physics().paddingY(),
			0.0f
	);
	m_voxelOutline->set(chunk, findPlayerPointingAt(
			chunk,
			playerPosition,
			m_player->direction(true)
	));
}

void GameEngine::updateDebugInfo() {
	std::stringstream ss;
	ss << "FPS: " << m_framePerSecond << "\n";
	ss << "X=" << m_player->position().x << ", Y=" << m_player->position().y << ", Z=" << m_player->position().z;
	VoxelLocation playerLocation(
			(int) roundf(m_player->position().x),
			(int) roundf(m_player->position().y),
			(int) roundf(m_player->position().z)
	);
	auto playerChunkLocation = playerLocation.chunk();
	VoxelLightLevel lightLevel = MAX_VOXEL_LIGHT_LEVEL;
	{
		auto chunk = m_voxelWorld->chunk(playerChunkLocation);
		if (chunk) {
			lightLevel = chunk.at(playerLocation.inChunk()).lightLevel();
		}
	}
	auto playerInChunkLocation = playerLocation.inChunk();
	ss << " (chunk X=" << playerChunkLocation.x << ", Y=" << playerChunkLocation.y <<
			", Z=" << playerChunkLocation.z << ") (" <<
			"in-chunk X=" << playerInChunkLocation.x << ", Y=" << playerInChunkLocation.y <<
			", Z=" << playerInChunkLocation.z << ") lightLevel=" << (int) lightLevel;
	ss << " yaw=" << m_player->orientation().yaw << ", pitch=" << m_player->orientation().pitch << "\n";
	if (m_voxelOutline->voxelDetected()) {
		auto &l = m_voxelOutline->voxelLocation();
		ss << "Pointing at X=" << l.x << ",Y=" << l.y << ",Z=" << l.z << ": " << m_voxelOutline->text();
		auto &d = m_voxelOutline->direction();
		ss << " (direction X=" << d.x << ",Y=" << d.y << ",Z=" << d.z << ")\n";
	}
	ss << "Loaded " << m_voxelWorld->chunkCount() << " chunks";
	ss << " (" << m_voxelWorldRenderer->queueSize() << " chunk(s) in mesh build queue)\n";
	ss << "Used " << m_voxelWorldRenderer->usedBufferCount() << " voxel mesh buffers";
	ss << " (" << m_voxelWorldRenderer->availableBufferCount() << " available)\n";
	ss << "World render time (ms): " << m_voxelWorldRenderer->renderPerformanceCounter() << "\n";
	ss << "Chunk mesh build time (ms): " << m_voxelWorldRenderer->buildPerformanceCounter() << "\n";
	if (m_transport && m_transport->isConnected()) {
		ss << "Connected to the server\n";
	} else {
		ss << "!!! NOT CONNECTED TO THE SERVER !!!\n";
	}
	if (!m_debugStr.empty()) {
		ss << m_debugStr;
	}
	m_debugTextRenderer->setText(ss.str(), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void GameEngine::keyDown(KeyCode keyCode) {
	m_pressedKeys.insert(keyCode);
	switch (keyCode) {
		case KeyCode::TOGGLE_DEBUG_INFO:
			m_showDebugInfo = !m_showDebugInfo;
			break;
		case KeyCode::RESET_PERFORMANCE_COUNTERS:
			m_voxelWorldRenderer->renderPerformanceCounter().reset();
			m_voxelWorldRenderer->buildPerformanceCounter().reset();
			m_voxelWorldRenderer->reset();
			break;
		case KeyCode::SAVE_CHUNK_TEXTURE:
			m_voxelWorldRenderer->saveChunkTexture(m_player->position());
			break;
		case KeyCode::PRIMARY_CLICK:
			if (m_mouseClicked) break;
			m_mouseClicked = true;
			if (!m_voxelOutline->voxelDetected()) break;
			if (!m_transport) break;
			m_transport->digVoxel();
			break;
		case KeyCode::SECONDARY_CLICK:
			if (m_mouseSecondaryClicked) break;
			m_mouseSecondaryClicked = true;
			if (!m_voxelOutline->voxelDetected()) break;
			if (!m_transport) break;
			m_transport->placeVoxel();
			break;
		case KeyCode::INVENTORY_1:
		case KeyCode::INVENTORY_2:
		case KeyCode::INVENTORY_3:
		case KeyCode::INVENTORY_4:
		case KeyCode::INVENTORY_5:
		case KeyCode::INVENTORY_6:
		case KeyCode::INVENTORY_7:
		case KeyCode::INVENTORY_8:
			m_userInterface->inventory().setActive((int) keyCode - (int) KeyCode::INVENTORY_1);
			if (m_transport) {
				m_transport->updateActiveInventoryItem(m_userInterface->inventory().activeIndex());
			}
			break;
		default:;
	}
}

void GameEngine::keyUp(KeyCode keyCode) {
	m_pressedKeys.erase(keyCode);
	switch (keyCode) {
		case KeyCode::PRIMARY_CLICK:
			m_mouseClicked = false;
			break;
		case KeyCode::SECONDARY_CLICK:
			m_mouseSecondaryClicked = false;
			break;
		default:;
	}
}

void GameEngine::mouseWheel(int delta) {
	auto &inventory = m_userInterface->inventory();
	if (delta < 0) {
		inventory.setActive(inventory.activeIndex() < inventory.size() - 1 ? inventory.activeIndex() + 1 : 0);
	} else if (delta > 0) {
		inventory.setActive(inventory.activeIndex() > 0 ? inventory.activeIndex() - 1 : inventory.size() - 1);
	}
	if (m_transport) {
		m_transport->updateActiveInventoryItem(m_userInterface->inventory().activeIndex());
	}
}

void GameEngine::updatePlayerDirection(float dx, float dy) {
	static const float sensitivity = 100.0f;
	auto chunk = m_player->mutableChunk(*m_voxelWorld, false);
	if (!chunk) return;
	m_player->adjustRotation(dx * sensitivity, dy * sensitivity);
}

void GameEngine::updatePlayerMovement(const float *dx, const float *dy, const float *dz) {
	if (dx) {
		m_playerSpeed.x = *dx;
	}
	if (dy) {
		m_playerSpeed.y = *dy;
	}
	if (dz) {
		m_playerSpeed.z = *dz;
	}
}

void GameEngine::updatePlayerPosition() {
	float SPEED = m_pressedKeys.count(KeyCode::SPEEDUP) ? 6.0f : 3.0f; // player moving distance per second
	
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
	);
	float delta = (float) (now - m_lastPlayerPositionUpdateTime).count() / 1000.0f;
	m_lastPlayerPositionUpdateTime = now;
	
	glm::vec3 moveDirection = m_playerSpeed * SPEED * delta;
	if (m_pressedKeys.count(KeyCode::MOVE_FORWARD)) {
		moveDirection.z += SPEED * delta;
	}
	if (m_pressedKeys.count(KeyCode::MOVE_BACKWARD)) {
		moveDirection.z -= SPEED * delta;
	}
	if (m_pressedKeys.count(KeyCode::MOVE_LEFT)) {
		moveDirection.x -= SPEED * delta;
	}
	if (m_pressedKeys.count(KeyCode::MOVE_RIGHT)) {
		moveDirection.x += SPEED * delta;
	}
	if (m_pressedKeys.count(KeyCode::JUMP)) {
		moveDirection.y += SPEED * delta;
	}
	if (m_pressedKeys.count(KeyCode::CLIMB_DOWN)) {
		moveDirection.y -= SPEED * delta;
	}
	auto chunk = m_player->extendedMutableChunk(*m_voxelWorld, false);
	if (!chunk) return;
	m_player->move(chunk, moveDirection);
	
	if (m_transport) {
		auto position = m_player->position();
		auto yaw = m_player->orientation().yaw;
		auto pitch = m_player->orientation().pitch;
		chunk.unlock();
		m_transport->updatePlayerPosition(position, yaw, pitch, 2);
	}
}

void GameEngine::setPlayerPosition(const glm::vec3 &position) {
	auto chunk = m_player->extendedMutableChunk(*m_voxelWorld, true);
	m_player->setPosition(chunk, position);
}

void GameEngine::setTransport(std::unique_ptr<ClientTransport> transport) {
	if (m_transport) {
		m_transport->shutdown();
	}
	m_transport = std::move(transport);
	m_transport->start();
}

void GameEngine::chunkUnlocked(const VoxelChunkLocation &chunkLocation, VoxelChunkLightState lightState) {
	if (lightState != VoxelChunkLightState::READY) return;
	m_voxelWorldRenderer->invalidate(chunkLocation);
	m_voxelWorldRenderer->invalidate({chunkLocation.x - 1, chunkLocation.y, chunkLocation.z});
	m_voxelWorldRenderer->invalidate({chunkLocation.x, chunkLocation.y - 1, chunkLocation.z});
	m_voxelWorldRenderer->invalidate({chunkLocation.x, chunkLocation.y, chunkLocation.z - 1});
	m_voxelWorldRenderer->invalidate({chunkLocation.x + 1, chunkLocation.y, chunkLocation.z});
	m_voxelWorldRenderer->invalidate({chunkLocation.x, chunkLocation.y + 1, chunkLocation.z});
	m_voxelWorldRenderer->invalidate({chunkLocation.x, chunkLocation.y, chunkLocation.z + 1});
}
