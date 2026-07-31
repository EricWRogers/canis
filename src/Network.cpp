#include <Canis/Network.hpp>

#include <Canis/App.hpp>
#include <Canis/Debug.hpp>

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
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

        constexpr std::size_t RelayHeaderLength = 36;
        constexpr std::size_t RelayMaxDatagram = 1200;
        constexpr std::size_t RelayMaxPayload = RelayMaxDatagram - RelayHeaderLength;
        constexpr std::uint16_t RelayBroadcastFlag = 0x0001;

        enum class RelayPacketKind : std::uint8_t
        {
            Create = 1,
            Created = 2,
            Join = 3,
            Joined = 4,
            Data = 5,
            KeepAlive = 6,
            Leave = 7,
            Error = 8,
            LobbyClosed = 9
        };

        void AppendU16(std::vector<std::uint8_t> &_bytes, std::uint16_t _value)
        {
            _bytes.push_back(static_cast<std::uint8_t>(_value >> 8));
            _bytes.push_back(static_cast<std::uint8_t>(_value));
        }

        void AppendU32(std::vector<std::uint8_t> &_bytes, std::uint32_t _value)
        {
            for (int shift = 24; shift >= 0; shift -= 8)
                _bytes.push_back(static_cast<std::uint8_t>(_value >> shift));
        }

        void AppendU64(std::vector<std::uint8_t> &_bytes, std::uint64_t _value)
        {
            for (int shift = 56; shift >= 0; shift -= 8)
                _bytes.push_back(static_cast<std::uint8_t>(_value >> shift));
        }

        std::uint16_t ReadU16(const std::uint8_t *_bytes)
        {
            return (static_cast<std::uint16_t>(_bytes[0]) << 8) |
                static_cast<std::uint16_t>(_bytes[1]);
        }

        std::uint32_t ReadU32(const std::uint8_t *_bytes)
        {
            std::uint32_t value = 0;
            for (int index = 0; index < 4; ++index)
                value = (value << 8) | _bytes[index];
            return value;
        }

        std::uint64_t ReadU64(const std::uint8_t *_bytes)
        {
            std::uint64_t value = 0;
            for (int index = 0; index < 8; ++index)
                value = (value << 8) | _bytes[index];
            return value;
        }

        std::vector<std::uint8_t> BuildRelayPacket(
            RelayPacketKind _kind,
            std::uint16_t _flags,
            std::uint32_t _requestId,
            std::uint32_t _lobbyCode,
            std::uint32_t _participantId,
            std::uint32_t _targetId,
            std::uint64_t _sessionToken,
            const std::string &_payload)
        {
            if (_payload.size() > RelayMaxPayload)
                return {};
            std::vector<std::uint8_t> bytes = {'C', 'R', 'L', 'Y', 1, static_cast<std::uint8_t>(_kind)};
            bytes.reserve(RelayHeaderLength + _payload.size());
            AppendU16(bytes, _flags);
            AppendU32(bytes, _requestId);
            AppendU32(bytes, _lobbyCode);
            AppendU32(bytes, _participantId);
            AppendU32(bytes, _targetId);
            AppendU64(bytes, _sessionToken);
            AppendU16(bytes, static_cast<std::uint16_t>(_payload.size()));
            AppendU16(bytes, 0);
            bytes.insert(bytes.end(), _payload.begin(), _payload.end());
            return bytes;
        }
    }

    struct NetworkSession::Impl
    {
        bool initialized = false;
        NET_DatagramSocket *socket = nullptr;
        NetworkEndpoint serverEndpoint = {};
        std::unordered_map<std::string, NetworkEndpoint> peers = {};
        std::unordered_map<NetworkClientId, std::string> clientPeerKeys = {};
        bool relayTransport = false;
        bool relayReady = false;
        std::uint32_t relayLobbyCode = 0;
        std::uint32_t relayParticipantId = 0;
        std::uint64_t relaySessionToken = 0;
        std::uint32_t nextRelayMessageId = 1;

        struct RelayFragments
        {
            std::vector<std::string> chunks = {};
            std::size_t received = 0;
        };
        std::unordered_map<std::string, RelayFragments> relayFragments = {};

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
            relayTransport = false;
            relayReady = false;
            relayLobbyCode = 0;
            relayParticipantId = 0;
            relaySessionToken = 0;
            nextRelayMessageId = 1;
            relayFragments.clear();
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

        void SendBytes(const NetworkEndpoint &_endpoint, const std::vector<std::uint8_t> &_bytes)
        {
            if (socket == nullptr || _endpoint.address == nullptr || _bytes.empty())
                return;
            NET_SendDatagram(socket, _endpoint.address, _endpoint.port, _bytes.data(), static_cast<int>(_bytes.size()));
        }

        void SendRelayControl(RelayPacketKind _kind, std::uint32_t _requestId, std::uint32_t _lobbyCode, std::uint8_t _capacity = 0)
        {
            const std::string payload = _kind == RelayPacketKind::Create
                ? std::string(1, static_cast<char>(_capacity)) : std::string();
            SendBytes(serverEndpoint, BuildRelayPacket(
                _kind, 0, _requestId, _lobbyCode, 0, 0, 0, payload));
        }

        void SendRelaySessionPacket(RelayPacketKind _kind)
        {
            if (!relayReady)
                return;
            SendBytes(serverEndpoint, BuildRelayPacket(
                _kind, 0, 0, relayLobbyCode, relayParticipantId, 0,
                relaySessionToken, {}));
        }

        void SendRelayData(std::uint32_t _targetId, bool _broadcast, const std::string &_message)
        {
            if (!relayReady || _message.empty())
                return;

            const std::string messagePrefix = "@F|" + std::to_string(nextRelayMessageId++) + "|";
            const std::size_t maximumChunk = RelayMaxPayload > messagePrefix.size() + 24u
                ? RelayMaxPayload - messagePrefix.size() - 24u : 512u;
            const std::size_t count = std::max<std::size_t>(1u, (_message.size() + maximumChunk - 1u) / maximumChunk);
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::string payload = count == 1u
                    ? _message
                    : messagePrefix + std::to_string(index) + "|" + std::to_string(count) + "|" +
                        _message.substr(index * maximumChunk, maximumChunk);
                SendBytes(serverEndpoint, BuildRelayPacket(
                    RelayPacketKind::Data,
                    _broadcast ? RelayBroadcastFlag : 0,
                    0,
                    relayLobbyCode,
                    relayParticipantId,
                    _broadcast ? 0 : _targetId,
                    relaySessionToken,
                    payload));
            }
        }

        void SendToPeer(const std::string &_peerKey, const std::string &_message)
        {
            if (relayTransport && _peerKey.rfind("relay:", 0) == 0)
            {
                SendRelayData(ParseNumber<std::uint32_t>(_peerKey.substr(6), 0), false, _message);
                return;
            }
            if (auto peerIt = peers.find(_peerKey); peerIt != peers.end())
                SendTo(peerIt->second, _message);
        }

        std::optional<std::string> AcceptRelayFragment(
            std::uint32_t _senderId,
            const std::string &_payload)
        {
            if (_payload.rfind("@F|", 0) != 0)
                return _payload;
            const std::size_t idEnd = _payload.find('|', 3);
            const std::size_t indexEnd = idEnd == std::string::npos ? idEnd : _payload.find('|', idEnd + 1);
            const std::size_t countEnd = indexEnd == std::string::npos ? indexEnd : _payload.find('|', indexEnd + 1);
            if (idEnd == std::string::npos || indexEnd == std::string::npos || countEnd == std::string::npos)
                return std::nullopt;
            const std::uint32_t messageId = ParseNumber<std::uint32_t>(_payload.substr(3, idEnd - 3), 0);
            const std::size_t index = ParseNumber<std::size_t>(_payload.substr(idEnd + 1, indexEnd - idEnd - 1), 0);
            const std::size_t count = ParseNumber<std::size_t>(_payload.substr(indexEnd + 1, countEnd - indexEnd - 1), 0);
            if (messageId == 0 || count == 0 || count > 256 || index >= count)
                return std::nullopt;
            const std::string key = std::to_string(_senderId) + ":" + std::to_string(messageId);
            RelayFragments &fragments = relayFragments[key];
            if (fragments.chunks.empty())
                fragments.chunks.resize(count);
            if (fragments.chunks.size() != count)
                return std::nullopt;
            if (fragments.chunks[index].empty())
            {
                fragments.chunks[index] = _payload.substr(countEnd + 1);
                ++fragments.received;
            }
            if (fragments.received != count)
                return std::nullopt;
            std::string message;
            for (const std::string &chunk : fragments.chunks)
                message += chunk;
            relayFragments.erase(key);
            return message;
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

                const std::uint8_t *bytes = reinterpret_cast<const std::uint8_t *>(datagram->buf);
                if (relayTransport)
                {
                    const bool fromRelay = datagram->port == serverEndpoint.port &&
                        AddressKey(datagram->addr, datagram->port) ==
                            AddressKey(serverEndpoint.address, serverEndpoint.port);
                    if (fromRelay && datagram->buflen >= static_cast<int>(RelayHeaderLength) &&
                        bytes[0] == 'C' && bytes[1] == 'R' && bytes[2] == 'L' && bytes[3] == 'Y' && bytes[4] == 1)
                    {
                        const RelayPacketKind kind = static_cast<RelayPacketKind>(bytes[5]);
                        const std::uint32_t requestId = ReadU32(bytes + 8);
                        const std::uint32_t lobbyCode = ReadU32(bytes + 12);
                        const std::uint32_t participantId = ReadU32(bytes + 16);
                        const std::uint64_t token = ReadU64(bytes + 24);
                        const std::uint16_t payloadLength = ReadU16(bytes + 32);
                        if (RelayHeaderLength + payloadLength == static_cast<std::size_t>(datagram->buflen))
                        {
                            const std::string payload(reinterpret_cast<const char *>(bytes + RelayHeaderLength), payloadLength);
                            if (kind == RelayPacketKind::Created || kind == RelayPacketKind::Joined)
                            {
                                relayReady = true;
                                relayLobbyCode = lobbyCode;
                                relayParticipantId = participantId;
                                relaySessionToken = token;
                                packets.push_back({
                                    kind == RelayPacketKind::Created
                                        ? "@RELAY_CREATED|" + std::to_string(lobbyCode)
                                        : "@RELAY_JOINED|" + std::to_string(lobbyCode),
                                    "relay" });
                            }
                            else if (kind == RelayPacketKind::Data)
                            {
                                if (std::optional<std::string> message = AcceptRelayFragment(participantId, payload))
                                    packets.push_back({*message, "relay:" + std::to_string(participantId)});
                            }
                            else if (kind == RelayPacketKind::Error)
                                packets.push_back({"@RELAY_ERROR|" + std::to_string(requestId) + "|" + (payload.empty() ? "0" : std::to_string(static_cast<unsigned char>(payload[0]))), "relay"});
                            else if (kind == RelayPacketKind::LobbyClosed)
                                packets.push_back({"@RELAY_CLOSED", "relay"});
                        }
                    }
                }
                else
                {
                    const std::string key = AddressKey(datagram->addr, datagram->port);
                    RememberPeer(key, datagram->addr, datagram->port);
                    packets.push_back({ std::string(reinterpret_cast<const char *>(datagram->buf), static_cast<std::size_t>(datagram->buflen)), key });
                }
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
        m_lastNetworkError.clear();

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
        m_lastNetworkError.clear();

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

    bool NetworkSession::HostRelay(
        const std::string &_relayAddress,
        std::uint16_t _relayPort,
        const std::string &_playerName,
        const std::string &_lobbyScenePath,
        std::uint8_t _capacity)
    {
        Disconnect();
        m_lastNetworkError.clear();
        m_transport = NetworkTransport::Relay;
        m_playerName = SanitizeToken(_playerName.empty() ? "Host" : _playerName);
        if (!_lobbyScenePath.empty())
            m_lobbyScenePath = _lobbyScenePath;
        m_mode = NetworkMode::Host;
        m_phase = NetworkPhase::Start;
        m_localClientId = 1;
        m_nextClientId = 2;
        m_relayRequestId = 1;
        m_relayControlAttempts = 1;
        m_relayCapacity = std::clamp<std::uint8_t>(_capacity, 2, 64);
        m_players = {NetworkPlayer{ .id = 1, .name = m_playerName, .ready = true, .host = true, .connected = true }};

        if (!m_impl->Open(0) || !m_impl->ResolveServer(_relayAddress.empty() ? "127.0.0.1" : _relayAddress, _relayPort))
        {
            m_lastNetworkError = "Could not connect to relay";
            Disconnect();
            return false;
        }
        m_impl->relayTransport = true;
        m_impl->SendRelayControl(RelayPacketKind::Create, m_relayRequestId, 0, m_relayCapacity);
        Debug::Log("Creating relay lobby through %s:%u.", _relayAddress.c_str(), static_cast<unsigned int>(_relayPort));
        return true;
    }

    bool NetworkSession::JoinRelay(
        const std::string &_relayAddress,
        std::uint32_t _lobbyCode,
        std::uint16_t _relayPort,
        const std::string &_playerName,
        const std::string &_lobbyScenePath)
    {
        if (_lobbyCode < 100000 || _lobbyCode > 999999)
        {
            m_lastNetworkError = "Relay lobby code must be six digits";
            return false;
        }
        Disconnect();
        m_lastNetworkError.clear();
        m_transport = NetworkTransport::Relay;
        m_playerName = SanitizeToken(_playerName.empty() ? "Player" : _playerName);
        if (!_lobbyScenePath.empty())
            m_lobbyScenePath = _lobbyScenePath;
        m_mode = NetworkMode::Client;
        m_phase = NetworkPhase::Start;
        m_relayLobbyCode = _lobbyCode;
        m_relayRequestId = 1;
        m_relayControlAttempts = 1;

        if (!m_impl->Open(0) || !m_impl->ResolveServer(_relayAddress.empty() ? "127.0.0.1" : _relayAddress, _relayPort))
        {
            m_lastNetworkError = "Could not connect to relay";
            Disconnect();
            return false;
        }
        m_impl->relayTransport = true;
        m_impl->SendRelayControl(RelayPacketKind::Join, m_relayRequestId, _lobbyCode);
        Debug::Log("Joining relay lobby %06u through %s:%u.", _lobbyCode, _relayAddress.c_str(), static_cast<unsigned int>(_relayPort));
        return true;
    }

    void NetworkSession::Disconnect()
    {
        if (m_impl != nullptr && m_impl->socket != nullptr)
        {
            if (m_transport == NetworkTransport::Relay)
                m_impl->SendRelaySessionPacket(RelayPacketKind::Leave);
            else if (m_mode == NetworkMode::Client && m_localClientId != 0)
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
        m_relayControlTimer = 0.0f;
        m_relayKeepAliveTimer = 0.0f;
        m_relayLobbyCode = 0;
        m_relayRequestId = 0;
        m_relayControlAttempts = 0;
        m_transport = NetworkTransport::Direct;
    }

    void NetworkSession::Update(float _deltaTime)
    {
        m_timeSeconds += _deltaTime;

        if (m_impl != nullptr)
        {
            for (const auto &packet : m_impl->Poll())
                HandlePacket(packet.first, packet.second);
        }

        if (m_transport == NetworkTransport::Relay && m_impl != nullptr)
        {
            if (!m_impl->relayReady)
            {
                m_relayControlTimer += _deltaTime;
                if (m_relayControlTimer >= 0.5f)
                {
                    m_relayControlTimer = 0.0f;
                    if (m_relayControlAttempts >= 6u)
                    {
                        m_lastNetworkError = "Relay timed out";
                        Disconnect();
                        return;
                    }
                    ++m_relayControlAttempts;
                    m_impl->SendRelayControl(
                        m_mode == NetworkMode::Host ? RelayPacketKind::Create : RelayPacketKind::Join,
                        m_relayRequestId,
                        m_relayLobbyCode,
                        m_relayCapacity);
                }
                return;
            }
            m_relayKeepAliveTimer += _deltaTime;
            if (m_relayKeepAliveTimer >= 5.0f)
            {
                m_relayKeepAliveTimer = 0.0f;
                m_impl->SendRelaySessionPacket(RelayPacketKind::KeepAlive);
            }
        }

        if (m_transport == NetworkTransport::Direct && m_mode == NetworkMode::Client && m_localClientId == 0)
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

        std::string status = std::string(mode) + " / " + phase + " / players: " + std::to_string(m_players.size());
        if (m_transport == NetworkTransport::Relay)
            status += m_relayLobbyCode != 0 ? " / relay: " + std::to_string(m_relayLobbyCode) : " / relay: connecting";
        if (!m_lastNetworkError.empty())
            status += " / " + m_lastNetworkError;
        return status;
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
        if (_message.rfind("@RELAY_CREATED|", 0) == 0)
        {
            m_relayLobbyCode = ParseNumber<std::uint32_t>(_message.substr(15), 0);
            m_phase = NetworkPhase::Lobby;
            m_lastNetworkError.clear();
            Debug::Log("Relay lobby created with code %06u.", m_relayLobbyCode);
            LoadLobbyScene();
            return;
        }
        if (_message.rfind("@RELAY_JOINED|", 0) == 0)
        {
            m_relayLobbyCode = ParseNumber<std::uint32_t>(_message.substr(14), m_relayLobbyCode);
            m_lastNetworkError.clear();
            SendMessageToServer("HELLO|" + m_playerName + "|" + m_lobbyScenePath);
            return;
        }
        if (_message.rfind("@RELAY_ERROR|", 0) == 0)
        {
            const std::vector<std::string> errorParts = Split(_message, '|');
            const int errorCode = errorParts.size() >= 3u ? ParseNumber(errorParts[2], 0) : 0;
            const char *messages[] = {"Relay error", "Bad relay request", "Relay protocol mismatch", "Lobby not found", "Lobby full", "Relay authorization failed", "Relay rate limit reached", "Relay server full", "Relay packet too large", "Relay internal error"};
            m_lastNetworkError = messages[std::clamp(errorCode, 0, 9)];
            Debug::Warning("%s.", m_lastNetworkError.c_str());
            Disconnect();
            return;
        }
        if (_message == "@RELAY_CLOSED")
        {
            m_lastNetworkError = "Relay lobby closed";
            Disconnect();
            return;
        }

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
                        m_impl->SendToPeer(_peerKey, "REJECT|Lobby_full");
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

                m_impl->SendToPeer(_peerKey, "WELCOME|" + std::to_string(clientId) + "|" + m_lobbyScenePath);
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

        if (m_transport == NetworkTransport::Relay)
        {
            m_impl->SendRelayData(0, true, _message);
            return;
        }
        for (const auto &entry : m_impl->clientPeerKeys)
        {
            if (auto peerIt = m_impl->peers.find(entry.second); peerIt != m_impl->peers.end())
                m_impl->SendTo(peerIt->second, _message);
        }
    }

    void NetworkSession::SendMessageToServer(const std::string &_message)
    {
        if (m_impl == nullptr || !m_impl->IsOpen())
            return;
        if (m_transport == NetworkTransport::Relay)
            m_impl->SendRelayData(1, false, _message);
        else
            m_impl->SendTo(m_impl->serverEndpoint, _message);
    }
}
