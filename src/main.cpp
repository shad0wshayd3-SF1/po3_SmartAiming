namespace Config
{
	namespace Settings
	{
		static REX::INI::Bool bSwitchOnDraw{ "Settings", "bSwitchOnDraw", false };
		static REX::INI::Bool bSwitchOnAim{ "Settings", "bSwitchOnAim", true };
		static REX::INI::Bool bRememberLastCameraState{ "Settings", "bRememberLastCameraState", true };
	}

	static void Load()
	{
		const auto ini = REX::INI::SettingStore::GetSingleton();
		ini->Init(
			"Data/SFSE/plugins/po3_SmartAiming.ini",
			"Data/SFSE/plugins/po3_SmartAimingCustom.ini");
		ini->Load();
	}
}

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
			    (!Config::Settings::bRememberLastCameraState.GetValue() || a_lastCameraState == RE::CameraState::kThirdPerson))
			{
				RE::PlayerCamera::GetSingleton()->ForceThirdPerson();
			}
		}
	}

	struct WeaponDraw
	{
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
		inline static REL::Relocation address{ REL::ID(155299), 0x2C };
	};

	struct WeaponSheathe
	{
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
		inline static std::size_t idx{ 0x01 };
	};

	struct EnterIronSights
	{
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
		inline static REL::Relocation address{ REL::ID(153910), 0x128 };
	};

	struct ExitIronSights
	{
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
		inline static REL::Relocation address{ REL::ID(153910), 0x231 };
	};

	static void Install()
	{
		if (Config::Settings::bSwitchOnDraw.GetValue())
		{
			// avoid weaponDraw firing twice
			RE::stl::write_thunk_call<WeaponDraw>();
			RE::stl::write_vfunc<WeaponSheathe>(RE::VTABLE::WeaponSheatheHandler[0]);

			SFSE::log::info("Hooked WeaponDraw/Sheathe"sv);
		}

		if (Config::Settings::bSwitchOnAim.GetValue())
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

	Config::Load();

	SFSE::AllocTrampoline(64);
	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}
