#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <libTMCG.hh>

#define debug(a...) { fprintf(stderr, gettime().c_str()); fprintf(stderr, a); }

using namespace boost::asio;

std::vector<std::istream*> inputs;
std::vector<std::ostream*> outputs;
int num_players;
int player_id;
int base_port;

std::string gettime()
{
	return (boost::format("[%u] ") % time(NULL)).str();
}

void get_card(VTMF_Card &card, SchindelhauerTMCG *tmcg,
		BarnettSmartVTMF_dlog *vtmf)
{
	//debug("getting card\n");
	tmcg->TMCG_SelfCardSecret(card, vtmf);

	for(int i = 0; i < num_players; ++i) {
		if(i == player_id)
			continue;

		if(!tmcg->TMCG_VerifyCardSecret(card, vtmf, *inputs[i], *outputs[i]))
			debug("card verification failed\n");
	}
	debug("    got card %u\n", tmcg->TMCG_TypeOfCard(card, vtmf));
}

void wait_for(int who)
{
	io_service ios;
	ip::tcp::endpoint ep(ip::tcp::v4(), base_port + player_id);
	ip::tcp::acceptor acceptor(ios, ep);

	ip::tcp::iostream *stream = new ip::tcp::iostream;
	debug("waiting for connection from %d\n", who);
	acceptor.accept(*stream->rdbuf());
	debug("got connection from %d\n", who);
	std::string hello;
	std::getline(*stream, hello);
	if(hello != (boost::format("hello %d %d\r") % who % 
			num_players).str()) {
		debug("incorrect hello %s\n", hello.c_str());
		exit(1);
	}
	debug("confirmed connection from %d\n", who);
	inputs.push_back(stream);
}

void connect_to(int who)
{
	debug("connecting to %d\n", who);
	ip::tcp::iostream *stream = new ip::tcp::iostream("127.0.0.1",
			(boost::format("%d") % (base_port + who)).str());
	if(!*stream) {
		debug("couldn't connect, trying again\n");
		sleep(1);
		connect_to(who);
		return;
	}
	debug("sending hello %d %d\n", player_id, num_players);
	*stream << (boost::format("hello %d %d\r\n") % player_id %
			num_players).str();
	stream->flush();
	debug("appears connected\n");
	outputs.push_back(stream);
}

void initialize_connections()
{
	for(int i=0; i<num_players; ++i)
	{
		if(i < player_id) {
			connect_to(i);
			wait_for(i);
		} else if(i == player_id) {
			// create a dummy stream
			debug("creating dummy stream for myself\n");
			inputs.push_back(NULL);
			outputs.push_back(NULL);
		} else {
			wait_for(i);
			connect_to(i);
		}
	}
}

int main(int argc, char **argv)
{
	if(argc != 4) {
		debug("usage: %s <num_players> <player_id> <base_port>\n", argv[0]);
		return 1;
	}

	num_players = atoi(argv[1]);
	player_id = atoi(argv[2]);
	base_port = atoi(argv[3]);

	initialize_connections();

	if(!init_libTMCG())
		debug("init failed\n");

	SchindelhauerTMCG *tmcg = new SchindelhauerTMCG(64, num_players, 6);
	BarnettSmartVTMF_dlog *vtmf;

	if(player_id == 0) {
		vtmf = new BarnettSmartVTMF_dlog();

		if(!vtmf->CheckGroup())
			debug("checkgroup failed\n");

		debug("> publishing group:\n");
		for(int i = 1; i < num_players; ++i) {
			vtmf->PublishGroup(*outputs[i]);
		}
	} else {
		debug("> waiting for group\n");
		vtmf = new BarnettSmartVTMF_dlog(*inputs[0]);

		if(!vtmf->CheckGroup())
			debug("checkgroup failed\n");
	}
	debug("< done\n");

	vtmf->KeyGenerationProtocol_GenerateKey();

	debug("> publishing key\n");
	for(int i = 0; i < num_players; ++i) {
		if(i != player_id)
			vtmf->KeyGenerationProtocol_PublishKey(*outputs[i]);
	}
	debug("< done\n");

	debug("> give me keys\n");
	for(int i = 0; i < num_players; i++) {
		if(i != player_id) {
			debug("> waiting for key from %d\n", i);
			if(!vtmf->KeyGenerationProtocol_UpdateKey(*inputs[i]))
				debug("wrong key\n");
			debug("thanks\n");
		}
	}
	debug("< done\n");
	
	vtmf->KeyGenerationProtocol_Finalize();

	// create the deck
	TMCG_OpenStack<VTMF_Card> deck;
	for (size_t type = 0; type < 52; ++type)
	{
		VTMF_Card c;
		tmcg->TMCG_CreateOpenCard(c, vtmf, type);
		deck.push(type, c);
	}

	// shuffle the deck
	TMCG_Stack<VTMF_Card> s;
	s.push(deck);

	for(int i = 0; i < num_players; ++i)
	{
		TMCG_Stack<VTMF_Card> s2;
		if(i == player_id) {
			TMCG_StackSecret<VTMF_CardSecret> ss;
			tmcg->TMCG_CreateStackSecret(ss, false, s.size(), vtmf);
			tmcg->TMCG_MixStack(s, s2, ss, vtmf);
			for(int i2 = 0; i2 < num_players; ++i2) {
				if(i2 == player_id)
					continue;
				debug("> sending stack\n");
				*outputs[i2] << s2 << std::endl;
				debug("< done\n");
				debug("> proving stack\n");
				tmcg->TMCG_ProveStackEquality(s, s2, ss, false, vtmf,
					*inputs[i2], *outputs[i2]);
				debug("< done\n");
			}
		} else {
			debug("> give me stack\n");
			*inputs[i] >> s2;
			debug("thanks\n");
			if(!inputs[i]->good())
				debug("read or parse error\n");
			debug("> verifying stack\n");
			if (!tmcg->TMCG_VerifyStackEquality(s, s2, false, vtmf, 
						*inputs[i], *outputs[i]))
				debug("bad stack\n");
			debug("done\n");
		}
		s = s2;
	}
	
	// get cards
	TMCG_Stack<VTMF_Card> cards;
	VTMF_Card c;

	// push 2 cards for each player and then 5 for the board
	debug("pushing cards\n");
	for(int i = 0; i < 2 * num_players + 5; ++i) {
		s.pop(c);
		cards.push(c);
	}

	debug("giving hands\n");
	for(int i = 0; i < num_players; ++i) {
		if(i == player_id) {
			debug("  getting my hand\n");
			get_card(cards[i*2], tmcg, vtmf);
			get_card(cards[i*2 + 1], tmcg, vtmf);
		} else {
			tmcg->TMCG_ProveCardSecret(cards[i*2], vtmf, *inputs[i], 
					*outputs[i]);
			tmcg->TMCG_ProveCardSecret(cards[i*2+1], vtmf, *inputs[i], 
					*outputs[i]);
		}
	}

	debug("giving/getting board\n");
	for(int i = 0; i < num_players; ++i) {
		for(int j = 0; j < 5; ++j) {
			int card_id = 2 * num_players + j;
			if(i == player_id) {
				get_card(cards[card_id], tmcg, vtmf);
			} else {
				tmcg->TMCG_ProveCardSecret(cards[card_id], vtmf, 
						*inputs[i], *outputs[i]);
			}
		}
	}

	debug("revealing all hands\n");
	for(int i = 0; i < num_players; ++i) {
		for(int j = 0; j < num_players; ++j) {
			if(i == player_id) {
				if(j == player_id)
					continue;
				debug("  getting player %d's hand\n", j);
				get_card(cards[j*2], tmcg, vtmf);
				get_card(cards[j*2 + 1], tmcg, vtmf);
			} else {
				if(j == i)
					continue;
				debug("  revealing player %d's hand to player %d\n", j, i);
				tmcg->TMCG_ProveCardSecret(cards[j*2], vtmf, *inputs[i], 
						*outputs[i]);
				tmcg->TMCG_ProveCardSecret(cards[j*2+1], vtmf, *inputs[i], 
						*outputs[i]);
			}
		}
	}

	return 0;
}
