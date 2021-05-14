#pragma once

#include <climits>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <functional>
#include <utility>
#include <typeinfo>
#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#ifndef HEADLESS
#include "../client/ShaderProgram.h"
#endif

class Asset;
struct InChunkVoxelLocation;
class VoxelChunkExtendedRef;
class VoxelChunkExtendedMutableRef;
class VoxelTypeRegistry;

struct VoxelVertexData {
	float x, y, z;
	float u, v;
};

static const int MAX_VOXEL_SHADER_PRIORITY = INT_MAX;

class VoxelShaderProvider {
public:
	virtual ~VoxelShaderProvider() = default;
	[[nodiscard]] virtual int priority() const {
		return MAX_VOXEL_SHADER_PRIORITY;
	}
#ifndef HEADLESS
	[[nodiscard]] virtual const CommonShaderProgram &get() const = 0;
	virtual void setup(const CommonShaderProgram &program) const = 0;
#endif

};

class VoxelTextureShaderProvider: public VoxelShaderProvider {
#ifndef HEADLESS
	std::variant<std::unique_ptr<GL::Texture>, std::reference_wrapper<const GL::Texture>> m_texture;
#endif

public:
	explicit VoxelTextureShaderProvider(Asset asset);
#ifndef HEADLESS
	explicit VoxelTextureShaderProvider(const GL::Texture &texture);
	[[nodiscard]] const CommonShaderProgram &get() const override;
	void setup(const CommonShaderProgram &program) const override;
#endif

};

struct Voxel;
class VoxelTypeRegistry;
class VoxelTypeSerializationContext;

typedef bitsery::Serializer<bitsery::OutputBufferAdapter<std::string>, const VoxelTypeSerializationContext> VoxelSerializer;
typedef bitsery::Deserializer<bitsery::InputBufferAdapter<std::string>, const VoxelTypeSerializationContext> VoxelDeserializer;

typedef int8_t VoxelLightLevel;
static const VoxelLightLevel MAX_VOXEL_LIGHT_LEVEL = 16;

class VoxelTypeInterface {
public:
	virtual ~VoxelTypeInterface() = default;
	virtual void registerChildren(const std::string &name, VoxelTypeRegistry &registry) {
	}
	virtual void link(VoxelTypeRegistry &registry) {
	}
	virtual Voxel &invokeInit(void *ptr) = 0;
	virtual Voxel &invokeInit(void *ptr, const Voxel &voxel) = 0;
	virtual Voxel &invokeInit(void *ptr, Voxel &&voxel) = 0;
	virtual void invokeDestroy(Voxel &voxel) = 0;
	virtual bool invokeCheckType(const std::type_info &typeInfo) {
		return false;
	}
	virtual void invokeSerialize(const Voxel &voxel, VoxelSerializer &serializer) = 0;
	virtual void invokeDeserialize(Voxel &voxel, VoxelDeserializer &deserializer) = 0;
	virtual std::string invokeToString(const Voxel &voxel) = 0;
	virtual const VoxelShaderProvider *invokeShaderProvider(const Voxel &voxel) = 0;
	virtual void invokeBuildVertexData(
			const VoxelChunkExtendedRef &chunk,
			const InChunkVoxelLocation &location,
			const Voxel &voxel,
			std::vector<VoxelVertexData> &data
	) = 0;
	virtual VoxelLightLevel invokeLightLevel(const Voxel &voxel) = 0;
	virtual void invokeSlowUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) = 0;
	virtual bool invokeUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			unsigned long deltaTime,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) = 0;
	virtual bool invokeHasDensity(const Voxel &voxel) = 0;
	
};

class VoxelTypeSerializationContext {
	VoxelTypeRegistry &m_registry;
	std::vector<std::pair<std::string, std::reference_wrapper<VoxelTypeInterface>>> m_types;
	std::unordered_map<const VoxelTypeInterface*, int> m_typeMap;
	
public:
	explicit VoxelTypeSerializationContext(VoxelTypeRegistry &registry);
	int typeId(const VoxelTypeInterface &type) const;
	VoxelTypeInterface &findTypeById(int id) const;
	std::vector<std::string> names() const;
	void setTypeId(int id, const std::string &name);
	[[nodiscard]] int size() const {
		return m_types.size();
	}
	void update();

	template<typename S> void serialize(S &s) const {
		s.ext(*this, SerializationHelper {});
	}
	template<typename S> void serialize(S &s) {
		s.ext(*this, SerializationHelper {});
	}
	
	class SerializationHelper {
	public:
		template<typename Ser, typename T, typename Fnc> void serialize(Ser& ser, const T& obj, Fnc&& fnc) const {
			ser.container(obj.names(), UINT16_MAX, [](auto &s, const std::string &name) {
				s.container1b(name, 127);
			});
		}
		
		template<typename Des, typename T, typename Fnc> void deserialize(Des& des, T& obj, Fnc&& fnc) const {
			std::vector<std::string> names;
			des.container(names, UINT16_MAX, [](auto &s, std::string &name) {
				s.container1b(name, 127);
			});
			obj.m_types.clear();
			obj.m_typeMap.clear();
			for (auto &name : names) {
				auto &type = obj.m_registry.get(name);
				obj.m_types.emplace_back(name, type);
				obj.m_typeMap.emplace(&type, (int) (obj.m_types.size() - 1));
			}
		}
	};
	
};

class VoxelTypeSerializationHelper {
public:
	template<typename Ser, typename T, typename Fnc> void serialize(Ser& ser, const T& obj, Fnc&& fnc) const {
		auto &ctx = ser.template context<const VoxelTypeSerializationContext>();
		uint16_t value = ctx.typeId(*obj);
		ser.value2b(value);
	}
	
	template<typename Des, typename T, typename Fnc> void deserialize(Des& des, T& obj, Fnc&& fnc) const {
		auto &ctx = des.template context<const VoxelTypeSerializationContext>();
		uint16_t value;
		des.value2b(value);
		obj = &ctx.findTypeById(value);
	}
	
};

namespace bitsery::traits {
	template<typename T> struct ExtensionTraits<VoxelTypeSerializationHelper, T> {
		using TValue = void;
		static constexpr bool SupportValueOverload = false;
		static constexpr bool SupportObjectOverload = true;
		static constexpr bool SupportLambdaOverload = false;
	};
	
	template<typename T> struct ExtensionTraits<VoxelTypeSerializationContext::SerializationHelper, T> {
		using TValue = void;
		static constexpr bool SupportValueOverload = false;
		static constexpr bool SupportObjectOverload = true;
		static constexpr bool SupportLambdaOverload = false;
	};
}

struct Voxel {
	VoxelTypeInterface *type;
	VoxelLightLevel lightLevel = MAX_VOXEL_LIGHT_LEVEL;
	
	template<typename S> void serialize(S& s) {
		s.ext(type, VoxelTypeSerializationHelper {});
		s.value1b(lightLevel);
	}
	
};

static const size_t MAX_VOXEL_DATA_SIZE = sizeof(Voxel) + 16;

template<typename T, typename Data=Voxel, typename Base=VoxelTypeInterface> class VoxelType: public Base {
public:
	template<typename ...Args> explicit VoxelType(Args&&... args): Base(std::forward<Args>(args)...) {
	}
	
	Voxel &invokeInit(void *ptr) override {
		static_assert(sizeof(Data) <= MAX_VOXEL_DATA_SIZE);
		return *(new (ptr) Data { this });
	}
	
	Voxel &invokeInit(void *ptr, const Voxel &voxel) override {
		static_assert(sizeof(Data) <= MAX_VOXEL_DATA_SIZE);
		return *(new (ptr) Data(static_cast<const Data&>(voxel)));
	}
	
	Voxel &invokeInit(void *ptr, Voxel &&voxel) override {
		static_assert(sizeof(Data) <= MAX_VOXEL_DATA_SIZE);
		return *(new (ptr) Data(std::move(static_cast<Data&>(voxel))));
	}
	
	void invokeDestroy(Voxel &voxel) override {
		(static_cast<Data&>(voxel)).~Data();
	}
	
	bool invokeCheckType(const std::type_info &typeInfo) override {
		return typeid(Data) == typeInfo || Base::invokeCheckType(typeInfo);
	}
	
	std::string invokeToString(const Voxel &voxel) override {
		return static_cast<T*>(this)->T::toString(static_cast<const Data&>(voxel));
	}

	const VoxelShaderProvider *invokeShaderProvider(const Voxel &voxel) override {
		return static_cast<T*>(this)->T::shaderProvider(static_cast<const Data&>(voxel));
	}
	
	void invokeBuildVertexData(
			const VoxelChunkExtendedRef &chunk,
			const InChunkVoxelLocation &location,
			const Voxel &voxel,
			std::vector<VoxelVertexData> &data
	) override {
		static_cast<T*>(this)->T::buildVertexData(chunk, location, static_cast<const Data&>(voxel), data);
	}

	VoxelLightLevel invokeLightLevel(const Voxel &voxel) override {
		return static_cast<T*>(this)->T::lightLevel(static_cast<const Data&>(voxel));
	}
	
	void invokeSerialize(const Voxel &voxel, VoxelSerializer &serializer) override {
		serializer.object(static_cast<const Data&>(voxel));
	}
	
	void invokeDeserialize(Voxel &voxel, VoxelDeserializer &deserializer) override {
		invokeInit(&voxel);
		deserializer.object(static_cast<Data&>(voxel));
	}
	
	void invokeSlowUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) override {
		static_cast<T*>(this)->T::slowUpdate(
				chunk,
				location,
				static_cast<Data&>(voxel),
				invalidatedLocations
		);
	}
	
	bool invokeUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			unsigned long deltaTime,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) override {
		return static_cast<T*>(this)->T::update(
				chunk,
				location,
				static_cast<Data&>(voxel),
				deltaTime,
				invalidatedLocations
		);
	}
	
	bool invokeHasDensity(const Voxel &voxel) override {
		return static_cast<T*>(this)->T::hasDensity(static_cast<const Data&>(voxel));
	}
	
};

class EmptyVoxelType: public VoxelType<EmptyVoxelType, Voxel> {
public:
	static EmptyVoxelType INSTANCE;
	
	std::string toString(const Voxel &voxel);
	const VoxelShaderProvider *shaderProvider(const Voxel &voxel);
	void buildVertexData(
			const VoxelChunkExtendedRef &chunk,
			const InChunkVoxelLocation &location,
			const Voxel &voxel,
			std::vector<VoxelVertexData> &data
	);
	VoxelLightLevel lightLevel(const Voxel &voxel);
	void slowUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	);
	bool update(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			unsigned long deltaTime,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	);
	bool hasDensity(const Voxel &voxel);
	
};

template<typename T> static inline bool checkVoxelType(const Voxel &voxel) {
	return voxel.type->invokeCheckType(typeid(T));
}

template<> bool checkVoxelType<Voxel>(const Voxel &voxel) {
	return true;
}

class VoxelHolder {
	char m_data[MAX_VOXEL_DATA_SIZE];

public:
	VoxelHolder(): VoxelHolder(EmptyVoxelType::INSTANCE) {
	}
	
	explicit VoxelHolder(VoxelTypeInterface &type) {
		type.invokeInit(m_data);
	}
	
	VoxelHolder(const VoxelHolder& holder) {
		holder.type().invokeInit(m_data, holder.get());
	}
	
	VoxelHolder(VoxelHolder &&holder) noexcept {
		holder.type().invokeInit(m_data, std::move(holder.get()));
		holder.setType(EmptyVoxelType::INSTANCE);
	}
	
	VoxelHolder &operator=(const VoxelHolder &holder) {
		if (this != &holder) {
			auto savedLightLevel = lightLevel();
			get().type->invokeDestroy(get());
			holder.type().invokeInit(m_data, holder.get());
			setLightLevel(savedLightLevel);
		}
		return *this;
	}
	
	VoxelHolder &operator=(VoxelHolder &&holder) noexcept {
		if (this != &holder) {
			auto savedLightLevel = lightLevel();
			get().type->invokeDestroy(get());
			holder.type().invokeInit(m_data, std::move(holder.get()));
			setLightLevel(savedLightLevel);
		}
		return *this;
	}
	
	~VoxelHolder() {
		get().type->invokeDestroy(get());
	}
	
	template<typename T=Voxel> [[nodiscard]] const T &get() const {
		assert(checkVoxelType<T>(*reinterpret_cast<const Voxel*>(m_data)));
		return *reinterpret_cast<const T*>(m_data);
	}
	
	template<typename T=Voxel> T &get() {
		assert(checkVoxelType<T>(*reinterpret_cast<const Voxel*>(m_data)));
		return *reinterpret_cast<T*>(m_data);
	}

	[[nodiscard]] VoxelTypeInterface &type() const {
		return *get().type;
	}

	void setType(VoxelTypeInterface &newType) {
		auto savedLightLevel = lightLevel();
		get().type->invokeDestroy(get());
		newType.invokeInit(m_data);
		setLightLevel(savedLightLevel);
	}

	[[nodiscard]] VoxelLightLevel lightLevel() const {
		return get().lightLevel;
	}

	void setLightLevel(VoxelLightLevel level) {
		get().lightLevel = level;
	}

	[[nodiscard]] VoxelLightLevel typeLightLevel() const {
		return get().type->invokeLightLevel(get());
	}
	
	[[nodiscard]] std::string toString() const {
		return get().type->invokeToString(get());
	}

	[[nodiscard]] const VoxelShaderProvider *shaderProvider() const {
		return get().type->invokeShaderProvider(get());
	}
	
	void buildVertexData(
			const VoxelChunkExtendedRef &chunk,
			const InChunkVoxelLocation &location,
			std::vector<VoxelVertexData> &data
	) const {
		get().type->invokeBuildVertexData(chunk, location, get(), data);
	}
	
	void serialize(VoxelSerializer &serializer) const {
		get().type->invokeSerialize(get(), serializer);
	}
	
	void serialize(VoxelDeserializer &deserializer);
	
	void slowUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) {
		get().type->invokeSlowUpdate(chunk, location, get(), invalidatedLocations);
	}
	
	bool update(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			unsigned long deltaTime,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	) {
		return get().type->invokeUpdate(chunk, location, get(), deltaTime, invalidatedLocations);
	}
	
	[[nodiscard]] bool hasDensity() const {
		return get().type->invokeHasDensity(get());
	}
	
};

class SimpleVoxelType: public VoxelType<SimpleVoxelType>, public VoxelTextureShaderProvider {
	std::string m_name;
	bool m_unwrap;
	VoxelLightLevel m_lightLevel;
	bool m_transparent;
	bool m_hasDensity;

public:
	SimpleVoxelType(
			std::string name,
			Asset asset,
			bool unwrap = false,
			VoxelLightLevel lightLevel = 0,
			bool transparent = false,
			bool hasDensity = true
	);

#ifndef HEADLESS
	SimpleVoxelType(
			std::string name,
			const GL::Texture &texture,
			bool unwrap = false,
			VoxelLightLevel lightLevel = 0,
			bool transparent = false,
			bool hasDensity = true
	);
#endif
	std::string toString(const Voxel &voxel);
	const VoxelShaderProvider *shaderProvider(const Voxel &voxel);
	void buildVertexData(
			const VoxelChunkExtendedRef &chunk,
			const InChunkVoxelLocation &location,
			const Voxel &voxel,
			std::vector<VoxelVertexData> &data
	);
	VoxelLightLevel lightLevel(const Voxel &voxel);
	[[nodiscard]] int priority() const override;
	void slowUpdate(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	);
	bool update(
			const VoxelChunkExtendedMutableRef &chunk,
			const InChunkVoxelLocation &location,
			Voxel &voxel,
			unsigned long deltaTime,
			std::unordered_set<InChunkVoxelLocation> &invalidatedLocations
	);
	bool hasDensity(const Voxel &voxel);
	
};
