#include <easylogging++.h>
#include "VoxelWorld.h"

/* SharedVoxelChunk */

void SharedVoxelChunk::setNeighbors(
		const std::unordered_map<VoxelChunkLocation, std::unique_ptr<SharedVoxelChunk>> &chunks
) {
	for (int dz = -1; dz <= 1; dz++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				SharedVoxelChunk *chunk = nullptr;
				if (dx != 0 || dy != 0 || dz != 0) {
					auto it = chunks.find({
						location().x + dx,
						location().y + dy,
						location().z + dz
					});
					if (it == chunks.end()) continue;
					chunk = it->second.get();
					chunk->m_neighbors[(-dx + 1) + (-dy + 1) * 3 + (-dz + 1) * 3 * 3] = this;
				}
				m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3] = chunk;
			}
		}
	}
}

void SharedVoxelChunk::unsetNeighbors() {
	for (int dz = -1; dz <= 1; dz++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				auto chunk = m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3];
				if (chunk == nullptr) continue;
				chunk->m_neighbors[(-dx + 1) + (-dy + 1) * 3 + (-dz + 1) * 3 * 3] = nullptr;
			}
		}
	}
}

SharedVoxelChunk *SharedVoxelChunk::neighbor(int dx, int dy, int dz) const {
	return m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3];
}

/* VoxelChunkRef */

VoxelChunkRef::VoxelChunkRef(SharedVoxelChunk &chunk, bool lock): m_chunk(&chunk) {
	if (lock) {
		chunk.mutex().lock_shared();
	}
}

VoxelChunkRef::VoxelChunkRef(SharedVoxelChunk &chunk): VoxelChunkRef(chunk, true) {
}

VoxelChunkRef::VoxelChunkRef(VoxelChunkRef &&ref) noexcept: VoxelChunkRef(*ref.m_chunk, false) {
	ref.m_chunk = nullptr;
}

VoxelChunkRef &VoxelChunkRef::operator=(VoxelChunkRef &&ref) noexcept {
	unlock();
	m_chunk = ref.m_chunk;
	ref.m_chunk = nullptr;
	return *this;
}

VoxelChunkRef::~VoxelChunkRef() {
	unlock();
}

void VoxelChunkRef::unlock() {
	if (m_chunk) {
		m_chunk->mutex().unlock_shared();
		m_chunk = nullptr;
	}
}

/* VoxelChunkExtendedRef */

VoxelChunkExtendedRef::VoxelChunkExtendedRef(
		SharedVoxelChunk &chunk,
		bool lock,
		bool lockNeighbors
): VoxelChunkRef(chunk, lock) {
	for (int dz = -1; dz <= 1; dz++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				auto neighbor = chunk.neighbor(dx, dy, dz);
				m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3] = neighbor;
				if (neighbor == nullptr || !lockNeighbors) continue;
				neighbor->mutex().lock_shared();
			}
		}
	}
}

VoxelChunkExtendedRef::VoxelChunkExtendedRef(
		SharedVoxelChunk &chunk
): VoxelChunkExtendedRef(chunk, true, true) {
}

VoxelChunkExtendedRef::VoxelChunkExtendedRef(VoxelChunkExtendedRef &&ref) noexcept: VoxelChunkRef(std::move(ref)) {
	for (int i = 0; i < sizeof(m_neighbors) / sizeof(m_neighbors[0]); i++) {
		m_neighbors[i] = ref.m_neighbors[i];
		ref.m_neighbors[i] = nullptr;
	}
}

VoxelChunkExtendedRef &VoxelChunkExtendedRef::operator=(VoxelChunkExtendedRef &&ref) noexcept {
	unlock();
	for (int i = 0; i < sizeof(m_neighbors) / sizeof(m_neighbors[0]); i++) {
		m_neighbors[i] = ref.m_neighbors[i];
		ref.m_neighbors[i] = nullptr;
	}
	VoxelChunkRef::operator=(std::move(ref));
	return *this;
}

VoxelChunkExtendedRef::~VoxelChunkExtendedRef() {
	unlock();
}

void VoxelChunkExtendedRef::unlock() {
	for (auto &&neighbor : m_neighbors) {
		if (neighbor == nullptr) continue;
		neighbor->mutex().unlock_shared();
		neighbor = nullptr;
	}
	VoxelChunkRef::unlock();
}

bool VoxelChunkExtendedRef::hasNeighbor(int dx, int dy, int dz) const {
	return m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3] != nullptr;
}

const VoxelHolder &VoxelChunkExtendedRef::extendedAt(int x, int y, int z, VoxelLocation *outLocation) const {
	return extendedAt({x, y, z}, outLocation);
}

const VoxelHolder &VoxelChunkExtendedRef::extendedAt(const InChunkVoxelLocation &location, VoxelLocation *outLocation) const {
	const SharedVoxelChunk *chunk = m_chunk;
	VoxelChunkLocation chunkLocation = this->location();
	InChunkVoxelLocation correctedLocation = location;
	if (location.x < 0) {
		chunkLocation.x--;
		correctedLocation.x += VOXEL_CHUNK_SIZE;
	} else if (location.x >= VOXEL_CHUNK_SIZE) {
		chunkLocation.x++;
		correctedLocation.x -= VOXEL_CHUNK_SIZE;
	}
	if (location.y < 0) {
		chunkLocation.y--;
		correctedLocation.y += VOXEL_CHUNK_SIZE;
	} else if (location.y >= VOXEL_CHUNK_SIZE) {
		chunkLocation.y++;
		correctedLocation.y -= VOXEL_CHUNK_SIZE;
	}
	if (location.z < 0) {
		chunkLocation.z--;
		correctedLocation.z += VOXEL_CHUNK_SIZE;
	} else if (location.z >= VOXEL_CHUNK_SIZE) {
		chunkLocation.z++;
		correctedLocation.z -= VOXEL_CHUNK_SIZE;
	}
	int dx = chunkLocation.x - this->location().x;
	int dy = chunkLocation.y - this->location().y;
	int dz = chunkLocation.z - this->location().z;
	if (dx != 0 || dy != 0 || dz != 0) {
		chunk = m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3];
	}
	if (outLocation != nullptr) {
		*outLocation = VoxelLocation(chunkLocation, correctedLocation);
	}
	if (chunk) {
		return chunk->at(correctedLocation);
	}
	static const VoxelHolder empty;
	return empty;
}

/* VoxelChunkMutableRef */

VoxelChunkMutableRef::VoxelChunkMutableRef(
		SharedVoxelChunk &chunk,
		bool lockNeighbors
): VoxelChunkExtendedRef(chunk, false, lockNeighbors) {
	chunk.mutex().lock();
}

VoxelChunkMutableRef::VoxelChunkMutableRef(SharedVoxelChunk &chunk): VoxelChunkMutableRef(chunk, true) {
}

VoxelChunkMutableRef &VoxelChunkMutableRef::operator=(VoxelChunkMutableRef &&ref) noexcept {
	unlock();
	VoxelChunkExtendedRef::operator=(std::move(ref));
	return *this;
}

VoxelChunkMutableRef::~VoxelChunkMutableRef() {
	unlock();
}

void VoxelChunkMutableRef::unlock() {
	if (m_chunk) {
		auto &world = m_chunk->world();
		VoxelChunkLocation l;
		auto dirty = m_chunk->dirty();
		if (dirty) {
			l = location();
			m_chunk->clearDirty();
		}
		m_chunk->mutex().unlock();
		m_chunk = nullptr;
		if (dirty) {
			LOG(TRACE) << "Chunk at x=" << l.x << ",y=" << l.y << ",z=" << l.z << " invalidated";
			if (world.m_chunkListener) {
				world.m_chunkListener->chunkInvalidated(l);
			}
		}
	}
	VoxelChunkExtendedRef::unlock();
}

VoxelHolder &VoxelChunkMutableRef::at(int x, int y, int z) const {
	return m_chunk->at(x, y, z);
}

VoxelHolder &VoxelChunkMutableRef::at(const InChunkVoxelLocation &location) const {
	return m_chunk->at(location);
}

/* VoxelChunkExtendedMutableRef */

VoxelChunkExtendedMutableRef::VoxelChunkExtendedMutableRef(
		SharedVoxelChunk &chunk
): VoxelChunkMutableRef(chunk, false) {
	for (auto neighbor : m_neighbors) {
		if (neighbor == nullptr) continue;
		neighbor->mutex().lock();
	}
}

VoxelChunkExtendedMutableRef &VoxelChunkExtendedMutableRef::operator=(VoxelChunkExtendedMutableRef &&ref) noexcept {
	unlock();
	VoxelChunkMutableRef::operator=(std::move(ref));
	return *this;
}

VoxelChunkExtendedMutableRef::~VoxelChunkExtendedMutableRef() {
	unlock();
}

void VoxelChunkExtendedMutableRef::unlock() {
	for (auto &&neighbor : m_neighbors) {
		if (neighbor == nullptr) continue;
		neighbor->mutex().unlock();
		neighbor = nullptr;
	}
	VoxelChunkMutableRef::unlock();
}

VoxelHolder &VoxelChunkExtendedMutableRef::extendedAt(int x, int y, int z, VoxelLocation *outLocation) const {
	return extendedAt({x, y, z}, outLocation);
}

VoxelHolder &VoxelChunkExtendedMutableRef::extendedAt(
		const InChunkVoxelLocation &location,
		VoxelLocation *outLocation
) const {
	SharedVoxelChunk *chunk = m_chunk;
	VoxelChunkLocation chunkLocation = this->location();
	InChunkVoxelLocation correctedLocation = location;
	if (location.x < 0) {
		chunkLocation.x--;
		correctedLocation.x += VOXEL_CHUNK_SIZE;
	} else if (location.x >= VOXEL_CHUNK_SIZE) {
		chunkLocation.x++;
		correctedLocation.x -= VOXEL_CHUNK_SIZE;
	}
	if (location.y < 0) {
		chunkLocation.y--;
		correctedLocation.y += VOXEL_CHUNK_SIZE;
	} else if (location.y >= VOXEL_CHUNK_SIZE) {
		chunkLocation.y++;
		correctedLocation.y -= VOXEL_CHUNK_SIZE;
	}
	if (location.z < 0) {
		chunkLocation.z--;
		correctedLocation.z += VOXEL_CHUNK_SIZE;
	} else if (location.z >= VOXEL_CHUNK_SIZE) {
		chunkLocation.z++;
		correctedLocation.z -= VOXEL_CHUNK_SIZE;
	}
	int dx = chunkLocation.x - this->location().x;
	int dy = chunkLocation.y - this->location().y;
	int dz = chunkLocation.z - this->location().z;
	if (dx != 0 || dy != 0 || dz != 0) {
		chunk = m_neighbors[(dx + 1) + (dy + 1) * 3 + (dz + 1) * 3 * 3];
	}
	if (outLocation != nullptr) {
		*outLocation = VoxelLocation(chunkLocation, correctedLocation);
	}
	if (chunk) {
		return chunk->at(correctedLocation);
	}
	static VoxelHolder empty;
	return empty;
}

/* VoxelWorld */

VoxelWorld::VoxelWorld(
		VoxelChunkLoader *chunkLoader,
		VoxelChunkListener *chunkListener
): m_chunkLoader(chunkLoader), m_chunkListener(chunkListener) {
}

template<typename T> T VoxelWorld::createChunk(const VoxelChunkLocation &location) {
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_chunks.find(location);
	if (it != m_chunks.end()) {
		return T(*it->second);
	}
	auto chunkPtr = std::make_unique<SharedVoxelChunk>(*this, location);
	auto &chunk = *chunkPtr;
	chunk.setNeighbors(m_chunks);
	m_chunks.emplace(location, std::move(chunkPtr));
	return T(chunk);
}

template<typename T> T VoxelWorld::createAndLoadChunk(const VoxelChunkLocation &location) {
	auto chunk = createChunk<T>(location);
	m_chunkLoader->load(chunk);
	return chunk;
}

VoxelChunkRef VoxelWorld::chunk(const VoxelChunkLocation &location, MissingChunkPolicy policy) {
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_chunks.find(location);
	if (it == m_chunks.end()) {
		switch (policy) {
			case MissingChunkPolicy::NONE:
				return VoxelChunkRef();
			case MissingChunkPolicy::CREATE:
				lock.unlock();
				return createChunk<VoxelChunkRef>(location);
			case MissingChunkPolicy::LOAD:
				lock.unlock();
				createAndLoadChunk<VoxelChunkMutableRef>(location);
				return chunk(location, MissingChunkPolicy::NONE);
			case MissingChunkPolicy::LOAD_ASYNC:
				lock.unlock();
				m_chunkLoader->loadAsync(*this, location);
				return VoxelChunkRef();
		}
	}
	return VoxelChunkRef(*it->second);
}

VoxelChunkExtendedRef VoxelWorld::extendedChunk(const VoxelChunkLocation &location, MissingChunkPolicy policy) {
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_chunks.find(location);
	if (it == m_chunks.end()) {
		switch (policy) {
			case MissingChunkPolicy::NONE:
				return VoxelChunkExtendedRef();
			case MissingChunkPolicy::CREATE:
				lock.unlock();
				return createChunk<VoxelChunkExtendedRef>(location);
			case MissingChunkPolicy::LOAD:
				lock.unlock();
				createAndLoadChunk<VoxelChunkMutableRef>(location);
				return extendedChunk(location, MissingChunkPolicy::NONE);
			case MissingChunkPolicy::LOAD_ASYNC:
				lock.unlock();
				m_chunkLoader->loadAsync(*this, location);
				return VoxelChunkExtendedRef();
		}
	}
	return VoxelChunkExtendedRef(*it->second);
}

VoxelChunkMutableRef VoxelWorld::mutableChunk(const VoxelChunkLocation &location, MissingChunkPolicy policy) {
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_chunks.find(location);
	if (it == m_chunks.end()) {
		switch (policy) {
			case MissingChunkPolicy::NONE:
				return VoxelChunkMutableRef();
			case MissingChunkPolicy::CREATE:
				lock.unlock();
				return createChunk<VoxelChunkMutableRef>(location);
			case MissingChunkPolicy::LOAD:
				lock.unlock();
				return createAndLoadChunk<VoxelChunkMutableRef>(location);
			case MissingChunkPolicy::LOAD_ASYNC:
				lock.unlock();
				m_chunkLoader->loadAsync(*this, location);
				return VoxelChunkMutableRef();
		}
	}
	return VoxelChunkMutableRef(*it->second);
}

VoxelChunkExtendedMutableRef VoxelWorld::extendedMutableChunk(
		const VoxelChunkLocation &location,
		MissingChunkPolicy policy
) {
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_chunks.find(location);
	if (it == m_chunks.end()) {
		switch (policy) {
			case MissingChunkPolicy::NONE:
				return VoxelChunkExtendedMutableRef();
			case MissingChunkPolicy::CREATE:
				lock.unlock();
				return createChunk<VoxelChunkExtendedMutableRef>(location);
			case MissingChunkPolicy::LOAD:
				lock.unlock();
				return createAndLoadChunk<VoxelChunkExtendedMutableRef>(location);
			case MissingChunkPolicy::LOAD_ASYNC:
				lock.unlock();
				m_chunkLoader->loadAsync(*this, location);
				return VoxelChunkExtendedMutableRef();
		}
	}
	return VoxelChunkExtendedMutableRef(*it->second);
}

void VoxelWorld::unloadChunks(const std::vector<VoxelChunkLocation> &locations) {
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	for (auto &location : locations) {
		auto it = m_chunks.find(location);
		if (it != m_chunks.end()) {
			std::unique_lock<std::shared_mutex> chunkLock(it->second->mutex());
			it->second->unsetNeighbors();
			chunkLock.unlock();
			m_chunks.erase(it);
		}
	}
}
