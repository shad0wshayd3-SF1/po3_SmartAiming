class Config
{
public:
	class SectionSettings
	{
	public:
		bool bSwitchOnDraw{ false };
		bool bSwitchOnAim{ true };
		bool bRememberLastCameraState{ true };
	};

	// members
	SectionSettings Settings;

public:
	static Config* GetSingleton()
	{
		static Config singleton;
		return std::addressof(singleton);
	}

	static void Load()
	{
		const auto plugin = SFSE::PluginVersionData::GetSingleton();
		auto config = std::filesystem::current_path() /=
			std::format("Data/SFSE/plugins/{}.ini"sv, plugin->GetPluginName());
		try
		{
			auto reader = figcone::ConfigReader{};
			*GetSingleton() = reader.readIniFile<Config>(config.make_preferred());
		}
		catch (const std::exception& e)
		{
			SFSE::log::error("{}"sv, e.what());
		}
	}
};

namespace Handler
{
	RE::CameraState lastDrawCameraState;
	RE::CameraState lastAimCameraState;

	namespace detail
	{
		static RE::CameraState QCameraState()
		{
			return *RE::stl::adjust_pointer<RE::CameraState>(RE::PlayerCamera::GetSingleton()->currentState, 0x50);
		}

		static void TrySetFirstPerson(RE::CameraState& a_lastCameraState)
		{
			a_lastCameraState = QCameraState();
			if (RE::PlayerCamera::GetSingleton()->IsInThirdPerson())
			{
				RE::PlayerCamera::GetSingleton()->ForceFirstPerson();
			}
		}

		static void TrySetThirdPerson(const RE::CameraState a_lastCameraState)
		{
			if (RE::PlayerCamera::GetSingleton()->IsInFirstPerson() &&
			    (!Config::GetSingleton()->Settings.bRememberLastCameraState || a_lastCameraState == RE::CameraState::kThirdPerson))
			{
				RE::PlayerCamera::GetSingleton()->ForceThirdPerson();
			}
		}
	}

	struct WeaponDraw
	{
	public:
		static bool thunk(std::uintptr_t a_handler, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = func(a_handler, a_actor, a_tag);
			if (a_actor->IsPlayerRef())
			{
				detail::TrySetFirstPerson(lastDrawCameraState);
			}
			return result;
		}

		inline static REL::Relocation<decltype(thunk)> func;
		inline static std::uintptr_t address{ REL::Relocation<std::uintptr_t>(REL::ID(155299), 0x2C).address() };
	};

	struct WeaponSheathe
	{
	public:
		static bool thunk(std::uintptr_t a_handler, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = func(a_handler, a_actor, a_tag);
			if (a_actor->IsPlayerRef())
			{
				detail::TrySetThirdPerson(lastDrawCameraState);
			}
			return result;
		}

		inline static REL::Relocation<decltype(thunk)> func;
		inline static std::size_t idx{ 0x1 };
	};

	struct EnterIronSights
	{
	public:
		static bool thunk(RE::PlayerCamera* a_camera, std::uint32_t a_state)
		{
			const auto result = func(a_camera, a_state);
			if (!result)
			{
				detail::TrySetFirstPerson(lastAimCameraState);
			}
			return result;
		}

		inline static REL::Relocation<decltype(thunk)> func;
		inline static std::uintptr_t address{ REL::Relocation<std::uintptr_t>(REL::ID(153910), 0x128).address() };
	};

	struct ExitIronSights
	{
	public:
		static bool thunk(RE::PlayerCamera* a_camera, std::uint32_t a_state)
		{
			const auto result = func(a_camera, a_state);
			if (!result)
			{
				detail::TrySetThirdPerson(lastAimCameraState);
			}
			return result;
		}

		inline static REL::Relocation<decltype(thunk)> func;
		inline static std::uintptr_t address{ REL::Relocation<std::uintptr_t>(REL::ID(153910), 0x231).address() };
	};

	static void Install()
	{
		Config::Load();

		if (Config::GetSingleton()->Settings.bSwitchOnDraw)
		{
			// avoid weaponDraw firing twice
			RE::stl::write_thunk_call<WeaponDraw>();
			RE::stl::write_vfunc<WeaponSheathe>(RE::VTABLE::WeaponSheatheHandler[0]);

			SFSE::log::info("Hooked WeaponDraw/Sheathe"sv);
		}

		if (Config::GetSingleton()->Settings.bSwitchOnAim)
		{
			// if (!playerCamera->QCameraState(0x10)
			//	Push/PopIronSightMode

			RE::stl::write_thunk_call<EnterIronSights>();
			RE::stl::write_thunk_call<ExitIronSights>();

			SFSE::log::info("Hooked Enter/ExitIronSights"sv);
		}
	}
}

namespace
{
	void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
	{
		switch (a_msg->type)
		{
		case SFSE::MessagingInterface::kPostLoad:
		{
			Handler::Install();
			break;
		}
		default:
			break;
		}
	}
}

SFSEPluginLoad(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}
