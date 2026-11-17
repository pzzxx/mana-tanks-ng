#pragma once

// Minimal consumer-side declaration of the skee64 (RaceMenu / NiOverride)
// plugin interface. Vtable order must match expired6978's skee64/
// IPluginInterface.h exactly — only the virtual function layout matters since
// we call through a pointer returned by the interface-exchange message.
// Signatures use plain const char* (skee wraps them into SKEEFixedString).

namespace SKEE
{
	class IPluginInterface
	{
	public:
		virtual ~IPluginInterface() = default;

		virtual std::uint32_t GetVersion();
		virtual void          Revert();
	};

	class IInterfaceMap
	{
	public:
		virtual IPluginInterface* QueryInterface(const char* name);
		virtual bool              AddInterface(const char* name, IPluginInterface* pluginInterface);
		virtual IPluginInterface* RemoveInterface(const char* name);
	};

	struct InterfaceExchangeMessage
	{
		enum : std::uint32_t
		{
			kMessage_ExchangeInterface = 0x9E3779B9
		};

		IInterfaceMap* interfaceMap = nullptr;
	};

	class IBodyMorphInterface : public IPluginInterface
	{
	public:
		// Visitor placeholder types — never instantiated by us, they only
		// reserve the correct vtable slots.
		class MorphKeyVisitor
		{
		public:
			virtual void Visit(const char*, float) = 0;
		};
		class StringVisitor
		{
		public:
			virtual void Visit(const char*) = 0;
		};
		class ActorVisitor
		{
		public:
			virtual void Visit(RE::TESObjectREFR*) = 0;
		};
		class MorphValueVisitor
		{
		public:
			virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float) = 0;
		};
		class MorphVisitor
		{
		public:
			virtual void Visit(RE::TESObjectREFR*, const char*) = 0;
		};
		using MorphShapeCallback = void (*)(RE::TESObjectREFR*, RE::NiAVObject*, void*, void*, void*);

		virtual void  SetMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey, float relative);
		virtual float GetMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey);
		virtual void  ClearMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey);

		virtual float GetBodyMorphs(RE::TESObjectREFR* actor, const char* morphName);
		virtual void  ClearBodyMorphNames(RE::TESObjectREFR* actor, const char* morphName);

		virtual void VisitMorphs(RE::TESObjectREFR* actor, MorphVisitor& visitor);
		virtual void VisitKeys(RE::TESObjectREFR* actor, const char* name, MorphKeyVisitor& visitor);
		virtual void VisitMorphValues(RE::TESObjectREFR* actor, MorphValueVisitor& visitor);

		virtual void ClearMorphs(RE::TESObjectREFR* actor);

		virtual void ApplyVertexDiff(RE::TESObjectREFR* refr, RE::NiAVObject* rootNode, bool erase);
		virtual void ApplyBodyMorphs(RE::TESObjectREFR* refr, bool deferUpdate);
		virtual void UpdateModelWeight(RE::TESObjectREFR* refr, bool immediate);

		virtual void          SetCacheLimit(std::uint64_t limit);
		virtual bool          HasMorphs(RE::TESObjectREFR* actor);
		virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR* actor);

		virtual bool HasBodyMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey);
		virtual bool HasBodyMorphName(RE::TESObjectREFR* actor, const char* morphName);
		virtual bool HasBodyMorphKey(RE::TESObjectREFR* actor, const char* morphKey);
		virtual void ClearBodyMorphKeys(RE::TESObjectREFR* actor, const char* morphKey);
		virtual void VisitStrings(StringVisitor& visitor);
		virtual void VisitActors(ActorVisitor& visitor);
		virtual std::uint64_t ClearMorphCache();
		virtual void          AddMorphShapeCallback(MorphShapeCallback cb, std::uint64_t order);
	};

	[[nodiscard]] inline IBodyMorphInterface* GetBodyMorphInterface()
	{
		auto* messaging = SKSE::GetMessagingInterface();
		if (!messaging) {
			return nullptr;
		}

		InterfaceExchangeMessage msg;
		for (const char* receiver : { "SKEE", "skee" }) {
			msg.interfaceMap = nullptr;
			if (messaging->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &msg, sizeof(msg), receiver) &&
			    msg.interfaceMap) {
				break;
			}
		}
		if (!msg.interfaceMap) {
			return nullptr;
		}

		return static_cast<IBodyMorphInterface*>(msg.interfaceMap->QueryInterface("BodyMorph"));
	}
}
