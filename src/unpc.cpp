#include "unpc.hpp"
#include <version.hpp>

#include <fstream>
#include <iterator>

#include "hook.hpp"
#include "hotkey.hpp"
#include "integration/nexus.hpp"
#include "ui.hpp"

using namespace std::chrono_literals;
using namespace memory;

using namespace unpc;

std::atomic_bool               unpc::nexusPresent {};
std::atomic_bool               unpc::arcDpsPresent {};
std::atomic_bool               unpc::injected {};
std::atomic_bool               unpc::exit {};
std::atomic_bool               unpc::stopping {};
std::optional<logging::Logger> unpc::logger;
std::optional<Settings>        unpc::settings;
std::optional<Detour>          unpc::hook {};
std::optional<Detour>          unpc::hook2 {};

HotkeyManager unpc::hotkeyManager("ArenaNet_Gr_Window_Class",
                                  std::filesystem::current_path() / "addons" / "ReUnhideNPCs" / "hotkeys.json");
HANDLE        unpc::hMutex {};
HMODULE       unpc::hModule {};

HANDLE unpc::hThread {};

std::string unpc::proxyModuleName {};
HMODULE     unpc::hProxyModule {};
MumbleLink* unpc::mumbleLink {};
int32_t*    unpc::loadingScreenActive {};

uint32_t unpc::numPlayersVisible {};
uint32_t unpc::numPlayersCulled {};
uint32_t unpc::numMountsVisible {};
uint32_t unpc::numMountsCulled {};
uint32_t unpc::numPlayerOwnedVisible {};
uint32_t unpc::numPlayerOwnedCulled {};
uint32_t unpc::numOffScreenCulled {};
uint32_t unpc::numOffScreenConfirmed {};
uint32_t unpc::numNPCVisible {};
uint32_t unpc::numNPCCulled {};

unpc::EHideReason                        unpc::hideReason { unpc::EHideReason::None };
std::array<uint32_t, unpc::hideReasonCount> unpc::hiddenByReason {};
uint32_t                                 unpc::passMicrosX10 {};
uint32_t                                 unpc::passCharacters {};
bool                                     unpc::clientLayoutOk { true };

const char* unpc::hideReasonName(const EHideReason reason)
{
    switch (reason)
    {
    case EHideReason::None:
        return "none";
    case EHideReason::NotUnhidden:
        return "not unhidden";
    case EHideReason::OffScreen:
        return "off-screen";
    case EHideReason::MaximumDistance:
        return "distance";
    case EHideReason::MaxPlayers:
        return "max players";
    case EHideReason::MaxPlayerOwned:
        return "max owned";
    case EHideReason::MaxNpcs:
        return "max NPCs";
    case EHideReason::HidePlayers:
        return "hide players";
    case EHideReason::HidePlayerOwned:
        return "hide owned";
    case EHideReason::NonFriends:
        return "non-friends";
    case EHideReason::NonFriendsOwned:
        return "non-friends owned";
    case EHideReason::Blocked:
        return "blocked";
    case EHideReason::BlockedOwned:
        return "blocked owned";
    case EHideReason::NonGuild:
        return "non-guild";
    case EHideReason::NonGuildOwned:
        return "non-guild owned";
    case EHideReason::NonGroup:
        return "non-group";
    case EHideReason::NonGroupOwned:
        return "non-group owned";
    case EHideReason::Strangers:
        return "strangers";
    case EHideReason::StrangersOwned:
        return "strangers owned";
    case EHideReason::InCombat:
        return "in combat";
    case EHideReason::InCombatOwned:
        return "in combat owned";
    case EHideReason::OwnedInstances:
        return "instances";
    case EHideReason::OwnedSelf:
        return "my own";
    case EHideReason::Rank:
        return "rank";
    case EHideReason::AttackableFilter:
        return "attackable filter";
    case EHideReason::RiderHidden:
        return "hidden rider";
    case EHideReason::Count:
        break;
    }

    return "?";
}

std::shared_mutex             unpc::debugRenderMutex {};
std::vector<DebugRenderEntry> unpc::debugRenderEntries {};

#include "re.hpp"

static void hotkeyCallback(const std::string& id)
{
    if (!settings)
    {
        return;
    }

    auto lambda = [id](fw::Settings& settings)
    {
        for (auto& _setting : settings.settings())
        {
            auto matched = false;

            std::visit(
                [&](auto&& setting)
                {
                    using T = std::decay_t<decltype(*setting)>;
                    if constexpr (!std::is_same_v<T, fw::Setting<bool>>)
                    {
                        return;
                    }
                    if (id == setting->name())
                    {
                        setting->set(!setting->get());
                        matched = true;
                    }
                },
                _setting);

            if (matched)
            {
                LOG_INFO("Hotkey triggered: {}", id);
                return true;
            }
        }
        return false;
    };

    if (lambda(*settings) || lambda(settings->profile()))
    {
        return;
    }

    if (id == "ForceVisibility")
    {
        ++re::forceVisibility;
    }
    else if (id == "ToggleOverlay")
    {
        if (settings->OverlayOpen.get() && hotkeyManager.isCapturing())
        {
            hotkeyManager.stopCapturing();
        }

        settings->OverlayOpen.set(!settings->OverlayOpen.get());
    }
    else if (id == "SwitchProfile")
    {
        auto idx = settings->ActiveProfile.get();
        ++idx;
        if (idx >= static_cast<int>(settings->children().size()))
        {
            idx = 0;
        }
        if (idx != settings->ActiveProfile.get())
        {
            settings->ActiveProfile.set(idx);
            settings->needSave();
            ++re::forceVisibility;
        }
    }
}

static void initializeHotkeys()
{
    auto lambda = [](fw::Settings& settings)
    {
        for (auto& _setting : settings.settings())
        {
            std::visit(
                [&](auto&& setting)
                {
                    using T = std::decay_t<decltype(*setting)>;
                    if constexpr (!std::is_same_v<T, fw::Setting<bool>>)
                    {
                        return;
                    }

                    hotkeyManager.registerHotkey(setting->name(), setting->description());
                },
                _setting);
        }
    };

    hotkeyManager.registerHotkey("ForceVisibility", "Force Visibility");
    hotkeyManager.registerHotkey("ToggleOverlay", "Alt. Overlay Toggle");
    hotkeyManager.registerHotkey("SwitchProfile", "Switches to the next profile");

    lambda(*settings);
    lambda(settings->profile());

    hotkeyManager.registerCallback(hotkeyCallback);
    hotkeyManager.load();
}

static bool initialize()
{
    if (!mumbleLink)
    {
        mumbleLink = mode == EMode::Nexus ? nexus::getMumbleLink() : getMumbleLink();
        if (!mumbleLink)
        {
            LOG_ERR("MumbleLink Failed");
            return false;
        }
    }

    const auto settingsPath =
        std::filesystem::current_path() / "addons" / "ReUnhideNPCs" / "settings.json";

    // The previous community attempt used a different schema. Keep its file
    // for reference, but start from known-good defaults instead of inheriting
    // stale values (for example MaxPlayersVisible=20, which makes players
    // flicker in busy maps).
    if (std::filesystem::exists(settingsPath))
    {
        std::ifstream in(settingsPath);
        const std::string content(std::istreambuf_iterator<char>(in), {});

        if (content.find("\"RenderDebugCircles\"") != std::string::npos
            || content.find("\"ReUnhideNPCs\":") != std::string::npos)
        {
            std::error_code ec;
            const auto archived = settingsPath.string() + ".old-attempt";
            std::filesystem::remove(archived, ec);
            std::filesystem::rename(settingsPath, archived, ec);
            LOG_INFO("Archived settings from the previous implementation");
        }
    }

    settings.emplace(settingsPath);
    if (!settings->loaded())
    {
        LOG_ERR("Settings Failed");
        return false;
    }

    {
        const auto& debug = settings->profile();
        LOG_INFO("Settings: HidePlayerOwned={} HidePlayerOwnedSelf={} HideNonFriendsOwned={} MaxPlayersVisible={}",
                 debug.HidePlayerOwned.get(),
                 debug.HidePlayerOwnedSelf.get(),
                 debug.HideNonFriendsOwned.get(),
                 debug.MaxPlayersVisible.get());
    }

    if (auto& profile = settings->profile(); profile.HideOffScreen.get())
    {
        if (!profile.UnhidePlayers.get())
        {
            profile.UnhidePlayers.set(true);
        }
        if (!profile.UnhideNPCs.get())
        {
            profile.UnhideNPCs.set(true);
        }
        if (!profile.PlayerOwned.get())
        {
            profile.PlayerOwned.set(true);
        }
    }

    if (settings->ForceConsole.get())
    {
        logger->setConsole(true);
    }

    // Resolving the client patterns is reverse-engineering detail, not something
    // a user needs in their log. Everything below this point runs quietly
    // unless verbose logging is enabled; the level is restored before returning
    // so normal messages keep their level.
    const auto normalLevel = settings->DebugLog.get() ? logging::LogLevel::Debug : logging::LogLevel::Info;
    logger->setLevel(settings->DebugLog.get() ? logging::LogLevel::Debug : logging::LogLevel::Warning);

    Module game {};
    if (!Module::tryGetByName("Gw2-64.exe", game))
    {
        game = Module::getMain();
        LOG_ERR("Unable to find \"Gw2-64.exe\", using fallback module: \"{}\"", game.name());
    }

    Handle  ptr {};
    RefData ref {};

    auto pattern1 = Pattern("pattern1", re::pattern1)
                        .callback(
                            [&](const Handle& ptr)
                            {
                                re::vtableIndex = ptr.add(2).deref<uint32_t>() / 8;
                                LOG_DBG("VTable Index: {}", re::vtableIndex);
                                return true;
                            });

    if (!pattern1.resolve(game, ptr))
    {
        return false;
    }

    auto pattern2 = Pattern("pattern2", re::pattern2).add(10).relCall();
    if (!pattern2.resolve(game, ptr))
    {
        return false;
    }
    hook.emplace("Hook 1", ptr.to_ptr<void*>(), reinterpret_cast<void*>(re::hook));

    auto pattern3 = Pattern("pattern3", re::pattern3)
                        .callback(
                            [&](Handle& ptr)
                            {
                                ptr                 = ptr.add(5);
                                loadingScreenActive = ptr.add(6).add(ptr.add(2).deref<int32_t>()).to_ptr<int32_t*>();
                                ptr                 = Handle(loadingScreenActive);
                                LOG_DBG("Loading screen active: {}", ptr.formatted());
                                return true;
                            });
    if (!pattern3.resolve(game, ptr))
    {
        return false;
    }

    std::vector<Handle> asserts {};
    if (!findAsserts(
            game, std::get<0>(re::pattern7_1), std::get<1>(re::pattern7_1), std::get<2>(re::pattern7_1), asserts)
        && !findAsserts(
            game, std::get<0>(re::pattern7_2), std::get<1>(re::pattern7_2), std::get<2>(re::pattern7_2), asserts))
    {
        LOG_ERR("Unable to find pattern 7");
        return false;
    }
    re::gw2::getContextCollection =
        reinterpret_cast<re::gw2::GetContextCollectionFn>(asserts[0].sub(13).resolve_relative_call().raw());

    if (!game.findStringReference(re::pattern5, ref))
    {
        LOG_ERR("Unable to find pattern 5");
        return false;
    }
    re::gw2::getAvContext =
        reinterpret_cast<re::gw2::GetAvContextFn>(ref.instruction().add(12).resolve_relative_call().raw());

    if (!findAsserts(
            game, std::get<0>(re::pattern8_1), std::get<1>(re::pattern8_1), std::get<2>(re::pattern8_1), asserts)
        && !findAsserts(
            game, std::get<0>(re::pattern8_2), std::get<1>(re::pattern8_2), std::get<2>(re::pattern8_2), asserts))
    {
        LOG_ERR("Unable to find pattern 8");
        return false;
    }
    if (!game.findFunctionStart(asserts[0], ptr))
    {
        LOG_ERR("Unable to find pattern 8 [2]");
        return false;
    }
    hook2.emplace("Hook 2", ptr.to_ptr<void*>(), reinterpret_cast<void*>(re::hook2));

    if (!hook->enable())
    {
        LOG_ERR("Failed to enable hook");
        return false;
    }

    if (!hook2->enable())
    {
        LOG_ERR("Failed to enable hook");
        return false;
    }

    initializeHotkeys();
    game.clear();

    logger->setLevel(normalLevel);

    return true;
}

void unpc::onHookTick()
{
    if (exit)
    {
        return;
    }

    if (!settings || !settings->loaded())
    {
        return;
    }

    hotkeyManager.update();
    settings->save();

    // Apply the verbose setting without waiting for a restart. The RE layer
    // logs its own diagnostics at debug level, so this is what makes them
    // visible when someone is chasing a problem.
    static bool verboseLog = false;
    if (logger && settings->DebugLog.get() != verboseLog)
    {
        verboseLog = settings->DebugLog.get();
        logger->setLevel(verboseLog ? logging::LogLevel::Debug : logging::LogLevel::Info);
        LOG_INFO("Log level: {}", verboseLog ? "debug" : "info");
    }

    if (mode != EMode::Injected && mode != EMode::Proxy)
    {
        return;
    }

    const bool loadingScreen = *loadingScreenActive;
    const bool uiVersion     = mumbleLink && mumbleLink->uiVersion == 2;

    if (loadingScreen || uiVersion)
    {
        // If loading wait 3 seconds, otherwise initialize overlay straight away
        static uint64_t initialTick = GetTickCount64() - (uiVersion ? 3000 : 0);
        if (GetTickCount64() - initialTick < 3000)
        {
            return;
        }
        initialTick = GetTickCount64();

        if (!ui::d3dHook && (mode == EMode::Proxy || mode == EMode::Injected))
        {
            ui::d3dHook = hooks::D3D11::create("ArenaNet_Gr_Window_Class",
                                               "",
                                               ui::onD3DPresent,
                                               ui::onD3DResizeBuffers,
                                               ui::onD3DStarted,
                                               ui::onD3DShutdown);

            if (!ui::d3dHook || !ui::d3dHook.value()->enable())
            {
                LOG_INFO("Overlay Not OK");
            }
            else
            {
                LOG_INFO("Overlay OK");
            }
        }
    }
}

bool unpc::shouldHide(const re::CharacterEntry& c, const bool instanced)
{
    if (!settings)
    {
        return false;
    }

    hideReason = EHideReason::None;

    const auto& profile = settings->profile();

    if (c.isTarget && profile.AlwaysShowTarget.get())
    {
        return false;
    }

    if (c.isPlayerOwned && c.isOwnerLocalPlayer && profile.AlwaysShowOwnedSelf.get())
    {
        return false;
    }

    if (profile.AlwaysShowGroup.get() && (c.isPartyMember || c.isSquadMember))
    {
        return false;
    }

    if (profile.AlwaysShowFriends.get() && c.isFriend)
    {
        return false;
    }

    if (profile.AlwaysShowAttackable.get() && c.isAttackable)
    {
        return false;
    }

    if (!c.isOnScreen && profile.HideOffScreen.get())
    {
        ++numOffScreenCulled;
        hideReason = EHideReason::OffScreen;
        return true;
    }

    if (profile.MaximumDistance.get() > 0 && c.distance >= profile.MaximumDistance.get())
    {
        hideReason = EHideReason::MaximumDistance;
        return true;
    }

    if (c.isPlayer)
    {
        const auto maxVisible = profile.MaxPlayersVisible.get();
        if (maxVisible > 0 && numPlayersVisible >= static_cast<uint32_t>(maxVisible))
        {
            hideReason = EHideReason::MaxPlayers;
            return true;
        }

        if (profile.HidePlayers.get())
        {
            hideReason = EHideReason::HidePlayers;
            return true;
        }

        if (profile.HideNonFriends.get() && !c.isFriend)
        {
            hideReason = EHideReason::NonFriends;
            return true;
        }

        if (profile.HideBlockedPlayers.get() && c.isBlocked)
        {
            hideReason = EHideReason::Blocked;
            return true;
        }

        if (profile.HideNonGuildMembers.get() && !(c.isActiveGuildMember || c.isGuildMember))
        {
            hideReason = EHideReason::NonGuild;
            return true;
        }

        const bool groupMember = c.isPartyMember || c.isSquadMember;
        if (profile.HideNonGroupMembers.get() && !groupMember)
        {
            hideReason = EHideReason::NonGroup;
            return true;
        }

        if (profile.HideStrangers.get() && !c.isFriend && !c.isPartyMember && !c.isSquadMember
            && !c.isActiveGuildMember)
        {
            hideReason = EHideReason::Strangers;
            return true;
        }

        if (profile.HidePlayersInCombat.get() && mumbleLink && mumbleLink->getContext().isInCombat())
        {
            hideReason = EHideReason::InCombat;
            return true;
        }

        return false;
    }

    if (c.isPlayerOwned)
    {
        const auto maxVisible = profile.MaxPlayerOwnedVisible.get();
        if (maxVisible > 0 && numPlayerOwnedVisible >= static_cast<uint32_t>(maxVisible))
        {
            hideReason = EHideReason::MaxPlayerOwned;
            return true;
        }

        if (profile.HideOwnedInstances.get() && instanced && !c.isOwnerLocalPlayer)
        {
            hideReason = EHideReason::OwnedInstances;
            return true;
        }

        if (c.isOwnerLocalPlayer && profile.HidePlayerOwnedSelf.get())
        {
            hideReason = EHideReason::OwnedSelf;
            return true;
        }

        if (profile.HidePlayerOwned.get())
        {
            hideReason = EHideReason::HidePlayerOwned;
            return true;
        }

        if (profile.HideNonFriendsOwned.get() && !c.isFriend)
        {
            hideReason = EHideReason::NonFriendsOwned;
            return true;
        }

        if (profile.HideBlockedPlayersOwned.get() && c.isBlocked)
        {
            hideReason = EHideReason::BlockedOwned;
            return true;
        }

        if (profile.HideNonGuildMembersOwned.get() && !(c.isActiveGuildMember || c.isGuildMember))
        {
            hideReason = EHideReason::NonGuildOwned;
            return true;
        }

        const bool groupMember = c.isPartyMember || c.isSquadMember;
        if (profile.HideNonGroupMembersOwned.get() && !groupMember)
        {
            hideReason = EHideReason::NonGroupOwned;
            return true;
        }

        if (profile.HideStrangersOwned.get() && !c.isOwnerLocalPlayer && !c.isFriend && !c.isPartyMember
            && !c.isSquadMember && !c.isActiveGuildMember)
        {
            hideReason = EHideReason::StrangersOwned;
            return true;
        }

        if (!c.isOwnerLocalPlayer && profile.HidePlayerOwnedInCombat.get() && mumbleLink
            && mumbleLink->getContext().isInCombat())
        {
            hideReason = EHideReason::InCombatOwned;
            return true;
        }

        return false;
    }

    const auto maxVisible = profile.MaxNpcs.get();
    if (maxVisible > 0 && numNPCVisible >= static_cast<uint32_t>(maxVisible))
    {
        hideReason = EHideReason::MaxNpcs;
        return true;
    }

    return false;
}

bool unpc::shouldShow(const re::CharacterEntry& c, const bool instanced)
{
    if (!settings)
    {
        return false;
    }

    hideReason = EHideReason::None;

    const auto& profile = settings->profile();

    if (c.isTarget && profile.AlwaysShowTarget.get())
    {
        return true;
    }

    if (c.isPlayerOwned && c.isOwnerLocalPlayer && profile.AlwaysShowOwnedSelf.get())
    {
        return true;
    }

    if (profile.AlwaysShowGroup.get() && (c.isPartyMember || c.isSquadMember))
    {
        return true;
    }

    if (profile.AlwaysShowFriends.get() && c.isFriend)
    {
        return true;
    }

    if (profile.AlwaysShowAttackable.get() && c.isAttackable)
    {
        return true;
    }

    if (!c.isOnScreen && profile.HideOffScreen.get())
    {
        ++numOffScreenCulled;
        hideReason = EHideReason::OffScreen;
        return false;
    }

    if (profile.MaximumDistance.get() > 0 && c.distance >= profile.MaximumDistance.get())
    {
        hideReason = EHideReason::MaximumDistance;
        return false;
    }

    if (c.isPlayer)
    {
        if (!profile.UnhidePlayers.get())
        {
            hideReason = EHideReason::NotUnhidden;
            return false;
        }

        const auto maxVisible = profile.MaxPlayersVisible.get();
        if (maxVisible > 0 && numPlayersVisible >= static_cast<uint32_t>(maxVisible))
        {
            hideReason = EHideReason::MaxPlayers;
            return false;
        }

        if (profile.HidePlayers.get())
        {
            hideReason = EHideReason::HidePlayers;
            return false;
        }

        if (profile.HideNonFriends.get() && !c.isFriend)
        {
            hideReason = EHideReason::NonFriends;
            return false;
        }

        if (profile.HideBlockedPlayers.get() && c.isBlocked)
        {
            hideReason = EHideReason::Blocked;
            return false;
        }

        if (profile.HideNonGuildMembers.get() && !(c.isActiveGuildMember || c.isGuildMember))
        {
            hideReason = EHideReason::NonGuild;
            return false;
        }

        const bool groupMember = c.isPartyMember || c.isSquadMember;
        if (profile.HideNonGroupMembers.get() && !groupMember)
        {
            hideReason = EHideReason::NonGroup;
            return false;
        }

        if (profile.HideStrangers.get()
            && (!c.isFriend && !c.isPartyMember && !c.isSquadMember && !c.isActiveGuildMember))
        {
            hideReason = EHideReason::Strangers;
            return false;
        }

        if (profile.HidePlayersInCombat.get() && mumbleLink && mumbleLink->getContext().isInCombat())
        {
            hideReason = EHideReason::InCombat;
            return false;
        }

        return true;
    }

    if (c.isPlayerOwned)
    {
        if (!profile.PlayerOwned.get())
        {
            hideReason = EHideReason::NotUnhidden;
            return false;
        }

        const auto maxVisible = profile.MaxPlayerOwnedVisible.get();
        if (maxVisible > 0 && numPlayerOwnedVisible >= static_cast<uint32_t>(maxVisible))
        {
            hideReason = EHideReason::MaxPlayerOwned;
            return false;
        }

        if (profile.HideOwnedInstances.get() && instanced && !c.isOwnerLocalPlayer)
        {
            hideReason = EHideReason::OwnedInstances;
            return false;
        }

        if (profile.HidePlayerOwnedSelf.get() && c.isOwnerLocalPlayer)
        {
            hideReason = EHideReason::OwnedSelf;
            return false;
        }

        if (profile.HidePlayerOwned.get())
        {
            hideReason = EHideReason::HidePlayerOwned;
            return false;
        }

        if (profile.HideNonFriendsOwned.get() && !c.isFriend)
        {
            hideReason = EHideReason::NonFriendsOwned;
            return false;
        }

        if (profile.HideBlockedPlayersOwned.get() && c.isBlocked)
        {
            hideReason = EHideReason::BlockedOwned;
            return false;
        }

        if (profile.HideNonGuildMembersOwned.get() && !(c.isActiveGuildMember || c.isGuildMember))
        {
            hideReason = EHideReason::NonGuildOwned;
            return false;
        }

        const bool groupMember = c.isPartyMember || c.isSquadMember;
        if (profile.HideNonGroupMembersOwned.get() && !groupMember)
        {
            hideReason = EHideReason::NonGroupOwned;
            return false;
        }

        if (profile.HideStrangersOwned.get() && !c.isOwnerLocalPlayer && !c.isFriend && !c.isPartyMember
            && !c.isSquadMember && !c.isActiveGuildMember)
        {
            hideReason = EHideReason::StrangersOwned;
            return false;
        }

        if (!c.isOwnerLocalPlayer && profile.HidePlayerOwnedInCombat.get() && mumbleLink
            && mumbleLink->getContext().isInCombat())
        {
            hideReason = EHideReason::InCombatOwned;
            return false;
        }

        return true;
    }

    if (!profile.UnhideNPCs.get())
    {
        hideReason = EHideReason::NotUnhidden;
        return false;
    }

    const auto maxVisible = profile.MaxNpcs.get();
    if (maxVisible > 0 && numNPCVisible >= static_cast<uint32_t>(maxVisible))
    {
        hideReason = EHideReason::MaxNpcs;
        return false;
    }

    if (c.rank < profile.MinimumRank.get())
    {
        hideReason = EHideReason::Rank;
        return false;
    }

    if (profile.Attackable.get() == 1 && !c.isAttackable)
    {
        hideReason = EHideReason::AttackableFilter;
        return false;
    }
    if (profile.Attackable.get() == 2 && c.isAttackable)
    {
        hideReason = EHideReason::AttackableFilter;
        return false;
    }

    return true;
}


void unpc::start()
{
    const auto start = std::chrono::high_resolution_clock::now();

    if (settings && logger && hook)
    {
        return;
    }

    // Keep the add-on quiet in Nexus: no per-frame or framework debug output.
    auto logLevel = logging::LogLevel::Info;

    logger.emplace(
        "ReUnhideNPCs", std::filesystem::current_path() / "addons" / "ReUnhideNPCs" / "reunhidenpcs.log", logLevel);

    LOG_INFO("Starting");
    LOG_INFO("Version {}", version::STRING);

    if (injected)
    {
        LOG_INFO("Mode: Injected");
        mode = EMode::Injected;
    }
    else if (hProxyModule)
    {
        LOG_INFO("Mode: Proxy({})", proxyModuleName);
        mode = EMode::Proxy;
    }
    else if (nexusPresent && util::isModuleInAnyDirsRelativeToExe(hModule, {"addons"}))
    {
        LOG_INFO("Mode: Nexus");
        mode = EMode::Nexus;
        logger->registerCallback(nexus::logCallback);
    }
    else if (arcDpsPresent && util::isModuleInAnyDirsRelativeToExe(hModule, {"", "bin64"}))
    {
        LOG_INFO("Mode: ArcDPS");
        mode = EMode::ArcDps;
    }
    else
    {
        LOG_INFO("Mode: Unknown");
        mode = EMode::Unknown;
        exit = true;
        return;
    }

    const auto result   = initialize();
    const auto end      = std::chrono::high_resolution_clock::now();
    auto       duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (!result)
    {
        LOG_ERR("Initialization failed");
        exit = true;
        return;
    }

    LOG_INFO("Started in {}ms", duration);
}

void unpc::stop()
{
    LOG_INFO("Stopping");

    // invalidate character states so that the game will apply its own settings
    ++re::forceInvalidate;
    stopping.store(true, std::memory_order_release);

    while (re::forceInvalidate > 0)
    {
        ;
    }

    if (ui::d3dHook)
    {
        ui::d3dHook.reset();
    }

    if (hook)
    {
        if (hook->enabled())
        {
            hook->disable();

            while (re::hookInFlight.load(std::memory_order_acquire) != 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    if (hook2)
    {
        if (hook2->enabled())
        {
            hook2->disable();
        }
    }

    re::shutdown();

    MH_Uninitialize();

    if (mode == EMode::Nexus)
    {
        logger->unregisterCallback(nexus::logCallback);
    }

    settings.reset();
    logger.reset();

    if (mumbleLink && mode != EMode::Nexus)
    {
        UnmapViewOfFile(mumbleLink);
        mumbleLink = nullptr;
    }

    util::closeHandle(hMutex);
}

void unpc::entrypoint()
{
    // need to use CreateThread because for some reason std::thread doesn't allow the dll to unload cleanly
    hThread = CreateThread(
        nullptr,
        0,
        [](PVOID) -> DWORD
        {
            start();

            while (!exit)
            {
                if (injected && ui::wasKeyPressed(VK_END))
                {
                    exit = true;
                    break;
                }

                Sleep(250);
            }

            stop();

            util::closeHandle(hThread);
            if (!hProxyModule)
            {
                FreeLibraryAndExitThread(hModule, 0);
            }

            // otherwise just free the proxy module(decrease refcount) if we have one and exit the thread
            util::freeLibrary(hProxyModule);
            ExitThread(0);
        },
        nullptr,
        0,
        nullptr);
}

bool unpc::findAsserts(Module&              module,
                       const size_t         lineNumber,
                       const std::string&   file,
                       const std::string&   message,
                       std::vector<Handle>& results)
{
    if (lineNumber != SIZE_MAX)
    {
        LOG_DBG("Looking for asserts [0x{:X}, \"{}\", \"{}\"]", lineNumber, file, message);
    }
    else
    {
        LOG_DBG("Looking for asserts [\"{}\", \"{}\"]", file, message);
    }
    results.clear();

    std::vector<RefData> fileRefs {};
    std::vector<RefData> messageRefs {};

    if (!module.findStringReferences(file, fileRefs))
    {
        return false;
    }

    if (!module.findStringReferences(message, messageRefs))
    {
        return false;
    }

    for (const auto& messageRefData : messageRefs)
    {
        for (const auto& fileRefData : fileRefs)
        {
            const auto& fileRef    = fileRefData.instruction();
            const auto& messageRef = messageRefData.instruction();

            // 7 bytes before the reference to the message should be a reference to the file
            if (messageRef.sub(7) != fileRef)
            {
                continue;
            }

            // the subsequent instruction should be a call
            if (messageRef.add(7).deref<uint8_t>() != 0xE8)
            {
                continue;
            }

            // the preceding instruction should be a mov, matching the number
            const auto mov = fileRef.sub(5);
            if (mov.deref<uint8_t>() != 0xB8)
            {
                continue;
            }
            if (lineNumber != SIZE_MAX && mov.add(1).deref<i32>() != lineNumber)
            {
                continue;
            }

            // check if mov is rex prefixed
            if ((mov.sub(1).deref<uint8_t>() & 0xF0) == 0x40)
            {
                results.push_back(mov.sub(1));
            }
            else
            {
                results.push_back(mov);
            }
        }
    }

    LOG_DBG("Found {} asserts", results.size());
    return !results.empty();
}

bool
unpc::findAsserts(Module& module, const std::string& file, const std::string& message, std::vector<Handle>& results)
{
    return findAsserts(module, SIZE_MAX, file, message, results);
}
