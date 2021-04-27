#include <cmath>
#include "ClientConnection.h"
#include "server/GameServerEngine.h"

ClientConnection::ClientConnection(ServerTransport &transport): m_transport(transport) {
	m_logger = el::Loggers::getLogger("default");
}

void ClientConnection::updatePosition(const glm::vec3 &position, float yaw, float pitch, int viewRadius) {
	logger().trace(
			"[Client %v] updatePosition(x=%v, y=%v, z=%v, yaw=%v, pitch=%v, viewRadius=%v)",
			this, position.x, position.y, position.z, yaw, pitch, viewRadius
	);
	std::unique_lock<std::shared_mutex> lock(m_positionMutex);
	bool resetPosition = false;
	if (m_positionValid) {
		auto delta = position - m_position;
		static const float MAX_DELTA = 0.2f;
		if (fabsf(delta.x) >= MAX_DELTA || fabsf(delta.y) >= MAX_DELTA || fabsf(delta.z) >= MAX_DELTA) {
			logger().warn("[Client %v] player is moving too fast", this);
			resetPosition = true;
		}
	}
	if (!resetPosition) {
		m_position = position;
	}
	m_yaw = yaw;
	m_pitch = pitch;
	m_viewRadius = std::max(viewRadius, 3);
	m_positionValid = true;
	auto newPosition = m_position;
	auto radius = m_viewRadius;
	lock.unlock();
	if (resetPosition) {
		setPosition(newPosition);
	}
	sendUnloadedChunks(newPosition, radius);
}

void ClientConnection::sendUnloadedChunks(const glm::vec3 &position, int viewRadius) {
	auto x0 = (int) roundf(position.x / VOXEL_CHUNK_SIZE);
	auto y0 = (int) roundf(position.y / VOXEL_CHUNK_SIZE);
	auto z0 = (int) roundf(position.z / VOXEL_CHUNK_SIZE);
	for (int r = 0; r < viewRadius; r++) {
		for (int dz = -r; dz <= r; dz++) {
			for (int dy = -r; dy <= r; dy++) {
				for (int dx = -r; dx <= r; dx++) {
					VoxelChunkLocation location(x0 + dx, y0 + dy, z0 + dz);
					if (m_loadedChunks.find(location) == m_loadedChunks.end()) {
						m_loadedChunks.emplace(location);
						auto chunk = m_transport.engine()->voxelWorld().chunk(location);
						if (chunk) {
							logger().debug(
									"[Client %v] Sending chunk x=%v,y=%v,z=%v",
									this, location.x, location.y, location.z
							);
							setChunk(chunk);
						}
					}
				}
			}
		}
	}
}
