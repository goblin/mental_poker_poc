#include <stdio.h>
#include <iostream>
#include <libTMCG.hh>

#define debug(a...) fprintf(stderr, a)

void get_card(VTMF_Card &card, SchindelhauerTMCG *tmcg,
		BarnettSmartVTMF_dlog *vtmf)
{
	debug("getting card\n");
	tmcg->TMCG_SelfCardSecret(card, vtmf);
	if(!tmcg->TMCG_VerifyCardSecret(card, vtmf, std::cin, std::cout))
		debug("card verification failed\n");
	debug("my card is %u\n", tmcg->TMCG_TypeOfCard(card, vtmf));
}

int main()
{
	if(!init_libTMCG())
		debug("init failed\n");

	SchindelhauerTMCG *tmcg = new SchindelhauerTMCG(64, 2, 6);

	debug("> give me group\n");
	BarnettSmartVTMF_dlog *vtmf = new BarnettSmartVTMF_dlog(std::cin);
	debug("thanks\n");

	if(!vtmf->CheckGroup())
		debug("invalid group\n");

	vtmf->KeyGenerationProtocol_GenerateKey();
	
	debug("> give me key\n");
	if(!vtmf->KeyGenerationProtocol_UpdateKey(std::cin))
		debug("wrong key\n");
	debug("thanks\n");

	debug("> publishing key\n");
	vtmf->KeyGenerationProtocol_PublishKey(std::cout);
	debug("< done\n");

	vtmf->KeyGenerationProtocol_Finalize();
	
	// create the deck
	TMCG_OpenStack<VTMF_Card> deck;
	for (size_t type = 0; type < 52; type++)
	{
		VTMF_Card c;
		tmcg->TMCG_CreateOpenCard(c, vtmf, type);
		deck.push(type, c);
	}
	
	// shuffle the deck
	TMCG_Stack<VTMF_Card> s;
	s.push(deck);

	// verify a shuffle
	{
		TMCG_Stack<VTMF_Card> s2;
		debug("> give me stack\n");
		std::cin >> s2;
		debug("thanks\n");
		if(!std::cin.good())
			debug("read or parse error\n");
		debug("> verifying stack\n");
		if (!tmcg->TMCG_VerifyStackEquality(s, s2, false, vtmf,
			std::cin, std::cout))
				std::cerr << "Verification failed!" << std::endl;
		debug("done\n");
		s = s2;
	}
	
	// send the shuffle
	{
		TMCG_Stack<VTMF_Card> s2;
		TMCG_StackSecret<VTMF_CardSecret> ss;
		tmcg->TMCG_CreateStackSecret(ss, false, s.size(), vtmf);
		tmcg->TMCG_MixStack(s, s2, ss, vtmf);
		debug("> sending stack\n");
		std::cout << s2 << std::endl;
		debug("< done\n> proving stack\n");
		tmcg->TMCG_ProveStackEquality(s, s2, ss, false, vtmf,
			std::cin, std::cout);
		debug("< done\n");
		s = s2;
	}
	
	// get cards
	TMCG_Stack<VTMF_Card> my_hand;
	TMCG_Stack<VTMF_Card> his_hand;
	TMCG_Stack<VTMF_Card> board;
	VTMF_Card c;

	// first, two for him
	s.pop(c);
	his_hand.push(c);
	s.pop(c);
	his_hand.push(c);
	
	// then, two for me
	s.pop(c);
	my_hand.push(c);
	s.pop(c);
	my_hand.push(c);

	// finally, 5 for the table
	s.pop(c);
	board.push(c);
	s.pop(c);
	board.push(c);
	s.pop(c);
	board.push(c);
	s.pop(c);
	board.push(c);
	s.pop(c);
	board.push(c);

	// give him his cards
	tmcg->TMCG_ProveCardSecret(his_hand[0], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(his_hand[1], vtmf, std::cin, std::cout);
	
	// get my cards
	get_card(my_hand[0], tmcg, vtmf);
	get_card(my_hand[1], tmcg, vtmf);
	
	// give him board cards
	tmcg->TMCG_ProveCardSecret(board[0], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(board[1], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(board[2], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(board[3], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(board[4], vtmf, std::cin, std::cout);

	// get the board cards
	get_card(board[0], tmcg, vtmf);
	get_card(board[1], tmcg, vtmf);
	get_card(board[2], tmcg, vtmf);
	get_card(board[3], tmcg, vtmf);
	get_card(board[4], tmcg, vtmf);
	
	// give him my cards
	tmcg->TMCG_ProveCardSecret(my_hand[0], vtmf, std::cin, std::cout);
	tmcg->TMCG_ProveCardSecret(my_hand[1], vtmf, std::cin, std::cout);
	
	// get his cards
	get_card(his_hand[0], tmcg, vtmf);
	get_card(his_hand[1], tmcg, vtmf);
	
	return 0;
}
