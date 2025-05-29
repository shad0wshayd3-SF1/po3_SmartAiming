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

namespace Hooks
{
	static RE::CameraState lastDrawCameraState;
	static RE::CameraState lastIronCameraState;

	namespace detail
	{
		static RE::CameraState QCameraState()
		{
			auto PlayerCamera = RE::PlayerCamera::GetSingleton();
			return *REX::ADJUST_POINTER<RE::CameraState>(PlayerCamera->currentState, 0x50);
		}

		static void TrySetFirstPerson(RE::CameraState& a_lastCameraState)
		{
			a_lastCameraState = QCameraState();
			if (auto camera = RE::PlayerCamera::GetSingleton();
				camera && camera->IsInThirdPerson())
			{
				camera->ForceFirstPerson();
			}
		}

		static void TrySetThirdPerson(const RE::CameraState a_lastCameraState)
		{
			if (auto camera = RE::PlayerCamera::GetSingleton();
				camera && camera->IsInFirstPerson())
			{
				if (!Config::Settings::bRememberLastCameraState || a_lastCameraState == RE::CameraState::kThirdPerson)
				{
					camera->ForceThirdPerson();
				}
			}
		}
	}

	struct hkWeaponDraw
	{
		static bool WeaponDraw(void* a_this, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = _Hook0(a_this, a_actor, a_tag);
			if (Config::Settings::bSwitchOnDraw && a_actor->IsPlayerRef())
			{
				detail::TrySetFirstPerson(lastDrawCameraState);
			}

			return result;
		}

		inline static REL::Hook _Hook0{ REL::ID(103947), 0x2D, WeaponDraw };
	};

	struct hkWeaponSheathe
	{
		static bool WeaponSheathe(void* a_this, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = _Hook0(a_this, a_actor, a_tag);
			if (Config::Settings::bSwitchOnDraw && a_actor->IsPlayerRef())
			{
				detail::TrySetThirdPerson(lastDrawCameraState);
			}

			return result;
		}

		inline static REL::HookVFT _Hook0{ RE::VTABLE::WeaponSheatheHandler[0], 0x01, WeaponSheathe };
	};

	struct hkIronSights
	{
	private:
		static void* GetIronSights()
		{
			using func_t = decltype(&hkIronSights::GetIronSights);
			static REL::Relocation<func_t> func{ REL::ID(102581) };
			return func();
		}

		static void* _EnterIronSights(void* a_arg)
		{
			using func_t = decltype(&hkIronSights::_EnterIronSights);
			static REL::Relocation<func_t> func{ REL::ID(91180) };
			return func(a_arg);
		}

		static void* _ExitIronSights(void* a_arg)
		{
			using func_t = decltype(&hkIronSights::_ExitIronSights);
			static REL::Relocation<func_t> func{ REL::ID(91181) };
			return func(a_arg);
		}

		static void EnterIronSights()
		{
			if (Config::Settings::bSwitchOnAim)
			{
				if (auto camera = RE::PlayerCamera::GetSingleton();
					camera && !camera->QCameraEquals(RE::CameraState::kShipTargeting))
				{
					detail::TrySetFirstPerson(lastIronCameraState);
				}
			}

			_EnterIronSights(GetIronSights());
		}

		static void ExitIronSights()
		{
			if (Config::Settings::bSwitchOnAim)
			{
				if (auto camera = RE::PlayerCamera::GetSingleton();
					camera && !camera->QCameraEquals(RE::CameraState::kShipTargeting))
				{
					detail::TrySetThirdPerson(lastIronCameraState);
				}
			}

			_ExitIronSights(GetIronSights());
		}

	public:
		static void Install()
		{
			static REL::Relocation target{ REL::ID(103157) };
			{
				static constexpr auto TARGET_ADDR{ 0x15A };
				static constexpr auto TARGET_RETN{ 0x17B };
				static constexpr auto TARGET_FILL{ TARGET_RETN - TARGET_ADDR };
				target.write_fill<TARGET_ADDR>(REL::NOP, TARGET_FILL);
				target.write_call<5, TARGET_RETN>(EnterIronSights);
			}
			{
				static constexpr auto TARGET_ADDR{ 0x4D7 };
				static constexpr auto TARGET_RETN{ 0x4F8 };
				static constexpr auto TARGET_FILL{ TARGET_RETN - TARGET_ADDR };
				target.write_fill<TARGET_ADDR>(REL::NOP, TARGET_FILL);
				target.write_call<5, TARGET_RETN>(ExitIronSights);
			}
		}
	};

	static void Install()
	{
		hkIronSights::Install();
	}
}

namespace
{
	void MessageCallback(SFSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SFSE::MessagingInterface::kPostLoad:
			Config::Load();
			Hooks::Install();
			break;
		default:
			break;
		}
	}
}

SFSEPluginLoad(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize  = 64 });
	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);
	return true;
}
