#pragma once

#include <Canis/AssetHandle.hpp>
#include <Canis/Math.hpp>
#include <Canis/UUID.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Canis
{
    class App;
    class Entity;

    using NetworkClientId = std::uint32_t;
    using NetworkObjectId = std::uint32_t;

    enum class NetworkMode
    {
        Offline,
        Host,
        Client
    };

    enum class NetworkPhase
    {
        None,
        Start,
        Lobby,
        Match
    };

    struct NetworkPlayer
    {
        NetworkClientId id = 0;
        std::string name = "Player";
        bool ready = false;
        bool host = false;
        bool connected = true;
    };

    struct NetworkTransformState
    {
        Vector3 position = Vector3(0.0f);
        Vector3 rotation = Vector3(0.0f);
        float receivedTime = 0.0f;
    };

    struct NetworkInputState
    {
        float throttle = 0.0f;
        float steer = 0.0f;
        float lookYaw = 0.0f;
        float lookPitch = 0.0f;
        bool jump = false;
        bool firePrimary = false;
        bool fireSecondary = false;
        float receivedTime = 0.0f;
    };

    struct NetworkRigidbodyState
    {
        Vector3 position = Vector3(0.0f);
        Vector3 rotation = Vector3(0.0f);
        Vector3 linearVelocity = Vector3(0.0f);
        Vector3 angularVelocity = Vector3(0.0f);
        float receivedTime = 0.0f;
    };

    struct NetworkCombatState
    {
        int health = 100;
        int maxHealth = 100;
        int kills = 0;
        int deaths = 0;
        bool alive = true;
        float respawnSecondsRemaining = 0.0f;
        float damageFlash = 0.0f;
        float receivedTime = 0.0f;
    };

    struct NetworkIdentity
    {
        static constexpr const char* ScriptName = "Canis::NetworkIdentity";

        NetworkIdentity() = default;
        explicit NetworkIdentity(Canis::Entity& _entity) : entity(&_entity) {}

        void Create() {}

        Entity* entity = nullptr;
        NetworkObjectId netId = 0;
        NetworkClientId ownerClientId = 0;
        bool serverOwned = false;
        bool localOwned = false;
        bool replicateTransform = true;
        SceneAssetHandle prefab = {};
    };

    class NetworkSession
    {
    public:
        explicit NetworkSession(App &_app);
        ~NetworkSession();

        bool Host(
            std::uint16_t _port = 7777,
            const std::string &_playerName = "Host",
            const std::string &_lobbyScenePath = "assets/scenes/boat_lobby.scene");
        bool Join(
            const std::string &_address,
            std::uint16_t _port = 7777,
            const std::string &_playerName = "Player",
            const std::string &_lobbyScenePath = "assets/scenes/boat_lobby.scene");
        void Disconnect();

        void Update(float _deltaTime);

        bool IsOffline() const { return m_mode == NetworkMode::Offline; }
        bool IsHost() const { return m_mode == NetworkMode::Host; }
        bool IsClient() const { return m_mode == NetworkMode::Client; }
        bool IsConnected() const { return m_mode != NetworkMode::Offline; }
        bool IsMine(NetworkClientId _ownerClientId) const;

        NetworkMode GetMode() const { return m_mode; }
        NetworkPhase GetPhase() const { return m_phase; }
        NetworkClientId GetLocalClientId() const { return m_localClientId; }
        const std::vector<NetworkPlayer>& GetPlayers() const { return m_players; }
        std::string GetStatusText() const;

        void SetLobbyScene(const std::string &_scenePath) { m_lobbyScenePath = _scenePath; }
        const std::string& GetLobbyScene() const { return m_lobbyScenePath; }
        const std::string& GetMatchScene() const { return m_matchScenePath; }

        bool StartMatch(const std::string &_scenePath, float _durationSeconds);
        void ReturnToLobby();
        float GetMatchRemainingSeconds() const { return m_matchRemainingSeconds; }

        void PublishTransform(const std::string &_key, const Vector3 &_position, const Vector3 &_rotation);
        bool TryGetTransform(const std::string &_key, NetworkTransformState &_outState) const;
        void PublishInput(const std::string &_key, const NetworkInputState &_state);
        void PublishInput(const std::string &_key, float _throttle, float _steer);
        bool TryGetInput(const std::string &_key, NetworkInputState &_outState) const;
        void PublishRigidbody(
            const std::string &_key,
            const Vector3 &_position,
            const Vector3 &_rotation,
            const Vector3 &_linearVelocity,
            const Vector3 &_angularVelocity);
        bool TryGetRigidbody(const std::string &_key, NetworkRigidbodyState &_outState) const;
        void PublishCombat(const std::string &_key, const NetworkCombatState &_state);
        bool TryGetCombat(const std::string &_key, NetworkCombatState &_outState) const;

    private:
        struct Impl;

        App &m_app;
        Impl *m_impl = nullptr;
        NetworkMode m_mode = NetworkMode::Offline;
        NetworkPhase m_phase = NetworkPhase::None;
        NetworkClientId m_localClientId = 0;
        NetworkClientId m_nextClientId = 2;
        std::vector<NetworkPlayer> m_players = {};
        std::string m_playerName = "Player";
        std::string m_lobbyScenePath = "assets/scenes/boat_lobby.scene";
        std::string m_matchScenePath = "";
        float m_matchDurationSeconds = 60.0f;
        float m_matchRemainingSeconds = 0.0f;
        float m_timeSeconds = 0.0f;
        float m_broadcastTimer = 0.0f;
        std::unordered_map<std::string, NetworkTransformState> m_transformStates = {};
        std::unordered_map<std::string, NetworkInputState> m_inputStates = {};
        std::unordered_map<std::string, NetworkRigidbodyState> m_rigidbodyStates = {};
        std::unordered_map<std::string, NetworkCombatState> m_combatStates = {};

        NetworkPlayer* FindPlayer(NetworkClientId _id);
        const NetworkPlayer* FindPlayer(NetworkClientId _id) const;
        void AddOrUpdatePlayer(const NetworkPlayer &_player);
        void LoadLobbyScene();
        void HandlePacket(const std::string &_message, const std::string &_peerKey);
        void BroadcastPlayers();
        void BroadcastMessage(const std::string &_message);
        void SendMessageToServer(const std::string &_message);
    };
}
