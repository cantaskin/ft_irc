#include "../../inc/Server.hpp"
#include "../../inc/Client.hpp"
#include "../../inc/Channel.hpp"
#include "../../inc/Replies.hpp"
#include "../../inc/Utils.hpp"

//A list of users currently joined to the channel (with one or more RPL_NAMREPLY (353) numerics followed by a single RPL_ENDOFNAMES (366) numeric). 
//These RPL_NAMREPLY messages sent by the server MUST include the requesting client that has just joined the channel.
//RPL_TOPIC (332)

//ERR_NEEDMOREPARAMS (461)
//ERR_NOSUCHCHANNEL (403)
//ERR_BADCHANNELKEY (475)
//ERR_BANNEDFROMCHAN (474)
//ERR_CHANNELISFULL (471)
//ERR_INVITEONLYCHAN (473)
//ERR_USERONCHANNEL (443)
void Server::Join(Client &client,std::vector<const std::string &> params)
{
    if (int err = ParamsSizeControl(params, 1, 1) != 0)
    {
        if (err == -1)
            sendServerToClient(client, ERR_NEEDMOREPARAMS(std::string("/JOIN")));
        else if (err == 1)
            sendServerToClient(client, ERR_CUSTOM(std::string("/JOIN Excessive argument is given")));
        return;
    }
    if (IsExistChannel(params[0]))
    {
        if (IsInChannel(client, params[0]))
            sendServerToClient(client, ERR_USERONCHANNEL(client._nick, params[0]));
        else if (IsBannedClient(client, params[0]))
            sendServerToClient(client,ERR_BANNEDFROMCHAN(params[0]));
        else if (IsChannelLimitFull(params[0]))
            sendServerToClient(client,ERR_CHANNELISFULL(params[0]));
        else if (IsKeyWrong(params[0], params[1]))
            sendServerToClient(client,ERR_BADCHANNELKEY(params[0]));
        else
        {
            _channels.at(params[0]).addMembers(client);
            sendServerToClient(client, std::string("Topic: " + _channels.at(params[0])._topic));
            sendServerToChannel(params[0], std::string(client._nick + " has joined."));
        }
    }
    else
    {
        if (InvalidLetter(params[0]) || InvalidPrefix(params[0]) || params[0][0] != '#')
            sendServerToClient(client, ERR_CUSTOM(std::string("Forbidden letter in use as Channel name or didn't use #.\n")));
        else
        {
            Channel newish(params[0], client);
            _channels.insert(std::pair<std::string, Channel>(params[0], newish));
            if(!params[1].empty())
                _channels.at(params[0]).setKey(params[1]);
        }
    }
}