//	Author: Kozlov S.A. since 2018


#include <fstream>
#include <sstream>
#include <iostream>
#include <list>
#include <unordered_set>

#include "MachineLeaningControl.h"

#include "thread.h"
#include "search.h"
#include "UCI.h"
#include "types.h"
#include "movegen.h"

MachineLearningControl MachineLearningControlMain;
void learning_go_call(Position& pos, std::istringstream& is, StateListPtr& states);
void learning_position_call(Position& pos, std::istringstream& is, StateListPtr& states);
void go(Position& pos, StateListPtr& states, Search::LimitsType limits, bool ponderMode);


MachineLearningControl::MachineLearningControl()
	:learning_in_progress(false), learning_move_returned(true), current_position_set(false), learning_round_finished(false), learning_exit(false),
	is_960(false), 
	states(NULL), learning_thread(&MachineLearningControl::learning_thread_function, this)
{
}


MachineLearningControl::~MachineLearningControl()
{
	learning_exit = true;

	learning_thread.join();
}

void MachineLearningControl::SetFileName(std::string parameter_file_name)
{
	file_name = parameter_file_name;
}


//	0 - success
//	1 - fail
int MachineLearningControl::LoadData()
{
	std::ifstream input_stream;
	
	input_stream.open(file_name);

	if (input_stream.is_open())
	{
		if (!input_stream.eof())
		{
			do
			{
				MachineLearningDataAtom MachineLearningDataAtomCurrent;

				input_stream >> MachineLearningDataAtomCurrent;

				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

			} while (!input_stream.eof());
		}
		else
		{
			return 1;
		}

		if (MachineLearningDataStore.size() > 0)
		{
			MachineLearningDataStore.pop_back();
		}
	}
	else
	{
		return 1;
	}

	return 0;
}


//	0 - success
//	1 - fail
//	2 - not required
int MachineLearningControl::SaveData()
{
	std::ofstream output_stream;
	
	output_stream.open(file_name, std::ios_base::app | std::ios_base::out);

	if (output_stream.is_open())
	{

		if (Options["Save Machine Learning File"])
		{
			MachineLearningDataListIterator MachineLearningDataListIteratorCurrent = MachineLearningDataStore.begin();

			if (MachineLearningDataListIteratorCurrent != MachineLearningDataStore.end())
			{
				do
				{
					MachineLearningDataAtom MachineLearningDataAtomCurrent;

					MachineLearningDataAtomCurrent = *MachineLearningDataListIteratorCurrent;

					output_stream << MachineLearningDataAtomCurrent;

					MachineLearningDataListIteratorCurrent++;

				} while (MachineLearningDataListIteratorCurrent != MachineLearningDataStore.end());
			}
			else
			{
				return 1;
			}
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}


void MachineLearningControl::ClearData()
{
	MachineLearningDataStore.clear();
}


void MachineLearningControl::StartLearning(Position &position_parameter, std::istringstream& is, StateListPtr& parameter_states, const Search::LimitsType& limits, bool ponderMode)
{
	learning_in_progress = true;

	learning_round_finished = false;

	PrepareLearning(position_parameter, is, parameter_states);

	current_position_set = true;

	game_simulation_limits = limits;
	game_simulation_ponderMode = ponderMode;

	final_game_limits = limits;

	int time_corrector = games_to_simulate * 2;
	//	"games_to_simulate" is to time for each simulated games and "games_to_simulate" is to time for best moves from database

	game_simulation_limits.time[0] /= time_corrector;
	game_simulation_limits.time[1] /= time_corrector;
	game_simulation_limits.inc[0] /= time_corrector;
	game_simulation_limits.inc[1] /= time_corrector;
	game_simulation_limits.movetime /= time_corrector;

	final_game_limits.time[0] /= 2;
	final_game_limits.time[1] /= 2;
	final_game_limits.inc[0] /= 2;
	final_game_limits.inc[1] /= 2;
	final_game_limits.movetime /= 2;
}


void MachineLearningControl::EndLearning()
{
	learning_in_progress = false;

	current_position_set = false;
}


void MachineLearningControl::Answer(Move parameter_Move, bool parameter_960)
{
	if (learning_in_progress)
	{
		MachineLearningDataAtom MachineLearningDataAtomCurrent;

		std::string parameter_answer = UCI::move(parameter_Move, parameter_960);

		Last_Move = parameter_Move;
		is_960 = parameter_960;

		MachineLearningDataAtomCurrent.SetData(parameter_answer);

		MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

		learning_move_returned = true;
	}
}

void MachineLearningControl::learning_thread_function()
{
	while (!learning_exit)
	{
		while (!current_position_set && !learning_exit)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		//Options["Threads"] = 1;							//	for easier debuging
		//Options["Save Machine Learning File"] = true;		//	for easier debuging

		ClearData();

		for (size_t game_number = 1; game_number <= games_to_simulate && !learning_exit; game_number++)
		{
			StateInfo st;
			std::memset(&st, 0, sizeof(StateInfo));

			learning_round_finished = false;

			std::string fen_saved = fen_saved_main;

			{
				MachineLearningDataAtom MachineLearningDataAtomCurrent;

				std::string local_string;

				local_string = std::string("");
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

				local_string = std::string("");
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

				local_string = std::string("[FEN \"") + fen_saved + std::string("\"]");
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

				char local_char_buffer[data_atom_maximum_size];
				memset(local_char_buffer, 0, data_atom_maximum_size * sizeof(char));
				const char local_char_buffer_mask[] = "[Round \"%d\"]";
				sprintf_s(local_char_buffer, data_atom_maximum_size, local_char_buffer_mask, game_number);
				local_string = local_char_buffer;
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

				local_string = std::string("[Result \"*\"]");
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);

				local_string = std::string("");
				MachineLearningDataAtomCurrent.SetData(local_string);
				MachineLearningDataStore.push_back(MachineLearningDataAtomCurrent);
			}

			while (learning_in_progress && !learning_round_finished && !learning_exit)
			{
				learning_move_returned = false;

				Search::LimitsType limits;

				bool ponderMode = false;

				limits.startTime = now();

				{
					std::string input_stream_data("fen ");
					input_stream_data += fen_saved;
					std::istringstream input_stream(input_stream_data);

					learning_position_call(current_position, input_stream, *states);
				}

				if (false)
				{
					sync_cout << current_position << sync_endl;
				}

				Color us = current_position.side_to_move();

				if (current_position.is_draw(32))
				{
					LearningRoundFinished();

					std::cout << "Game over: draw" << std::endl;

					{
						auto MachineLearningDataAtomIteratorCurrent = MachineLearningDataStore.rbegin();

						for (; MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend(); MachineLearningDataAtomIteratorCurrent++)
						{
							if (MachineLearningDataAtomIteratorCurrent->GetData() == std::string("[Result \"*\"]"))
							{
								break;
							}
						}

						if (MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend())
						{
							std::string game_result = std::string("1/2-1/2");
							std::string local_string = std::string("[Result \"") + game_result + std::string("\"]");
							MachineLearningDataAtomIteratorCurrent->SetData(local_string);
						}
					}

					continue;
				}

				auto moveList = MoveList<LEGAL>(current_position);

				if (moveList.size() == 0)
				{
					LearningRoundFinished();

					if (current_position.checkers())
					{
						if (us == WHITE)
						{
							std::cout << "Game over: black wins" << std::endl;

							{
								auto MachineLearningDataAtomIteratorCurrent = MachineLearningDataStore.rbegin();

								for (; MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend(); MachineLearningDataAtomIteratorCurrent++)
								{
									if (MachineLearningDataAtomIteratorCurrent->GetData() == std::string("[Result \"*\"]"))
									{
										break;
									}
								}

								if (MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend())
								{
									std::string game_result = std::string("0-1");
									std::string local_string = std::string("[Result \"") + game_result + std::string("\"]");
									MachineLearningDataAtomIteratorCurrent->SetData(local_string);
								}
							}
						}
						else
						{
							if (us == BLACK)
							{
								std::cout << "Game over: white wins" << std::endl;

								{
									auto MachineLearningDataAtomIteratorCurrent = MachineLearningDataStore.rbegin();

									for (; MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend(); MachineLearningDataAtomIteratorCurrent++)
									{
										if (MachineLearningDataAtomIteratorCurrent->GetData() == std::string("[Result \"*\"]"))
										{
											break;
										}
									}

									if (MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend())
									{
										std::string game_result = std::string("1-0");
										std::string local_string = std::string("[Result \"") + game_result + std::string("\"]");
										MachineLearningDataAtomIteratorCurrent->SetData(local_string);
									}
								}
							}
							else
							{
								assert(false);
							}
						}
					}
					else
					{
						std::cout << "Game over: draw" << std::endl;

						{
							auto MachineLearningDataAtomIteratorCurrent = MachineLearningDataStore.rbegin();

							for (; MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend(); MachineLearningDataAtomIteratorCurrent++)
							{
								if (MachineLearningDataAtomIteratorCurrent->GetData() == std::string("[Result \"*\"]"))
								{
									break;
								}
							}

							if (MachineLearningDataAtomIteratorCurrent != MachineLearningDataStore.rend())
							{
								std::string game_result = std::string("1/2-1/2");
								std::string local_string = std::string("[Result \"") + game_result + std::string("\"]");
								MachineLearningDataAtomIteratorCurrent->SetData(local_string);
							}
						}
					}

					continue;
				}

				if (!learning_exit)
				{
					go(current_position, *states, game_simulation_limits, game_simulation_ponderMode);
				}


				while (!learning_move_returned && !learning_exit)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}

				if (!is_ok(Last_Move) && !learning_exit)
				{
					LearningRoundFinished();

					std::cout << "Last move is not ok" << std::endl;

					continue;
				}

				if (current_position.legal(Last_Move) && !learning_exit)
				{
					current_position.do_move(Last_Move, st);

					fen_saved = current_position.fen();

					if (!current_position.pos_is_ok() && !learning_exit)
					{
						LearningRoundFinished();

						current_position.undo_move(Last_Move);

						std::cout << "Position is not ok" << std::endl;

						continue;
					}

					current_position.undo_move(Last_Move);

					if (type_of(Last_Move) == CASTLING)
					{
						if (current_position.side_to_move() == WHITE)
						{
							size_t symbol_position_w;
							size_t symbol_position;
							symbol_position_w = fen_saved.find(' ');
							assert(symbol_position_w != std::string::npos);

							symbol_position = fen_saved.find('K', symbol_position_w);
							if (symbol_position == std::string::npos)
							{
								symbol_position = fen_saved.find('Q', symbol_position_w);
							}

							if (symbol_position != std::string::npos)
							{
								if (fen_saved.at(symbol_position) == 'K')
								{
									fen_saved.erase(symbol_position, 1);
								}
							}

							symbol_position = fen_saved.find('Q', symbol_position_w);
							if (symbol_position != std::string::npos)
							{
								if (fen_saved.at(symbol_position) == 'Q')
								{
									fen_saved.erase(symbol_position, 1);
								}
							}
						}
						else
						{
							if (current_position.side_to_move() == BLACK)
							{
								size_t symbol_position_b;
								size_t symbol_position;
								symbol_position_b = fen_saved.find(' ');
								assert(symbol_position_b != std::string::npos);

								symbol_position = fen_saved.find('k', symbol_position_b);
								if (symbol_position == std::string::npos)
								{
									symbol_position = fen_saved.find('q', symbol_position_b);
								}

								if (symbol_position != std::string::npos)
								{
									if (fen_saved.at(symbol_position) == 'k')
									{
										fen_saved.erase(symbol_position, 1);
									}
								}

								symbol_position = fen_saved.find('q', symbol_position_b);
								if (symbol_position != std::string::npos)
								{
									if (fen_saved.at(symbol_position) == 'q')
									{
										fen_saved.erase(symbol_position, 1);
									}
								}
							}
							else
							{
								assert(false);
							}
						}
					}
				}
				else
				{
					std::cout << "Game over" << std::endl;
				}
			}

			if (SaveData() == 1)
			{
				assert(false);
			}
			else
			{
				ClearData();
			}


			if (!learning_exit)
			{
				std::string input_stream_data("startpos");
				std::istringstream input_stream(input_stream_data);
				learning_position_call(current_position, input_stream, *states);
			}

			if (!learning_exit)
			{
				std::string input_stream_data("fen ");
				input_stream_data += fen_saved_main;
				std::istringstream input_stream(input_stream_data);
				learning_position_call(current_position, input_stream, *states);
			}
		}

		if (!learning_exit)
		{
			std::string input_stream_data("startpos");
			std::istringstream input_stream(input_stream_data);
			learning_position_call(current_position, input_stream, *states);
		}

		if (!learning_exit)
		{
			std::string input_stream_data("fen ");
			input_stream_data += fen_saved_main;
			std::istringstream input_stream(input_stream_data);
			learning_position_call(current_position, input_stream, *states);
		}

		if (!learning_exit)
		{
			std::string input_stream_data("");
			std::istringstream input_stream(input_stream_data);

			PrepareLearning(current_position, input_stream, *states);
		}

		if (!learning_exit)
		{
			std::string input_stream_data;

			auto us = current_position.side_to_move();

			if (LoadData() == 0)
			{
				int results_table[4];
				memset(results_table, 0, 4 * sizeof(int));
				
				std::unordered_set<std::string> CurrentDataUsBestMoves;

				std::list<std::string> CurrentDataMoves1;
				std::list<std::string> CurrentDataMoves2;
				std::list<std::string> CurrentDataMoves3;
				std::list<std::string> CurrentDataMoves4;

				std::string CurrentDataFen = std::string("[FEN \"") + fen_saved_main + std::string("\"]");
				auto MachineLearningDataStoreIterator = MachineLearningDataStore.begin();
				for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
				{
					for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
					{
						if (MachineLearningDataStoreIterator->GetData() == CurrentDataFen)
						{
							break;
						}
					}

					if (MachineLearningDataStoreIterator != MachineLearningDataStore.end())
					{
						std::string CurrentDataResult1 = std::string("[Result \"*\"]");
						std::string CurrentDataResult2 = std::string("[Result \"1/2-1/2\"]");
						std::string CurrentDataResult3 = std::string("[Result \"1-0\"]");
						std::string CurrentDataResult4 = std::string("[Result \"0-1\"]");

						int local_current_result = 0;

						for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
						{
							if (MachineLearningDataStoreIterator->GetData() == CurrentDataResult1)
							{
								MachineLearningDataStoreIterator++;
								for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
								{
									if (MachineLearningDataStoreIterator->GetData() != std::string(""))
									{
										CurrentDataMoves1.push_back(MachineLearningDataStoreIterator->GetData());
										break;
									}
								}

								local_current_result = 1;
								break;
							}
							else
								if (MachineLearningDataStoreIterator->GetData() == CurrentDataResult2)
								{
									MachineLearningDataStoreIterator++;
									for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
									{
										if (MachineLearningDataStoreIterator->GetData() != std::string(""))
										{
											CurrentDataMoves2.push_back(MachineLearningDataStoreIterator->GetData());
											break;
										}
									}
									local_current_result = 2;
									break;
								}
								else
									if (MachineLearningDataStoreIterator->GetData() == CurrentDataResult3)
									{
										MachineLearningDataStoreIterator++;
										for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
										{
											if (MachineLearningDataStoreIterator->GetData() != std::string(""))
											{
												CurrentDataMoves3.push_back(MachineLearningDataStoreIterator->GetData());
											}
											else
											{
												break;
											}
										}
										local_current_result = 3;
										break;
									}
									else
										if (MachineLearningDataStoreIterator->GetData() == CurrentDataResult4)
										{
											MachineLearningDataStoreIterator++;
											for (; MachineLearningDataStoreIterator != MachineLearningDataStore.end(); MachineLearningDataStoreIterator++)
											{
												if (MachineLearningDataStoreIterator->GetData() != std::string(""))
												{
													CurrentDataMoves4.push_back(MachineLearningDataStoreIterator->GetData());
													break;
												}
											}
											local_current_result = 4;
											break;
										}
						}

						if (local_current_result == 0)
						{
							local_current_result = 1;
						}

						if (MachineLearningDataStoreIterator != MachineLearningDataStore.end())
						{
							results_table[local_current_result-1]++;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}

				double games_number = results_table[1] + results_table[2] + results_table[1];
				double white_score_part = 0.5*results_table[1] + results_table[2];
				double black_score_part = 0.5*results_table[1] + results_table[3];
				if (games_number != 0)
				{
					white_score_part /= games_number;		//	"white_score_part" is part of white scores
					black_score_part /= games_number;		//	"black_score_part" is part of black scores
				}

				if (us == WHITE)
				{
					for (auto local_move : CurrentDataMoves3)
					{
						CurrentDataUsBestMoves.insert(local_move);
					}
					for (auto local_move : CurrentDataMoves2)
					{
						CurrentDataUsBestMoves.insert(local_move);
					}
					for (auto local_move : CurrentDataMoves4)
					{
						CurrentDataUsBestMoves.insert(local_move);
					}
				}
				else
				{
					if (us = BLACK)
					{
						for (auto local_move : CurrentDataMoves4)
						{
							CurrentDataUsBestMoves.insert(local_move);
						}
						for (auto local_move : CurrentDataMoves2)
						{
							CurrentDataUsBestMoves.insert(local_move);
						}
						for (auto local_move : CurrentDataMoves3)
						{
							CurrentDataUsBestMoves.insert(local_move);
						}
					}
					else
					{
						assert(false);
					}
				}

				///*/
				if (CurrentDataUsBestMoves.size() > 0)
				{
					std::string local_string("searchmoves ");

					for (auto current_best_move : CurrentDataUsBestMoves)
					{
						local_string += current_best_move;
						local_string += std::string(" ");
					}

					input_stream_data += local_string;
				}
				else
				{
					std::string local_string("searchmoves ");

					auto moveList = MoveList<LEGAL>(current_position);

					for (auto current_best_move_from_list : moveList)
					{
						local_string += UCI::move(current_best_move_from_list, is_960);
						local_string += std::string(" ");
					}

					input_stream_data += local_string;
				}
				///*/

				ClearData();
			}
			{
				std::string input_stream_data_best_moves;
				if (input_stream_data.length() != 0)
				{
					input_stream_data_best_moves = input_stream_data;
				}
				input_stream_data = std::string();

				if (final_game_limits.time[0] != 0)// && us == WHITE)
				{
					char local_string[data_atom_maximum_size];
					memset(local_string, 0, data_atom_maximum_size * sizeof(char));
					sprintf_s(local_string, data_atom_maximum_size, "wtime %d ", final_game_limits.time[0]);
					input_stream_data += std::string(local_string);
				}
				if (final_game_limits.time[1] != 0)// && us == BLACK)
				{
					char local_string[data_atom_maximum_size];
					memset(local_string, 0, data_atom_maximum_size * sizeof(char));
					sprintf_s(local_string, data_atom_maximum_size, "btime %d ", final_game_limits.time[1]);
					input_stream_data += std::string(local_string);
				}
				if (final_game_limits.inc[0] != 0)// && us == WHITE)
				{
					char local_string[data_atom_maximum_size];
					memset(local_string, 0, data_atom_maximum_size * sizeof(char));
					sprintf_s(local_string, data_atom_maximum_size, "winc %d ", final_game_limits.inc[0]);
					input_stream_data += std::string(local_string);
				}
				if (final_game_limits.inc[1] != 0)// && us == BLACK)
				{
					char local_string[data_atom_maximum_size];
					memset(local_string, 0, data_atom_maximum_size * sizeof(char));
					sprintf_s(local_string, data_atom_maximum_size, "binc %d ", final_game_limits.inc[1]);
					input_stream_data += std::string(local_string);
				}
				if (final_game_limits.movetime != 0)
				{
					char local_string[data_atom_maximum_size];
					memset(local_string, 0, data_atom_maximum_size * sizeof(char));
					sprintf_s(local_string, data_atom_maximum_size, "movetime %d ", final_game_limits.movetime);
					input_stream_data += std::string(local_string);
				}

				if (input_stream_data_best_moves.length() != 0)
				{
					input_stream_data = std::string(" ") + input_stream_data_best_moves;
				}
			}

			std::istringstream input_stream(input_stream_data);

			learning_go_call(current_position, input_stream, *states);
		}

		current_position_set = false;
	}
}

void MachineLearningControl::LearningExit()
{
	learning_exit = true;
}

void MachineLearningControl::LearningRoundFinished()
{
	learning_round_finished = true;
}

void MachineLearningControl::PrepareLearning(Position &position_parameter, std::istringstream& is, StateListPtr& parameter_states)
{
	states = &parameter_states;

	if (states != NULL)
	{
		if (states->get() != NULL)
		{
			if (states->get()->size() == 0)
			{
				//assert(false);
				parameter_states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
			}
			else
			{
			}
		}
		else
		{
			//assert(false);
			parameter_states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
		}
	}
	else
	{
		assert(false);
	}

	states = &parameter_states;

	current_position.init();
	current_position.set(position_parameter.fen(), Options["UCI_Chess960"], &states->get()->back(), Threads.main());

	fen_saved_main = current_position.fen();
}

bool MachineLearningControl::IsLearningInProgress()
{
	return learning_in_progress;
}