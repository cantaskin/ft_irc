#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Utils.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

void Server::processCommand(const std::string &command, std::pair<const std::string &,const std::string &> params, Client &client)
{
    if (client._status == RegistrationState::UsernameRegistered)
    {
        switch (cmds[command])
        {
        case 0:
            Pass(client, params.first);
            break;
        case 1:
            Nick(client, params.first);
            break;
        case 2:
            if (params.second.empty())
                User(client, params.first);
            else
                User(client, params.first, params.second);
            break;
        case 3:
            Quit(client, params.first);
            break;
        case 4:
            if (params.second.empty())
                Join(client, params.first, params.second);
            else
                Join(client, params.first);
            break;
        case 5:
            Part(client, params.first, params.second);
            break;
        case 6:
            Topic(client, params.first,params.second);
            break;
        case 7:
            Names(params.first);
            break;
        case 8:
            Invite(client,params.first,params.second);
            break;
        case 9:
            Mode(client, params.first,params.second);
            break;
        case 10:
            Notice(params.first,params.second);
            break;
        case 11:
            PrivMsg(params.first,params.second);
            break;
        case 12:
            if(params.first.empty())
                List();
            else
                List(params.first);
            break;
        }
    }
    else
    {
        switch (cmds[command])
        {
        case 0:
            Pass(client, params.first);
            break;
        case 1:
            Nick(client, params.first);
            break;
        case 2:
            if (params.second.empty())
                User(client, params.first);
            else
                User(client, params.first, params.second);
            break;
        default:
            break;
        }
    }
}

void Server::Run()
{
    while (true)
    {
        fd_set readSet;
        FD_ZERO(&readSet);

        int maxSocket = _serverSocketFd;

        // Add server socket to the set
        FD_SET(_serverSocketFd, &readSet);

        // Add client sockets to the set
        for (std::vector<Client>::iterator &client = _clients.begin(); client != _clients.end(); client++;)
        {
            FD_SET(client->getSocketFd(), &readSet);
            maxSocket = std::max(maxSocket, client->getSocketFd());
        }

        // Use select to wait for activity on sockets
        if (select(maxSocket + 1, &readSet, nullptr, nullptr, nullptr) == -1)
        {
            std::cerr << "Failed to select socket activity.\n";
            continue;
        }

        // Check if the server socket has activity (new client connection)
        if (FD_ISSET(_serverSocketFd, &readSet))
        {
            sockaddr_in clientAddress;
            socklen_t clientAddressLength = sizeof(clientAddress);

            // Accept a new connection
            int clientSocket = accept(_serverSocketFd, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddressLength);
            if (clientSocket == -1)
            {
                std::cerr << "Failed to accept client connection.\n";
                continue;
            }

            // Handle the client connection
            std::cout << "New client connected. Socket descriptor: " << clientSocket << "\n";

            // Set client socket to non-blocking mode
            int flags = fcntl(clientSocket, F_GETFL, 0);
            fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

            // Add client socket to the vector
            _clients.push_back(Client(clientSocket)); //{clientSocket, "", "", false, ConnectionState::None});
        }

        // Check client sockets for activity
        for (std::vector<Client>::iterator it = _clients.begin(); it != _clients.end(); it++)
        {
            int clientSocket = it->getSocketFd();

            if (FD_ISSET(clientSocket, &readSet))
            {
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, sizeof(buffer));

                // Read data from the client socket
                int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead == -1)
                {
                    std::cerr << "Failed to read from client socket.\n";
                    continue;
                }

                if (bytesRead == 0)
                {
                    // Connection closed by the client
                    std::cout << "Client disconnected. Socket descriptor: " << clientSocket << "\n";

                    // Remove client from the vector
                    it = _clients.erase(it);
                    close(clientSocket);
                    continue;
                }

                // Process received data and handle IRC commands
                std::string message(buffer, bytesRead);
                std::cout << "Received data from client: " << message << "\n";

                // Check if the message starts with a command character
                if (message[0] == '/')
                {
                    // Extract command and parameters
                    size_t spacePos = message.find(' ');
                    std::string command = message.substr(1, spacePos - 1);
                    std::string params = message.substr(spacePos + 1);

                    // Process the IRC command
                    processCommand(command, params, *it);
                }
                else
                {
                    // Regular chat message
                    processChatMessage(message, *it);
                }
            }
        }
    }
}
bool Server::ReadySocketandPort()
{
    _serverSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocketFd == -1)
    {
        std::cerr << "Failed to create socket.\n";
        return true;
    }

    int reuse = 1;
    if (setsockopt(_serverSocketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        std::cerr << "Failed to set socket options.\n";
        close(_serverSocketFd);
        return true;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(_port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    if (bind(_serverSocketFd, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        std::cerr << "Failed to bind socket to port " << _port << ".\n";
        close(_serverSocketFd);
        return true;
    }

    // Listen for incoming connections
    if (listen(_serverSocketFd, MAX_CLIENTS) == -1)
    {
        std::cerr << "Failed to listen on socket.\n";
        close(_serverSocketFd);
        return true;
    }

    std::cout << "IRC server listening on port " << _port << "...\n";
}
Server::Server(const std::string &Port, const std::string &Password)
{
    if (Port.empty())
    {
        _port = 6667;
        std::cout << "Port is not given so your server initialized with default port 6667\n";
    }
    else
    {
        _port = std::atoi(Port.c_str());
        if (_port < 1024 || _port > 49151)
            throw InvalidPortException();
    }
    if (Password.empty())
    {
    }
    else
        _password = Password;
}

Server::~Server()
{
}

void Server::Pass(Client &obj, const std::string &Password)
{
    if (!InvalidPassword(Password))
    {
        switch (obj._status) // _status
        {
case RegistrationState::None:
            obj.setPassword(Password);
            obj._status = RegistrationState::PassRegistered;
            // password assigned yazdır?;
            break;
case RegistrationState::UsernameRegistered:
            obj.setPassword(Password);
            // password changed yazdır?;
            break;
        }
    }
    // Invalid Password hatası
}
void Server::Nick(Client &obj, const std::string &NickName)
{
    if (InvalidLetter(NickName) || InvalidPrefix(NickName))
        // hata durumu;
        ;
    else if (IsExistClient(NickName, 0))
        // hata durumu
        ;
    switch (obj._status) // _status
    {
    case RegistrationState::PassRegistered:
        obj._nick = ToLowercase(NickName);
        obj._status = RegistrationState::NickRegistered;
        // nick assigned yazdır?;
        break;
    default:
        obj._nick = ToLowercase(NickName);
        // nick changed yazdır?;
        break;
    }
}

void Server::User(Client &obj, const std::string &Username)
{
    if (InvalidLetter(Username) || InvalidPrefix(Username))
        // hata durumu;
        ;
    switch (obj._status) // _status
    {
    case RegistrationState::NickRegistered:
        obj._username = ToLowercase(Username);
        obj._realname = "";
        obj._status = RegistrationState::UsernameRegistered;
        // username assigned yazdır?;
        break;
    case RegistrationState::UsernameRegistered:
        obj._username = ToLowercase(Username);
        // username changed yazdır?;
        break;
    }
}

void Server::User(Client &obj, const std::string &Username, const std::string &Realname)
{
    if (InvalidLetter(Username) || InvalidPrefix(Username) || InvalidLetter(Realname) || InvalidPrefix(Realname))
        // hata durumu;
        ;
    switch (obj._status) // _status
    {
    case RegistrationState::NickRegistered:
        obj._username = Username;
        obj._realname = Realname;
        obj._status = RegistrationState::UsernameRegistered;
        // username assigned yazdır?;
        break;
    case RegistrationState::UsernameRegistered:
        obj._username = Username;
        obj._realname = Realname;
        // username and realname changed yazdır?;
        break;
    }
}

void Server::Ping(Client &obj)
{
    sendMessageToClient(obj, "Ping");
}
void Server::Quit(Client &obj, const std::string &Message)
{
    // print client nickname quited msg
    // remove client all the members list of channels joined
    // remove from clients.
    close(obj.getSocketFd());
}
void Server::Join(Client &obj, const std::string &ChannelName)
{
    if (IsExistChannel(ChannelName))
    {
        if (IsInChannel(obj, ChannelName))
        {
            // print client already in channel msg;
        }

        else if (IsBannedClient(obj, ChannelName))
        {
            // print client cannot join bcs in banned list of channel msg;
        }
        else if (IsChannelLimitFull(ChannelName))
        {
            // Channel is full print error
        }
        else if (HasChannelKey(ChannelName))
        {
            // if has key key error
        }
        else
        {
            _channels[ChannelName].addMembers(obj);
        }
    }
    else
    {
        if (InvalidLetter(ChannelName) || InvalidPrefix(ChannelName))
        {
            // print invalid ChannelName error
        }
        else
        {
            Channel newish(ChannelName);
            _channels.insert(std::pair<std::string, Channel>(ChannelName, newish));
        }
    }
}
void Server::Join(Client &obj, const std::string &ChannelName, const std::string &ChannelKey)
{
    if (IsExistChannel(ChannelName))
    {
        if (IsInChannel(obj, ChannelName))
        {
            // print client already in channel msg;
        }

        else if (IsBannedClient(obj, ChannelName))
        {
            // print client cannot join bcs in banned list of channel msg;
        }
        else if (IsChannelLimitFull(ChannelName))
        {
            // Channel is full
        }
        else if (IsKeyWrong(ChannelName, ChannelKey))
        {
            // if key wrong print error
        }
        else
        {
            _channels[ChannelName].addMembers(obj);
        }
    }
    else
    {
        if (InvalidLetter(ChannelName) || InvalidPrefix)
        {
            // print invalid ChannelName error
        }
        else
        {
            Channel newish(ChannelName);
            _channels.insert(std::pair<std::string, Channel>(ChannelName, newish));
            _channels[ChannelName].setKey(ChannelKey);
        }
    }
}
void Server::Part(Client &obj, const std::string &ChannelName, const std::string &Reason)
{
    // print reason
    _channels[ChannelName].removeMembers(obj);
}
void Server::Topic(Client &obj, const std::string &ChannelName, const std::string &TopicName)
{
    if (InvalidPrefix(TopicName) || InvalidLetter(TopicName))
        // print error doesnt allow this letter
        if (IsExistChannel(ChannelName) && IsInChannel(obj, ChannelName))
        {
            if (_channels[ChannelName]._mode == Mode::ProtectedTopic && _channels[ChannelName].getOperator()->_nick == obj._nick)
                _channels[ChannelName]._topic = TopicName;
            else if (_channels[ChannelName]._mode == Mode::ProtectedTopic)
            {
                // print error
            }
            else
                _channels[ChannelName]._topic = TopicName;
            ;
        }
}
void Server::Names(const std::string &ChannelName)
{
    if (IsExistChannel(ChannelName))
    {
        std::cout << '@' << _channels[ChannelName].getOperator()->_nick << '\n';
        for (std::vector<Client>::iterator it = _channels[ChannelName].getMembers().begin(); it != _channels[ChannelName].getMembers().end(); ++it)
        {
            std::cout << it->_nick << "\n";
        }
    }
}
void Server::Invite(Client &obj, const std::string &ChannelName, const std::string &NickName)
{
    if (!IsExistClient(NickName, 0))
        // print error
        // check there is a man with this nick in clients
        if (IsExistChannel(ChannelName) && IsInChannel(obj, ChannelName))
        {
            if (_channels[ChannelName]._mode == Mode::InviteOnly && _channels[ChannelName].getOperator()->_nick == obj._nick)
                _channels[ChannelName].addMembers(findClient(NickName));
            else if (_channels[ChannelName]._mode == Mode::InviteOnly)
            {
                // print error
            }
            else
                _channels[ChannelName].addMembers(findClient(NickName));
        }
}
void Server::Mode(Client &obj, const std::string &ChannelName, const std::string &ModeString)
{
    if (IsExistChannel(ChannelName) && _channels[ChannelName].getOperator()->_nick == obj._nick)
    {
        if (ModeString.size() == 2)
        {
            ChangeMode(_channels[ChannelName]._mode, ModeString);
        }
        else
            ; // print mode char error
    }
    else if (IsExistChannel(ChannelName))
    {
        ; // print not operator msg.
    }
    else
        ; // print channel does not exist msg.
}

void Server::Notice(const std::string &ChannelName, const std::string &TargetNick)
{
}
void Server::PrivMsg(const std::string &NickName, const std::string &Message)
{
    if (IsExistChannel(NickName))
    {
        //_channels[NickName].getOperator()->sendmessage(Message);
    }
    else if (IsExistClient(NickName, 0))
    {
        // findClient(NickName).sendmessage(Message);
    }
}
void Server::List()
{
    for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); it++)
    {
        std::cout << "channelname: " << it->first << "limit: " << it->second._clientLimit << ", operator: @" << it->second.getOperator()->_nick << ", topic: " << it->second._topic << "\n";
    }
}
void Server::List(const std::string &ChannelName)
{
    if (IsExistChannel(ChannelName))
    {
        std::cout << "channelname: " << ChannelName << "limit: " << _channels[ChannelName]._clientLimit << ", operator: @" << _channels[ChannelName].getOperator()->_nick << ", topic: " << _channels[ChannelName]._topic << "\n";
    }
    else
        ; // print channel not exist
}

bool Server::IsExistClient(const std::string &ClientName, const int val)
{
    for (std::vector<Client>::iterator it = _clients.begin(); it != _clients.end(); it++)
    {
        if (!val)
        {
            if (it->_nick == ClientName)
                return true;
        }
        else if (it->_username == ClientName)
            return true;
    }
    return false;
}
bool Server::IsExistChannel(const std::string &ChannelName)
{
    std::map<std::string, class Channel>::iterator it = _channels.find(ChannelName);
    return (it != _channels.end()) ? true : false;
}

bool Server::IsBannedClient(Client &obj, const std::string &ChannelName) // channela taşınabilir bu method
{
    std::vector<Client> &BannedVec = _channels[ChannelName].getBanned();
    for (std::vector<Client>::iterator it = BannedVec.begin(); it != BannedVec.end(); it++)
    {
        if (it->_nick == obj._nick)
            return false;
    }
    return true;
}

bool Server::IsInChannel(Client &obj, const std::string &ChannelName) // channela taşınabilir bu method
{
    std::vector<Client> &MemberVec = _channels[ChannelName].getMembers();
    for (std::vector<Client>::iterator it = MemberVec.begin(); it != MemberVec.end(); it++)
    {
        if (it->_nick == obj._nick)
            return false;
    }
    return true;
}

bool Server::HasChannelKey(const std::string &ChannelName)
{
    std::string NewKey = _channels[ChannelName].getKey();
    return !NewKey.empty();
}

bool Server::IsKeyWrong(const std::string &ChannelName, const std::string &Key)
{
    if (HasChannelKey(ChannelName))
    {
        if (_channels[ChannelName].getKey() == Key)
            return false;
        return true;
    }
    return true;
}

bool Server::IsChannelLimitFull(const std::string &ChannelName)
{
    return _channels[ChannelName].getMembers().size() == _channels[ChannelName]._clientLimit;
}

Client &Server::findClient(const std::string &NickName)
{
    for (std::vector<Client>::iterator it = _clients.begin(); it != _clients.end(); it++)
    {
        if (it->_nick == NickName)
            return *it;
    }
}

void Server::ChangeMode(enum Mode &mode, const std::string &ModeString)
{
    if (ValidModeChars(ModeString))
    {
        std::map<char, enum Mode> modes = {std::make_pair('i', InviteOnly), std::make_pair('k', KeyChannel), std::make_pair('t', ProtectedTopic)};
        if (ModeString[0] == '+')
            mode = static_cast<enum Mode>((mode | modes[ModeString[1]]));
        else if (ModeString[0] == '-')
            mode = static_cast<enum Mode>((mode ^ modes[ModeString[1]]));
    }
    else
        ; // print not valid
}