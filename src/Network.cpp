#include <Canis/Network.hpp>

#include <Canis/App.hpp>
#include <Canis/Debug.hpp>

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <algorithm>
#include <charconv>
#include <sstream>
#include <utility>

namespace Canis
{
    namespace
    {
        struct NetworkEndpoint
        {
            NET_Address *address = nullptr;
            Uint16 port = 0;
        };

        void ResetEndpoint(NetworkEndpoint &_endpoint)
        {
            if (_endpoint.address != nullptr)
            {
                NET_UnrefAddress(_endpoint.address);
                _endpoint.address = nullptr;
            }

            _endpoint.port = 0;
        }

        void AssignEndpoint(NetworkEndpoint &_endpoint, NET_Address *_address, Uint16 _port)
        {
            if (_endpoint.address == _address && _endpoint.port == _port)
                return;

            ResetEndpoint(_endpoint);
            if (_address != nullptr)
                _endpoint.address = NET_RefAddress(_address);
            _endpoint.port = _port;
        }

        std::string AddressKey(NET_Address *_address, Uint16 _port)
        {
            const char *addressText = (_address != nullptr) ? NET_GetAddressString(_address) : nullptr;
            return std::string(addressText != nullptr ? addressText : "unknown") + ":" + std::to_string(_port);
        }

        std::vector<std::string> Split(const std::string &_text, char _delimiter)
        {
            std::vector<std::string> parts = {};
            std::stringstream stream(_text);
            std::string part;
            while (std::getline(stream, part, _delimiter))
                parts.push_back(part);
            return parts;
        }

        template <typename T>
        T ParseNumber(const std::string &_text, T _fallback)
        {
            T value = _fallback;
            std::from_chars(_text.data(), _text.data() + _text.size(), value);
            return value;
        }

        std::string FormatFloat(float _value)
        {
            std::ostringstream stream;
            stream << _value;
            return stream.str();
        }

        std::string SanitizeToken(std::string _value)
        {
            for (char &c : _value)
            {
                if (c == '|' || c == ';' || c == ',')
                    c = '_';
            }
            return _value;
        }
    }

    struct NetworkSession::Impl
    {
        bool initialized = false;
        NET_DatagramSocket *socket = nullptr;
        NetworkEndpoint serverEndpoint = {};
        std::unordered_map<std::string, NetworkEndpoint> peers = {};
        std::unordered_map<NetworkClientId, std::string> clientPeerKeys = {};

        ~Impl()
        {
            Close();
            if (initialized)
                NET_Quit();
        }

        bool EnsureInitialized()
        {
            if (initialized)
                return true;

            initialized = NET_Init();
            return initialized;
        }

        bool Open(std::uint16_t _port)
        {
            if (!EnsureInitialized())
                return false;

            CloseSocket();
            socket = NET_CreateDatagramSocket(nullptr, static_cast<Uint16>(_port));
            return socket != nullptr;
        }

        void CloseSocket()
        {
            if (socket != nullptr)
            {
                NET_DestroyDatagramSocket(socket);
                socket = nullptr;
            }
        }

        void Close()
        {
            CloseSocket();
            ResetEndpoint(serverEndpoint);
            for (auto &[_, peer] : peers)
                ResetEndpoint(peer);
            peers.clear();
            clientPeerKeys.clear();
        }

        bool IsOpen() const
        {
            return socket != nullptr;
        }

        bool ResolveServer(const std::string &_address, std::uint16_t _port)
        {
            if (!EnsureInitialized())
                return false;

            NET_Address *resolved = NET_ResolveHostname(_address.c_str());
            if (resolved == nullptr)
                return false;

            const NET_Status status = NET_WaitUntilResolved(resolved, 2000);
            if (status != NET_SUCCESS)
            {
                NET_UnrefAddress(resolved);
                return false;
            }

            AssignEndpoint(serverEndpoint, resolved, static_cast<Uint16>(_port));
            NET_UnrefAddress(resolved);
            return true;
        }

        void RememberPeer(const std::string &_peerKey, NET_Address *_address, Uint16 _port)
        {
            AssignEndpoint(peers[_peerKey], _address, _port);
        }

        void SendTo(const NetworkEndpoint &_endpoint, const std::string &_message)
        {
            if (socket == nullptr || _endpoint.address == nullptr || _message.empty())
                return;

            if (!NET_SendDatagram(socket, _endpoint.address, _endpoint.port, _message.data(), static_cast<int>(_message.size())))
            {
                // Keep the session alive; transient send failures are okay for this prototype.
            }
        }

        std::vector<std::pair<std::string, std::string>> Poll()
        {
            std::vector<std::pair<std::string, std::string>> packets = {};
            if (socket == nullptr)
                return packets;

            for (;;)
            {
                NET_Datagram *datagram = nullptr;
                if (!NET_ReceiveDatagram(socket, &datagram))
                {
                    Debug::Warning("SDL3_net receive error: %s", SDL_GetError());
                    break;
                }

                if (datagram == nullptr)
                    break;

                const std::string key = AddressKey(datagram->addr, datagram->port);
                RememberPeer(key, datagram->addr, datagram->port);
                packets.push_back({ std::string(reinterpret_cast<const char *>(datagram->buf), static_cast<std::size_t>(datagram->buflen)), key });
                NET_DestroyDatagram(datagram);
            }

            return packets;
        }
    };

    NetworkSession::NetworkSession(App &_app)
        : m_app(_app)
        , m_impl(new Impl())
    {
        if (!m_impl->EnsureInitialized())
            Debug::Warning("SDL3_net failed to initialize: %s", SDL_GetError());
    }

    NetworkSession::~NetworkSession()
    {
        Disconnect();
        delete m_impl;
        m_impl = nullptr;
    }

    bool NetworkSession::Host(std::uint16_t _port, const std::string &_playerName, const std::string &_lobbyScenePath)
    {
        Disconnect();

        m_playerName = SanitizeToken(_playerName.empty() ? "Host" : _playerName);
        if (!_lobbyScenePath.empty())
            m_lobbyScenePath = _lobbyScenePath;
        m_mode = NetworkMode::Host;
        m_phase = NetworkPhase::Lobby;
        m_localClientId = 1;
        m_nextClientId = 2;
        m_players.clear();
        m_transformStates.clear();
        m_inputStates.clear();
        m_rigidbodyStates.clear();
        m_combatStates.clear();
        m_gameStates.clear();
        m_gameActions.clear();
        m_players.push_back(NetworkPlayer{ .id = 1, .name = m_playerName, .ready = true, .host = true, .connected = true });

        if (!m_impl->Open(_port))
            Debug::Warning("NetworkSession host could not open UDP port %u through SDL3_net. Continuing as local host.", static_cast<unsigned int>(_port));
        else
            Debug::Log("Hosting network session on UDP port %u.", static_cast<unsigned int>(_port));

        LoadLobbyScene();
        return true;
    }

    bool NetworkSession::Join(const std::string &_address, std::uint16_t _port, const std::string &_playerName, const std::string &_lobbyScenePath)
    {
        Disconnect();

        m_playerName = SanitizeToken(_playerName.empty() ? "Player" : _playerName);
        if (!_lobbyScenePath.empty())
            m_lobbyScenePath = _lobbyScenePath;
        m_mode = NetworkMode::Client;
        m_phase = NetworkPhase::Start;
        m_localClientId = 0;
        m_players.clear();
        m_transformStates.clear();
        m_inputStates.clear();
        m_rigidbodyStates.clear();
        m_combatStates.clear();
        m_gameStates.clear();
        m_gameActions.clear();

        const std::string address = _address.empty() ? "127.0.0.1" : _address;

        if (!m_impl->Open(0))
        {
            Debug::Warning("NetworkSession client could not open a UDP socket through SDL3_net.");
            return false;
        }

        if (!m_impl->ResolveServer(address, _port))
        {
            Debug::Warning("NetworkSession could not resolve host '%s:%u' through SDL3_net.", address.c_str(), static_cast<unsigned int>(_port));
            Disconnect();
            return false;
        }

        SendMessageToServer("HELLO|" + m_playerName + "|" + m_lobbyScenePath);
        Debug::Log(
            "Connecting to network host %s:%u.",
            address.c_str(),
            static_cast<unsigned int>(_port));
        return true;
    }

    void NetworkSession::Disconnect()
    {
        if (m_impl != nullptr && m_impl->socket != nullptr)
        {
            if (m_mode == NetworkMode::Client && m_localClientId != 0)
                SendMessageToServer("BYE|" + std::to_string(m_localClientId));
            else if (m_mode == NetworkMode::Host)
                BroadcastMessage("CLOSED");
        }
        if (m_impl != nullptr)
            m_impl->Close();

        m_mode = NetworkMode::Offline;
        m_phase = NetworkPhase::None;
        m_localClientId = 0;
        m_nextClientId = 2;
        m_players.clear();
        m_matchScenePath.clear();
        m_matchRemainingSeconds = 0.0f;
        m_transformStates.clear();
        m_inputStates.clear();
        m_rigidbodyStates.clear();
        m_combatStates.clear();
        m_gameStates.clear();
        m_gameActions.clear();
        m_broadcastTimer = 0.0f;
    }

    void NetworkSession::Update(float _deltaTime)
    {
        m_timeSeconds += _deltaTime;

        if (m_impl != nullptr)
        {
            for (const auto &packet : m_impl->Poll())
                HandlePacket(packet.first, packet.second);
        }

        if (m_mode == NetworkMode::Client && m_localClientId == 0)
        {
            m_broadcastTimer += _deltaTime;
            if (m_broadcastTimer >= 0.5f)
            {
                m_broadcastTimer = 0.0f;
                SendMessageToServer("HELLO|" + m_playerName + "|" + m_lobbyScenePath);
            }
            return;
        }

        if (m_mode == NetworkMode::Host)
        {
            m_broadcastTimer += _deltaTime;
            if (m_broadcastTimer >= 1.0f)
            {
                m_broadcastTimer = 0.0f;
                BroadcastPlayers();
            }

            if (m_phase == NetworkPhase::Match)
            {
                m_matchRemainingSeconds = std::max(0.0f, m_matchRemainingSeconds - _deltaTime);
                if (m_matchRemainingSeconds <= 0.0f)
                    ReturnToLobby();
            }
        }
    }

    bool NetworkSession::IsMine(NetworkClientId _ownerClientId) const
    {
        if (m_mode == NetworkMode::Offline)
            return true;

        return _ownerClientId == m_localClientId;
    }

    std::string NetworkSession::GetStatusText() const
    {
        const char *mode = "Offline";
        if (m_mode == NetworkMode::Host)
            mode = "Host";
        else if (m_mode == NetworkMode::Client)
            mode = "Client";

        const char *phase = "None";
        if (m_phase == NetworkPhase::Start)
            phase = "Start";
        else if (m_phase == NetworkPhase::Lobby)
            phase = "Lobby";
        else if (m_phase == NetworkPhase::Match)
            phase = "Match";

        return std::string(mode) + " / " + phase + " / players: " + std::to_string(m_players.size());
    }

    bool NetworkSession::StartMatch(const std::string &_scenePath, float _durationSeconds)
    {
        if (!IsHost() || _scenePath.empty())
            return false;

        m_phase = NetworkPhase::Match;
        m_matchScenePath = _scenePath;
        m_matchDurationSeconds = std::max(1.0f, _durationSeconds);
        m_matchRemainingSeconds = m_matchDurationSeconds;
        m_transformStates.clear();
        m_inputStates.clear();
        m_rigidbodyStates.clear();
        m_combatStates.clear();
        m_gameStates.clear();
        m_gameActions.clear();

        BroadcastMessage("START|" + m_matchScenePath + "|" + FormatFloat(m_matchDurationSeconds));
        m_app.LoadScene(m_matchScenePath);
        return true;
    }

    void NetworkSession::ReturnToLobby()
    {
        if (m_mode == NetworkMode::Offline)
            return;

        m_phase = NetworkPhase::Lobby;
        m_matchRemainingSeconds = 0.0f;
        m_transformStates.clear();
        m_inputStates.clear();
        m_rigidbodyStates.clear();
        m_combatStates.clear();
        m_gameStates.clear();
        m_gameActions.clear();

        if (m_mode == NetworkMode::Host)
            BroadcastMessage("LOBBY|" + m_lobbyScenePath);

        LoadLobbyScene();
    }

    void NetworkSession::PublishTransform(const std::string &_key, const Vector3 &_position, const Vector3 &_rotation)
    {
        if (_key.empty())
            return;

        m_transformStates[_key] = NetworkTransformState{ .position = _position, .rotation = _rotation, .receivedTime = m_timeSeconds };

        const std::string message =
            "STATE|" + _key + "|" +
            FormatFloat(_position.x) + "|" + FormatFloat(_position.y) + "|" + FormatFloat(_position.z) + "|" +
            FormatFloat(_rotation.x) + "|" + FormatFloat(_rotation.y) + "|" + FormatFloat(_rotation.z);

        if (m_mode == NetworkMode::Host)
            BroadcastMessage(message);
        else if (m_mode == NetworkMode::Client && m_localClientId != 0)
            SendMessageToServer(message);
    }

    bool NetworkSession::TryGetTransform(const std::string &_key, NetworkTransformState &_outState) const
    {
        const auto it = m_transformStates.find(_key);
        if (it == m_transformStates.end())
            return false;

        _outState = it->second;
        return true;
    }

    void NetworkSession::PublishInput(const std::string &_key, const NetworkInputState &_state)
    {
        if (_key.empty())
            return;

        NetworkInputState state = _state;
        state.receivedTime = m_timeSeconds;
        m_inputStates[_key] = state;

        const std::string message =
            "INPUT|" + _key + "|" +
            FormatFloat(state.throttle) + "|" +
            FormatFloat(state.steer) + "|" +
            FormatFloat(state.lookYaw) + "|" +
            FormatFloat(state.lookPitch) + "|" +
            (state.jump ? "1" : "0") + "|" +
            (state.firePrimary ? "1" : "0") + "|" +
            (state.fireSecondary ? "1" : "0");

        if (m_mode == NetworkMode::Host)
            BroadcastMessage(message);
        else if (m_mode == NetworkMode::Client && m_localClientId != 0)
            SendMessageToServer(message);
    }

    void NetworkSession::PublishInput(const std::string &_key, float _throttle, float _steer)
    {
        NetworkInputState state = {};
        state.throttle = _throttle;
        state.steer = _steer;
        PublishInput(_key, state);
    }

    bool NetworkSession::TryGetInput(const std::string &_key, NetworkInputState &_outState) const
    {
        const auto it = m_inputStates.find(_key);
        if (it == m_inputStates.end())
            return false;

        _outState = it->second;
        return true;
    }

    void NetworkSession::PublishRigidbody(
        const std::string &_key,
        const Vector3 &_position,
        const Vector3 &_rotation,
        const Vector3 &_linearVelocity,
        const Vector3 &_angularVelocity)
    {
        if (_key.empty())
            return;

        NetworkRigidbodyState state{};
        state.position = _position;
        state.rotation = _rotation;
        state.linearVelocity = _linearVelocity;
        state.angularVelocity = _angularVelocity;
        state.receivedTime = m_timeSeconds;
        m_rigidbodyStates[_key] = state;

        const std::string message =
            "RBSTATE|" + _key + "|" +
            FormatFloat(_position.x) + "|" + FormatFloat(_position.y) + "|" + FormatFloat(_position.z) + "|" +
            FormatFloat(_rotation.x) + "|" + FormatFloat(_rotation.y) + "|" + FormatFloat(_rotation.z) + "|" +
            FormatFloat(_linearVelocity.x) + "|" + FormatFloat(_linearVelocity.y) + "|" + FormatFloat(_linearVelocity.z) + "|" +
            FormatFloat(_angularVelocity.x) + "|" + FormatFloat(_angularVelocity.y) + "|" + FormatFloat(_angularVelocity.z);

        if (m_mode == NetworkMode::Host)
            BroadcastMessage(message);
    }

    bool NetworkSession::TryGetRigidbody(const std::string &_key, NetworkRigidbodyState &_outState) const
    {
        const auto it = m_rigidbodyStates.find(_key);
        if (it == m_rigidbodyStates.end())
            return false;

        _outState = it->second;
        return true;
    }

    void NetworkSession::PublishCombat(const std::string &_key, const NetworkCombatState &_state)
    {
        if (_key.empty())
            return;

        NetworkCombatState state = _state;
        state.receivedTime = m_timeSeconds;
        m_combatStates[_key] = state;

        const std::string message =
            "COMBAT|" + _key + "|" +
            std::to_string(state.health) + "|" +
            std::to_string(state.maxHealth) + "|" +
            std::to_string(state.kills) + "|" +
            std::to_string(state.deaths) + "|" +
            (state.alive ? "1" : "0") + "|" +
            FormatFloat(state.respawnSecondsRemaining) + "|" +
            FormatFloat(state.damageFlash);

        if (m_mode == NetworkMode::Host)
            BroadcastMessage(message);
    }

    bool NetworkSession::TryGetCombat(const std::string &_key, NetworkCombatState &_outState) const
    {
        const auto it = m_combatStates.find(_key);
        if (it == m_combatStates.end())
            return false;

        _outState = it->second;
        return true;
    }

    void NetworkSession::PublishGameState(const std::string &_key, const std::string &_value)
    {
        if (_key.empty() || !IsHost())
            return;

        const std::string key = SanitizeToken(_key);
        const std::string value = SanitizeToken(_value);
        m_gameStates[key] = value;
        BroadcastMessage("GAMESTATE|" + key + "|" + value);
    }

    bool NetworkSession::TryGetGameState(const std::string &_key, std::string &_outValue) const
    {
        const auto it = m_gameStates.find(_key);
        if (it == m_gameStates.end())
            return false;

        _outValue = it->second;
        return true;
    }

    void NetworkSession::SendGameAction(const std::string &_action, const std::string &_payload)
    {
        if (_action.empty() || IsOffline())
            return;

        const std::string action = SanitizeToken(_action);
        const std::string payload = SanitizeToken(_payload);
        if (IsHost())
        {
            m_gameActions.push_back(NetworkGameAction{
                .senderClientId = m_localClientId,
                .action = action,
                .payload = payload });
            return;
        }

        if (m_localClientId != 0)
            SendMessageToServer("GAMEACTION|" + action + "|" + payload);
    }

    std::vector<NetworkGameAction> NetworkSession::ConsumeGameActions()
    {
        std::vector<NetworkGameAction> actions = {};
        actions.swap(m_gameActions);
        return actions;
    }

    NetworkPlayer* NetworkSession::FindPlayer(NetworkClientId _id)
    {
        for (NetworkPlayer &player : m_players)
        {
            if (player.id == _id)
                return &player;
        }

        return nullptr;
    }

    const NetworkPlayer* NetworkSession::FindPlayer(NetworkClientId _id) const
    {
        for (const NetworkPlayer &player : m_players)
        {
            if (player.id == _id)
                return &player;
        }

        return nullptr;
    }

    void NetworkSession::AddOrUpdatePlayer(const NetworkPlayer &_player)
    {
        if (NetworkPlayer *existing = FindPlayer(_player.id))
        {
            *existing = _player;
            return;
        }

        m_players.push_back(_player);
        std::sort(
            m_players.begin(),
            m_players.end(),
            [](const NetworkPlayer &_a, const NetworkPlayer &_b)
            {
                return _a.id < _b.id;
            });
    }

    void NetworkSession::LoadLobbyScene()
    {
        if (!m_lobbyScenePath.empty())
            m_app.LoadScene(m_lobbyScenePath);
    }

    void NetworkSession::HandlePacket(const std::string &_message, const std::string &_peerKey)
    {
        const std::vector<std::string> parts = Split(_message, '|');
        if (parts.empty())
            return;

        const std::string &type = parts[0];

        if (m_mode == NetworkMode::Host)
        {
            if (type == "HELLO" && parts.size() >= 2u)
            {
                NetworkClientId clientId = 0;
                for (const auto &entry : m_impl->clientPeerKeys)
                {
                    if (entry.second == _peerKey)
                    {
                        clientId = entry.first;
                        break;
                    }
                }

                if (clientId == 0)
                {
                    if (m_players.size() >= 4u)
                    {
                        if (auto peerIt = m_impl->peers.find(_peerKey); peerIt != m_impl->peers.end())
                            m_impl->SendTo(peerIt->second, "REJECT|Lobby_full");
                        return;
                    }
                    clientId = m_nextClientId++;
                    m_impl->clientPeerKeys[clientId] = _peerKey;
                }

                AddOrUpdatePlayer(NetworkPlayer{ .id = clientId, .name = parts[1], .ready = true, .host = false, .connected = true });
                Debug::Log(
                    "Network client %u ('%s') joined; lobby now has %zu/4 players.",
                    static_cast<unsigned int>(clientId),
                    parts[1].c_str(),
                    m_players.size());

                if (auto peerIt = m_impl->peers.find(_peerKey); peerIt != m_impl->peers.end())
                    m_impl->SendTo(peerIt->second, "WELCOME|" + std::to_string(clientId) + "|" + m_lobbyScenePath);
                BroadcastPlayers();
                if (m_phase == NetworkPhase::Match)
                    BroadcastMessage("START|" + m_matchScenePath + "|" + FormatFloat(m_matchRemainingSeconds));
            }
            else if (type == "STATE" && parts.size() >= 8u)
            {
                NetworkTransformState state{};
                state.position = Vector3(ParseNumber(parts[2], 0.0f), ParseNumber(parts[3], 0.0f), ParseNumber(parts[4], 0.0f));
                state.rotation = Vector3(ParseNumber(parts[5], 0.0f), ParseNumber(parts[6], 0.0f), ParseNumber(parts[7], 0.0f));
                state.receivedTime = m_timeSeconds;
                m_transformStates[parts[1]] = state;
                BroadcastMessage(_message);
            }
            else if (type == "INPUT" && parts.size() >= 4u)
            {
                NetworkInputState state{};
                state.throttle = ParseNumber(parts[2], 0.0f);
                state.steer = ParseNumber(parts[3], 0.0f);
                if (parts.size() >= 8u)
                {
                    state.lookYaw = ParseNumber(parts[4], 0.0f);
                    state.lookPitch = ParseNumber(parts[5], 0.0f);
                    state.jump = ParseNumber<int>(parts[6], 0) != 0;
                    state.firePrimary = ParseNumber<int>(parts[7], 0) != 0;
                    if (parts.size() >= 9u)
                        state.fireSecondary = ParseNumber<int>(parts[8], 0) != 0;
                }
                state.receivedTime = m_timeSeconds;
                m_inputStates[parts[1]] = state;
                BroadcastMessage(
                    "INPUT|" + parts[1] + "|" +
                    FormatFloat(state.throttle) + "|" +
                    FormatFloat(state.steer) + "|" +
                    FormatFloat(state.lookYaw) + "|" +
                    FormatFloat(state.lookPitch) + "|" +
                    (state.jump ? "1" : "0") + "|" +
                    (state.firePrimary ? "1" : "0") + "|" +
                    (state.fireSecondary ? "1" : "0"));
            }
            else if (type == "COMBAT" && parts.size() >= 9u)
            {
                NetworkCombatState state{};
                state.health = ParseNumber(parts[2], 100);
                state.maxHealth = ParseNumber(parts[3], 100);
                state.kills = ParseNumber(parts[4], 0);
                state.deaths = ParseNumber(parts[5], 0);
                state.alive = ParseNumber<int>(parts[6], 1) != 0;
                state.respawnSecondsRemaining = ParseNumber(parts[7], 0.0f);
                state.damageFlash = ParseNumber(parts[8], 0.0f);
                state.receivedTime = m_timeSeconds;
                m_combatStates[parts[1]] = state;
                BroadcastMessage(_message);
            }
            else if (type == "GAMEACTION" && parts.size() >= 2u)
            {
                NetworkClientId senderClientId = 0;
                for (const auto &entry : m_impl->clientPeerKeys)
                {
                    if (entry.second == _peerKey)
                    {
                        senderClientId = entry.first;
                        break;
                    }
                }

                if (senderClientId != 0)
                {
                    m_gameActions.push_back(NetworkGameAction{
                        .senderClientId = senderClientId,
                        .action = parts[1],
                        .payload = parts.size() >= 3u ? parts[2] : "" });
                }
            }
            else if (type == "BYE")
            {
                NetworkClientId departingId = 0;
                for (const auto &entry : m_impl->clientPeerKeys)
                {
                    if (entry.second == _peerKey)
                    {
                        departingId = entry.first;
                        break;
                    }
                }
                if (departingId != 0)
                {
                    m_players.erase(
                        std::remove_if(
                            m_players.begin(),
                            m_players.end(),
                            [&](const NetworkPlayer &player) { return player.id == departingId; }),
                        m_players.end());
                    m_impl->clientPeerKeys.erase(departingId);
                    m_impl->peers.erase(_peerKey);
                    BroadcastPlayers();
                }
            }

            return;
        }

        if (m_mode == NetworkMode::Client)
        {
            if (type == "WELCOME" && parts.size() >= 3u)
            {
                m_localClientId = ParseNumber<NetworkClientId>(parts[1], 0);
                m_lobbyScenePath = parts[2];
                m_phase = NetworkPhase::Lobby;
                Debug::Log(
                    "Joined network session as client %u.",
                    static_cast<unsigned int>(m_localClientId));
                LoadLobbyScene();
            }
            else if (type == "PLAYERS" && parts.size() >= 2u)
            {
                m_players.clear();
                const std::vector<std::string> playerRows = Split(parts[1], ';');
                for (const std::string &row : playerRows)
                {
                    if (row.empty())
                        continue;

                    const std::vector<std::string> fields = Split(row, ',');
                    if (fields.size() < 5u)
                        continue;

                    AddOrUpdatePlayer(NetworkPlayer{
                        .id = ParseNumber<NetworkClientId>(fields[0], 0),
                        .name = fields[1],
                        .ready = ParseNumber<int>(fields[2], 0) != 0,
                        .host = ParseNumber<int>(fields[3], 0) != 0,
                        .connected = ParseNumber<int>(fields[4], 1) != 0 });
                }
            }
            else if (type == "START" && parts.size() >= 3u)
            {
                m_phase = NetworkPhase::Match;
                m_matchScenePath = parts[1];
                m_matchRemainingSeconds = ParseNumber(parts[2], 60.0f);
                m_transformStates.clear();
                m_inputStates.clear();
                m_rigidbodyStates.clear();
                m_combatStates.clear();
                m_app.LoadScene(m_matchScenePath);
            }
            else if (type == "LOBBY" && parts.size() >= 2u)
            {
                m_lobbyScenePath = parts[1];
                m_phase = NetworkPhase::Lobby;
                m_transformStates.clear();
                m_inputStates.clear();
                m_rigidbodyStates.clear();
                m_combatStates.clear();
                LoadLobbyScene();
            }
            else if (type == "STATE" && parts.size() >= 8u)
            {
                NetworkTransformState state{};
                state.position = Vector3(ParseNumber(parts[2], 0.0f), ParseNumber(parts[3], 0.0f), ParseNumber(parts[4], 0.0f));
                state.rotation = Vector3(ParseNumber(parts[5], 0.0f), ParseNumber(parts[6], 0.0f), ParseNumber(parts[7], 0.0f));
                state.receivedTime = m_timeSeconds;
                m_transformStates[parts[1]] = state;
            }
            else if (type == "RBSTATE" && parts.size() >= 14u)
            {
                NetworkRigidbodyState state{};
                state.position = Vector3(ParseNumber(parts[2], 0.0f), ParseNumber(parts[3], 0.0f), ParseNumber(parts[4], 0.0f));
                state.rotation = Vector3(ParseNumber(parts[5], 0.0f), ParseNumber(parts[6], 0.0f), ParseNumber(parts[7], 0.0f));
                state.linearVelocity = Vector3(ParseNumber(parts[8], 0.0f), ParseNumber(parts[9], 0.0f), ParseNumber(parts[10], 0.0f));
                state.angularVelocity = Vector3(ParseNumber(parts[11], 0.0f), ParseNumber(parts[12], 0.0f), ParseNumber(parts[13], 0.0f));
                state.receivedTime = m_timeSeconds;
                m_rigidbodyStates[parts[1]] = state;
            }
            else if (type == "INPUT" && parts.size() >= 4u)
            {
                NetworkInputState state{};
                state.throttle = ParseNumber(parts[2], 0.0f);
                state.steer = ParseNumber(parts[3], 0.0f);
                if (parts.size() >= 8u)
                {
                    state.lookYaw = ParseNumber(parts[4], 0.0f);
                    state.lookPitch = ParseNumber(parts[5], 0.0f);
                    state.jump = ParseNumber<int>(parts[6], 0) != 0;
                    state.firePrimary = ParseNumber<int>(parts[7], 0) != 0;
                    if (parts.size() >= 9u)
                        state.fireSecondary = ParseNumber<int>(parts[8], 0) != 0;
                }
                state.receivedTime = m_timeSeconds;
                m_inputStates[parts[1]] = state;
            }
            else if (type == "GAMESTATE" && parts.size() >= 3u)
            {
                m_gameStates[parts[1]] = parts[2];
            }
            else if (type == "REJECT" || type == "CLOSED")
            {
                Disconnect();
            }
            else if (type == "COMBAT" && parts.size() >= 9u)
            {
                NetworkCombatState state{};
                state.health = ParseNumber(parts[2], 100);
                state.maxHealth = ParseNumber(parts[3], 100);
                state.kills = ParseNumber(parts[4], 0);
                state.deaths = ParseNumber(parts[5], 0);
                state.alive = ParseNumber<int>(parts[6], 1) != 0;
                state.respawnSecondsRemaining = ParseNumber(parts[7], 0.0f);
                state.damageFlash = ParseNumber(parts[8], 0.0f);
                state.receivedTime = m_timeSeconds;
                m_combatStates[parts[1]] = state;
            }
        }
    }

    void NetworkSession::BroadcastPlayers()
    {
        if (m_mode != NetworkMode::Host)
            return;

        std::string message = "PLAYERS|";
        for (const NetworkPlayer &player : m_players)
        {
            message += std::to_string(player.id) + "," +
                SanitizeToken(player.name) + "," +
                (player.ready ? "1" : "0") + "," +
                (player.host ? "1" : "0") + "," +
                (player.connected ? "1" : "0") + ";";
        }

        BroadcastMessage(message);
    }

    void NetworkSession::BroadcastMessage(const std::string &_message)
    {
        if (m_impl == nullptr || !m_impl->IsOpen())
            return;

        for (const auto &entry : m_impl->clientPeerKeys)
        {
            if (auto peerIt = m_impl->peers.find(entry.second); peerIt != m_impl->peers.end())
                m_impl->SendTo(peerIt->second, _message);
        }
    }

    void NetworkSession::SendMessageToServer(const std::string &_message)
    {
        if (m_impl != nullptr && m_impl->IsOpen())
            m_impl->SendTo(m_impl->serverEndpoint, _message);
    }
}
