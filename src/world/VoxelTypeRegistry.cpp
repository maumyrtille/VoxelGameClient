#include "VoxelTypeRegistry.h"

class UnknownVoxelType: public VoxelTypeHelper<UnknownVoxelType, Voxel, SimpleVoxelType> {
public:
	UnknownVoxelType(VoxelTypeRegistry &registry, std::string name): VoxelTypeHelper(
			std::move(name),
#ifdef HEADLESS
			"assets/textures/unknown_block.png"
#else
			registry.m_unknownBlockTexture
#endif
	) {
	}
	
};

VoxelTypeRegistry::VoxelTypeRegistry() {
#ifndef HEADLESS
	m_unknownBlockTexture = GLTexture("assets/textures/unknown_block.png");
#endif
}

void VoxelTypeRegistry::add(std::string name, std::unique_ptr<VoxelType> type) {
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	printf("Registered \"%s\" voxel type\n", name.c_str());
	m_types.emplace(std::move(name), std::move(type));
}

VoxelType &VoxelTypeRegistry::get(const std::string &name) {
	if (name == "empty") {
		return EmptyVoxelType::INSTANCE;
	}
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	auto it = m_types.find(name);
	if (it != m_types.end()) {
		return *it->second;
	}
	lock.unlock();
	add(name, std::make_unique<UnknownVoxelType>(*this, name));
	return get(name);
}
