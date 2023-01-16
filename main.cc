#include <chrono>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <sstream>
#include <unistd.h>
#include "pokedex_ascii.h"
#include "map.h"
#include <cassert>
using namespace std;
using hrc = std::chrono::high_resolution_clock;

/*======================
  Gehret and Maloney
  ======================*/

const bool DEBUG = true;

const unsigned int TIMEOUT = 10; //Milliseconds to wait for a getch to finish
const int UP = 65; //Key code for up arrow
const int DOWN = 66;
const int LEFT = 68;
const int RIGHT = 67;

string uppercaseify(string s) {
	string str;
	for (char c : s) {
		c = toupper(c);
		str += c;
	}
	return str;
}

//This class holds a record for each move in the game
class Move {
	public:
		int index;
		string name;
		string type;
		string category;
		int PP;
		int power;
		int accuracy;
};

//Holds a record for a single species of Pokemon, such as Pikachu
class Pokemon {
	public:
		int index;			//Index number in the pokedex
		string name;		//Name of the pokemon
		int hp;				//Health points
		int original_hp;
		int attack;			//this number gets multiplied by the move's power to estimate damage
		int defense;		//incoming damage gets divided by this number
		int speed;			//whichever pokemon has the highest speed attacks first, flip a coin on a tie
		int special;		//like attack and defense both, but for special moves
		string type1;
		string type2;
		vector<Move> moves;
};

ostream& operator<<(ostream &outs, const Pokemon &p) {
	outs << p.index << "\t" << p.name << "\tHP: " << p.hp << "\tAttack: " << p.attack << "\tDefense: " << p.defense << "\tSpeed: " << p.speed << "\tSpecial: " << p.special << "\tType 1: " << p.type1 << "\tType2: " << p.type2;
	return outs;
}

ostream& operator<<(ostream &outs, const Move &m) {
	outs << m.name << "\tType: " << m.type << "\tCategory: " << m.category << "\tPP: " << m.PP << "\tPower: " << m.power << "\tAccuracy: " << m.accuracy;
	return outs;
}


bool operator==(const Pokemon &p, const string &s) {
	return uppercaseify(s)== uppercaseify(p.name);
}

bool operator==(const Pokemon &p, const int &i) {
	return i == p.index;
}

bool operator==(const Move &m, const string &s) {
	return uppercaseify(s)== uppercaseify(m.name);
}

bool operator==(const Move &m, const int &i) {
	return i == m.index;
}

vector<Pokemon> pokemon_db; //Holds all pokemon known to mankind
vector<Pokemon> water_pokemon_db;
vector<Move> move_db;	//Holds all moves available to pokemon

void turn_on_ncurses() {
	//	srand(time(0));
	initscr();//Start curses mode
	start_color(); //Enable Colors if possible
	init_pair(1, COLOR_GREEN, COLOR_BLACK); //Set up some color pairs
	init_pair(2, COLOR_CYAN, COLOR_BLUE);
	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_YELLOW, COLOR_BLACK);
	init_pair(5, COLOR_GREEN, COLOR_BLACK);
	init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
	clear();
	noecho();
	cbreak();
	timeout(TIMEOUT); //Set a max delay for key entry
}

void turn_off_ncurses() {
	//Exit NCURSES
	clear();
	endwin(); // End curses mode
	system("clear");
}

void die(string s = "INVALID INPUT!") {
	cout << s << endl;
	exit(EXIT_FAILURE);
}

bool string_search(const string &needle, const string &haystack) {
	size_t loc = haystack.find(needle);
	if (loc != string::npos) return true;
	return false;
}

//This function reads from the "pokemon.txt" file in the directory and loads the data into the pokemon_db global vector
//Each row has: pokemon number, name, hp, etc.
//void load_pokemon_db(string filename = "pokemon.txt", string filename2 = "types.txt") {
void load_pokemon_db(string filename, string filename2) {
	if constexpr(DEBUG) cout << "Loading pokemon database" << endl;
	ifstream ins(filename);
	if (!ins) die("Couldn't load file "s + filename);
	ifstream ins2(filename2);
	if (!ins2) die("Couldn't load file"s + filename2);
	while (ins and ins2) {
		//Read from the files the data into a temporary Pokemon, getting the stats from pokemon.txt and its types from types.txt
		Pokemon temp;

		//load stats
		vector<string> temp_stats;
		string line;
		getline(ins, line);		//take in data from pokemon.txt as a string, line by line
		if (!ins) break;
		istringstream iss(line);
		string token;
		while (getline(iss, token, '\t')) 		//parse the string at tabs, leaving spaces in the string
			temp_stats.push_back(token);		//push each piece back into a temporary vector
		//fill each class member variable with data from the temporary vector
		temp.index = stoi(temp_stats.at(0));
		temp.name = temp_stats.at(1);
		temp.hp = stoi(temp_stats.at(2));
		temp.original_hp = stoi(temp_stats.at(2));
		temp.attack = stoi(temp_stats.at(3));
		temp.defense = stoi(temp_stats.at(4));
		temp.speed = stoi(temp_stats.at(5));
		temp.special = stoi(temp_stats.at(6));

		//load types
		vector<string> temp_types;
		string line2;
		getline(ins2, line);
		if (!ins2) break;
		istringstream iss2(line);
		while (getline(iss2, token, '\t'))
			temp_types.push_back(token);
		temp.type1 = temp_types.at(2);
		if (temp_types.size() > 3) temp.type2 = temp_types.at(3);
		pokemon_db.push_back(temp);		//push the complete pokemon back into a the vector of pokemon
	}
	for (Pokemon p : pokemon_db) {
		if (p.type1 == "Water" or p.type2 == "Water") water_pokemon_db.push_back(p);
	}
	if constexpr(DEBUG) cout << "Pokemon loaded successfully." << endl;
}

//Reads all moves from moves.txt into move_db, discards all status moves
void load_move_db(string filename) {
	ifstream ins(filename);
	if (!ins) die("Couldn't load "s + filename);

	while (ins) {
		//Read from the file, make a temp Move with all the data read in
		Move temp;
		vector<string> temp_move_attributes;
		string line;
		getline(ins, line);
		if (!ins) break;
		istringstream ss(line);
		string token;
		while (getline(ss, token, '\t')) {		//parse the string at tabs
			temp_move_attributes.push_back(token);		//push each piece back into a temporary vector
		}

		//fill each class member variable with data from the temporary vector
		temp.index = stoi(temp_move_attributes.at(0));
		temp.name = temp_move_attributes.at(1);
		temp.type = temp_move_attributes.at(2);
		temp.category = temp_move_attributes.at(3);
		if (!isdigit(temp_move_attributes.at(4).at(0))) temp.PP = 0;
		else temp.PP = stoi(temp_move_attributes.at(4));
		if (!isdigit(temp_move_attributes.at(5).at(0))) temp.power = 0;
		else temp.power = stoi(temp_move_attributes.at(5));
		if (!isdigit(temp_move_attributes.at(6).at(0))) temp.accuracy = 0;
		else temp.accuracy = stoi(temp_move_attributes.at(6));

		if (temp.category == "Status") continue;

		move_db.push_back(temp);	//push the complete move back into a vector of moves

	}
	if constexpr(DEBUG) cout << "Moves loaded successfully." << endl;
}

vector<vector<double>>type_system(18,vector<double>(18));

const size_t NUM_TYPES = 18;

void load_type_system(string filename = "type_system.txt") {	//WRK - assert that type system rows and cols are the same as the file height and width
	double temp = 0;
	ifstream ins(filename);
	for (size_t i = 0; i < NUM_TYPES; i++) {
		for (size_t j = 0; j < NUM_TYPES; j++) {
			ins >> type_system.at(i).at(j);
		}
	}
}

Pokemon select_pokemon(vector<Pokemon> vec) {
	Pokemon temp;
	string name;
	bool found = false;
	while (true) {
		if (found) break;
		getline(cin, name);	//WRK - implement string_search()?
		auto iter = find(vec.begin(), vec.end(), name);

		if (iter != vec.end()) {
			temp = *iter;
			break;
		}
	}
	return temp;
}

const int MAX_MOVES = 4, MIN_MOVES = 1;

void add_moves(Pokemon &temp) {
	cout << "Enter up to four moves for " << temp.name << ": " << endl;
	cout << "(use move name or index. DONE to stop adding moves.)" << endl;
	int move_count = 0;
	string move_name;
	vector<Move> move_vec;
	bool movesFilled = false;
	while (true) {
		if (movesFilled) break;
		getline(cin, move_name);
		if (uppercaseify(move_name) == "DONE") {
			if (move_vec.size() < MIN_MOVES) {
				for (Move m : move_db) {	//WRK - implement find()
					if (m.name == "Struggle") 
						move_vec.push_back(m);
				}
				//move_vec.push_back(move_db.at(move_db.size() - 1));	//WRK - change from last item to struggles index search for custom move compatiability
				cout << "\nNo moves selected. \"" << move_vec.at(0).name << "\" has been added to " << temp.name << "'s moves.\n";
			}
			break;
		}
		for (Move m : move_db) {	//WRK - implement find() or string_search()
			if (move_name == m.name or move_name == to_string(m.index)) {
				move_vec.push_back(m);
				cout << "Move \"" << m.name << "\" added to " << temp.name << "'s moves." << endl;
			}
		}
		if (move_vec.size() == MAX_MOVES) {
			movesFilled = true;
			cout << "\nMoves for " << temp.name << " are now full.\n\n\n";
			break;
		}
		if (!move_vec.size()) cout << "Add a move: \n";
		else cout << "Add another move: \n";
	}
	temp.moves = move_vec;
	cout << temp.name << "'s moves: \n\n";
	for (Move m : temp.moves) {
		cout << m << endl;
	}
	cout << endl << endl;
}

int lookup_type(string s) {
	if (s == "Normal") return 0;
	else if (s == "Fighting") return 1;
	else if (s == "Flying") return 2;
	else if (s == "Poison") return 3;
	else if (s == "Ground") return 4;
	else if (s == "Rock") return 5;
	else if (s == "Bug") return 6;
	else if (s == "Ghost") return 7;
	else if (s == "Steel") return 8;
	else if (s == "Fire") return 9;
	else if (s == "Water") return 10;
	else if (s == "Grass") return 11;
	else if (s == "Electric") return 12;
	else if (s == "Psychic") return 13;
	else if (s == "Ice") return 14;
	else if (s == "Dragon") return 15;
	else if (s == "Dark") return 16;
	else if (s == "Fairy") return 17;
	else return -1;
}

void print_battle(Pokemon p1, Pokemon p2, float healthP2) {
	system("clear");
	cout << "\n\n";
	print_pokemon(p2.index);
	cout << "\n\n";
	float health_bar = (p2.hp / healthP2) * 100;
	cout << "[";
	for (int i = 0; i < health_bar; i++) cout << "#";
	for (int i = 0; i < (((healthP2-p2.hp)/healthP2)*100); i++) cout << " ";
	cout << "]\t" << p2.hp << "/" << healthP2 << " HP\n\n";
	int i = 0;
	for (Move m : p1.moves) 
		cout << ++i << ") " << m << endl;
	cout << endl;
}

void fight(Pokemon &p1, Pokemon &p2) {
	//p1 pokemon attacks p2 pokemon
	print_battle(p1, p2, p2.original_hp);
	string input;
	int choice = 0;
	cout << "Choose a move for " << p1.name << " to use against " << p2.name << " (enter the move's inventory number):\n";
	while (true) {
		cin >> input;
		if (isdigit(input.at(0))) {
			choice = stoi(input);
			if (choice > 0 and choice < 5) break;
		}
		else continue;
	}
	Move move = p1.moves.at(choice-1);

	int type1 = lookup_type(p2.type1);      //defending pokemon's type 1 type-system multiplier (column)
	int type2 = lookup_type(p2.type2);      //defending pokemon's type 2 type-system multiplier (column)
	int  move_type = lookup_type(move.type);    //attacking move's type system multiplier (row)
	float type_multiplier = 1.0;
	if (lookup_type(p2.type2) != -1) type_multiplier = type_system.at(move_type).at(type1) * type_system.at(move_type).at(type2);
	else type_multiplier = type_system.at(move_type).at(type1);
	float damage = (move.power * p1.attack * type_multiplier) / p2.defense;
	bool stab = false;
	if (p1.type1 == move.type or p1.type2 == move.type) {
		damage *= 1.5;
		stab = true;
	}
	p2.hp -= damage;
	if (p2.hp < 0) p2.hp = 0;
	print_battle(p1, p2, p2.original_hp);
	if constexpr (DEBUG) {
		cout << "type_multiplier(" << type_multiplier << ") = " << type_system.at(move_type).at(type1);
		if (type2 != -1) cout << " * " << type_system.at(move_type).at(type2) << endl;
		else cout << endl;
		cout << "damage(" << damage << ") = p1_move.power(" << move.power << ") * p1.attack(" << p1.attack << ") * type_multiplier(" << type_multiplier << ")] / p2.defense(" << p2.defense << ") " << (stab ? " * STAB(150%)" : "") << ".\n\n";
	}
	cout << p1.name << " used " << move.name << ", which dealt " << damage << " damage to " << p2.name << ".\n" << p2.name << "'s HP is now " << p2.hp << ".\n\n";
	if (type_multiplier < 1) cout << "It's not very effective...\n";
	else if (type_multiplier > 1) cout << "It's super effective!.\n";
	if (stab) cout << "Same Type Attack Bonus applied. 50% extra damage!\n\n";
	if (p2.hp == 0) {
		cout << p2.name << " has fainted. "  << p1.name << " has won!\n\n";
	}
	p1.moves.at(choice-1).PP--;
	if (p1.moves.at(choice-1).PP == 0) p1.moves.erase(p1.moves.begin()+choice-1);
	cout << "ENTER to continue.\n";
	string temp;
	getline(cin, temp);
	getline(cin, temp);
}


void explore_fight(Pokemon &p1, Pokemon &p2) {
	//Enemy pokemon attacks your pokemon
	print_battle(p1, p2, p2.original_hp);
	int choice = (rand() % MAX_MOVES) + 1;
	Move move = p1.moves.at(choice-1);
	cout << p1.name << "'s move.\n\nENTER to continue.\n";
	string temp;
	getline(cin, temp);
	cout << "\n\nEnemy " << p1.name << " used " << move.name << "!\n";

	int type1 = lookup_type(p2.type1);		//defending pokemon's type 1 type-system multiplier (column)
	int type2 = lookup_type(p2.type2);		//defending pokemon's type 2 type-system multiplier (column)
	int  move_type = lookup_type(move.type);	//attacking move's type system multiplier (row)
	float type_multiplier = 1.0;
	if (lookup_type(p2.type2) != -1) type_multiplier = type_system.at(move_type).at(type1) * type_system.at(move_type).at(type2);
	else type_multiplier = type_system.at(move_type).at(type1);
	float damage = (move.power * p1.attack * type_multiplier) / p2.defense;
	bool stab = false;
	if (p1.type1 == move.type or p1.type2 == move.type) {
		damage *= 1.5;
		stab = true;
	}
	p2.hp -= damage;
	if (p2.hp < 0) p2.hp = 0;
	print_battle(p1, p2, p2.original_hp);
	if constexpr (DEBUG) {
		cout << "type_multiplier(" << type_multiplier << ") = " << type_system.at(move_type).at(type1);
		if (type2 != -1) cout << " * " << type_system.at(move_type).at(type2) << endl;
		else cout << endl;
		cout << "damage(" << damage << ") = p1_move.power(" << move.power << ") * p1.attack(" << p1.attack << ") * type_multiplier(" << type_multiplier << ")] / p2.defense(" << p2.defense << ") " << (stab ? " * STAB(150%)" : "") << ".\n\n";
	}
	if (type_multiplier < 1) cout << "It's not very effective...\n";
	else if (type_multiplier > 1) cout << "It's super effective!.\n";
	if (stab) cout << "Same Type Attack Bonus applied. 50% extra damage!\n\n";
	cout << p1.name << " used " << move.name << ", which dealt " << damage << " damage to " << p2.name << ".\n" << p2.name << "'s HP is now " << p2.hp << ".\n\n";
	if (p2.hp == 0) {
		cout << p2.name << " has fainted. "  << p1.name << " has won!\n\n";
	}
	p1.moves.at(choice-1).PP--;
	if (p1.moves.at(choice-1).PP == 0) p1.moves.erase(p1.moves.begin()+choice-1);
	cout << "ENTER to continue.\n";
	getline(cin, temp);
}

//This conducts a one versus one battle between two pokemon of your choice
void battle_mode() {
	Pokemon oneP, twoP;
	//Pick a Pokemon for team one
	cout << "Choose a Pokemon for team 1 (enter the name): " << endl;
	oneP = select_pokemon(pokemon_db);
	cout << "Go! " << oneP.name << "!\n";
	//Pick up to four moves for team one's pokemon
	add_moves(oneP);
	//Pick a Pokemon for team two
	cout << "Choose a Pokemon for team 2 (enter the name): " << endl;
	twoP = select_pokemon(pokemon_db);
	cout << twoP.name << ", I choose you!\n";
	//Pick up to four moves for team two's pokemon
	add_moves(twoP);
	//Whichever Pokemon has the higher speed goes first
	Pokemon goesFirst, goesSecond;

	(oneP.speed > twoP.speed ? (goesFirst = oneP, goesSecond = twoP) : (goesFirst = twoP, goesSecond = oneP));
	if (oneP.speed == twoP.speed) {
		int coin_flip = (rand() % 2);
		if (coin_flip) {
			goesFirst = oneP;
			goesSecond = twoP;
		}
	}
	cout << "Team one -- " << oneP.name << "'s speed: " << oneP.speed << endl;
	cout << "Team two -- " << twoP.name << "'s speed: " << twoP.speed << endl;
	cout << goesFirst.name << " goes first.\n" << goesSecond.name << " goes second.\n";
	//Have them do damage to each other based on their move * power * type modifier
	//Target Pokémon reduces damage based on its defense or special defense
	while (true) {
		fight(goesFirst, goesSecond);
		if (goesSecond.hp == 0) {
			goesSecond.hp = goesSecond.original_hp;
			break;
		}
		fight(goesSecond, goesFirst);
		if (goesFirst.hp == 0) {
			goesFirst.hp = goesFirst.original_hp;	
			break;
		}
	}
}


Pokemon choose_starter() {
	Pokemon temp;
	string name;
	vector<Pokemon> starters;
	bool found = false;
	for (Pokemon p : pokemon_db) {
		if (p.index == 1 or p.index == 4 or p.index == 7 or p.index == 16) 
			starters.push_back(p);
	}
	cout << "\n\n\nPlease select your starter pokemon from the following:\n";
	for (Pokemon p : starters) 
		cout << p << endl;
	while (true) {
		getline(cin, name);
		for (Pokemon p : starters) {
			if (uppercaseify(name) == uppercaseify(p.name) or name == to_string(p.index)) {
				found = true;
				temp = p;
				break;
			} else continue;
		}
		if (found) break;
	}
	cout << "\nYou chose \"" << temp.name << "\"\n";
	cout << "ENTER to continue.\n";
	getline(cin, name);
	return temp;

}
vector<Pokemon> inventory;

void explore_battle(string location) {
	turn_off_ncurses();
	Pokemon oneP, twoP;
	//Randomly generate an enemy pokemon
	int random_index = rand() % pokemon_db.size();

	for (Pokemon p : pokemon_db) {
		if (p.index == random_index) twoP = p;
	}
	if (location == "water") twoP = water_pokemon_db.at(rand() % water_pokemon_db.size()); 
	print_pokemon(twoP.index);
	cout << "\nWild " << twoP.name << " appeared!\n";
	while (true) {	
		cout << "\n1) FIGHT\n2) RUN\n";
		string choice;
		cin >> choice;
		if (uppercaseify(choice) == "RUN" or choice == "2") return;
		else break;
	}

	//Pick a Pokemon out of your inventory
	cout << "\nChoose a Pokemon from your inventory (enter the name): " << endl;
	for (Pokemon p : inventory) {
		cout << p << endl;
	} 
	oneP = select_pokemon(inventory);
	cout << "Go! " << oneP.name << "!\n";
	//Pick up to four moves for team one's pokemon
	add_moves(oneP);
	//Randomly generate 4 moves for the enemy
	for (int i = 0; i < MAX_MOVES; i++) {
		int random_move = rand() % move_db.size();
		twoP.moves.push_back(move_db.at(random_move));
	}
	cout << "Enemies moves: " << endl;
	for (Move m : twoP.moves) {
		cout << m << endl;
	}
	//Whichever Pokemon has the higher speed goes first
	Pokemon goesFirst, goesSecond;

	(oneP.speed > twoP.speed ? (goesFirst = oneP, goesSecond = twoP) : (goesFirst = twoP, goesSecond = oneP));
	if (oneP.speed == twoP.speed) {
		int coin_flip = (rand() % 2);
		if (coin_flip) {
			goesFirst = oneP;
			goesSecond = twoP;
		}
	}
	//Have them do damage to each other based on their move * power * type modifier
	//Target Pokémon reduces damage based on its defense or special defense
	string temp;
	if (goesFirst.name == oneP.name) {
		while (true) {
			fight(oneP, twoP);
			if (twoP.hp == 0) {
				twoP.hp = twoP.original_hp;
				inventory.push_back(twoP);
				cout << twoP.name << " has been added to your inventory.\n\nENTER to continue.\n";
				getline(cin,temp);
				break;
			}
			explore_fight(twoP, oneP);
			if (oneP.hp == 0) break;
		}
	} else {
		while (true) {
			explore_fight(twoP, oneP);
			if (oneP.hp == 0) break;
			fight(oneP, twoP);
			if (twoP.hp == 0) {
				twoP.hp = twoP.original_hp;
				inventory.push_back(twoP);
				cout << twoP.name << " has been added to your inventory.\n\nENTER to continue.\n";
				getline(cin,temp);
				break;
			}
		}
	}
}

void explore_mode() {
	inventory.push_back(choose_starter());
	hrc::time_point old_time = hrc::now();
	turn_on_ncurses(); //Turn on full screen mode
	Map map;
	random_device rd;     // only used once to initialise (seed) engine
	mt19937 gen(rd());    // random-number engine used (Mersenne-Twister in this case)
	uniform_int_distribution<int> d100(1, 100);
	int x = Map::SIZE / 2, y = Map::SIZE / 2; //Start in middle of the world
	while (true) {
		int ch = getch(); // Wait for user input, with TIMEOUT delay
		if (ch == 'q' || ch == 'Q') break;
		else if (ch == RIGHT) {
			if (map.get(x + 1, y) == Map::WALL) {
				continue;
			} else if (map.get(x + 1, y) == Map::WATER) {	//WRK - implement water type pokemon battles in water areas
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					x++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::WATER) {
					x++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::GRASS) {
					x++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("water");
			} else if (map.get(x + 1, y) != Map::WATER and map.get(x, y) == Map::WATER) {
				map.set(x, y, Map::WATER);
				x++;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x + 1, y) != Map::GRASS and map.get(x, y) == Map::GRASS) {
				map.set(x, y, Map::GRASS);
				x++;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x + 1, y) == Map::GRASS) {
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					x++;
					map.set(x, y, Map::GRASS);
				} else if (map.get(x, y) == Map::GRASS) {
					x++;
					map.set(x, y, Map::GRASS);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("grass");
			} else if (map.get(x + 1, y) != Map::WATER) {
				map.set(x, y, Map::OPEN);
				x++;
				map.set(x, y, Map::TRAINER);
			}
			//LEFT
		} else if (ch == LEFT) {
			if (map.get(x - 1, y) == Map::WALL) {
				continue;
			} else if (map.get(x - 1, y) == Map::WATER) {	//WRK - implement water type pokemon battles in water areas
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					x--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::WATER) {
					x--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::GRASS) {
					x--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("water");
			} else if (map.get(x - 1, y) != Map::WATER and map.get(x, y) == Map::WATER) {
				map.set(x, y, Map::WATER);
				x--;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x - 1, y) != Map::GRASS and map.get(x, y) == Map::GRASS) {
				map.set(x, y, Map::GRASS);
				x--;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x - 1, y) == Map::GRASS) {
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					x--;
					map.set(x, y, Map::GRASS);
				} else if (map.get(x, y) == Map::GRASS) {
					x--;
					map.set(x, y, Map::GRASS);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 20) explore_battle("grass");
			} else if (map.get(x - 1, y) != Map::WATER) {
				map.set(x, y, Map::OPEN);
				x--;
				map.set(x, y, Map::TRAINER);
			}
			//DOWN
		} else if (ch == DOWN) {
			if (map.get(x, y + 1) == Map::WALL) {
				continue;
			} else if (map.get(x, y + 1) == Map::WATER) {   //WRK - implement water type pokemon battles in water areas
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					y++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::WATER) {
					y++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::GRASS) {
					y++;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("water");
			} else if (map.get(x, y + 1) != Map::WATER and map.get(x, y) == Map::WATER) {
				map.set(x, y, Map::WATER);
				y++;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x, y + 1) != Map::GRASS and map.get(x, y) == Map::GRASS) {
				map.set(x, y, Map::GRASS);
				y++;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x, y + 1) == Map::GRASS) {
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					y++;
					map.set(x, y, Map::GRASS);
				} else if (map.get(x, y) == Map::GRASS) {
					y++;
					map.set(x, y, Map::GRASS);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("grass");
			} else if (map.get(x, y + 1) != Map::WATER) {
				map.set(x, y, Map::OPEN);
				y++;
				map.set(x, y, Map::TRAINER);
			}
			//UP
		} else if (ch == UP) {
			if (map.get(x, y - 1) == Map::WALL) {
				continue;
			} else if (map.get(x, y - 1) == Map::WATER) {   //WRK - implement water type pokemon battles in water areas
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					y--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::WATER) {
					y--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else if (map.get(x, y) == Map::GRASS) {
					y--;
					map.set(x, y, Map::WATER);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("water");
			} else if (map.get(x, y - 1) != Map::WATER and map.get(x, y) == Map::WATER) {
				map.set(x, y, Map::WATER);
				y--;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x, y - 1) != Map::GRASS and map.get(x, y) == Map::GRASS) {
				map.set(x, y, Map::GRASS);
				y--;
				map.set(x, y, Map::TRAINER);
				if (x < 0) x = 0;
			} else if (map.get(x, y - 1) == Map::GRASS) {
				if (map.get(x, y) == Map::TRAINER) {
					map.set(x, y, Map::OPEN);
					y--;
					map.set(x, y, Map::GRASS);
				} else if (map.get(x, y) == Map::GRASS) {
					y--;
					map.set(x, y, Map::GRASS);
					if (x < 0) x = 0;
				} else continue;
				if (d100(gen) <= 30) explore_battle("grass");
			} else if (map.get(x, y - 1) != Map::WATER) {
				map.set(x, y, Map::OPEN);
				y--;
				map.set(x, y, Map::TRAINER);
			}
		} else if (ch == ERR) { //No keystroke
			; //Do nothing
		}
		//clear(); //Uncomment these lines if the code is drawing garbage
		map.draw(x, y);
		mvprintw(Map::DISPLAY + 1, 0, "X: %i Y: %i\n", x, y);
		usleep(10000); //pause for 10000 us
		//refresh(); //Uncomment these lines if the code is drawing garbage
		hrc::time_point new_time = hrc::now();
		chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(new_time - old_time);
		mvprintw(Map::DISPLAY + 2, 0, "FPS: %5.1f\n", 1 / time_span.count());
		old_time = new_time;
	}
	//Exit NCURSES
	clear();
	endwin(); // End curses mode
	system("clear");
}

int main() {
#ifndef MADE_USING_MAKEFILE
	static_assert(0, "Compile this code using 'make' not 'compile.");
#endif
	srand(time(0));
	system("figlet POKEMON");
	system("figlet ++ and \\#");
	cout << "Do you want to use the Gen1 Pokémon? (Type \"NO\" for no, anything else for yes.)\n";
	string answer;
	getline(cin, answer);
	for (char &c : answer) c = toupper(c); //Uppercaseify
	string filename1 = "pokemon.txt", filename2 = "types.txt";
	if (answer == "NO") {
		cout << "Please enter the two files containing the Pokémon and types for the Pokémon.\n";
		cin >> filename1 >> filename2;
		load_pokemon_db(filename1, filename2);
	} else
		load_pokemon_db(filename1, filename2);

	// Debug the load from pokemon.txt and types.txt
	if constexpr(DEBUG) {
		for (Pokemon p : pokemon_db) {
			cerr << "Pokedex Entry " << p.index << ": " << p.name << " hp: " << p.hp << " attack: " << p.attack;
			cerr << " defense: " << p.defense << " speed: " << p.speed << " special: " << p.special;
			cerr << " type1: " << p.type1 << " type2: " << p.type2 << endl;
		}
	}
	if constexpr(DEBUG) {
		cout << "WATER POKEMON DB: " << endl;
		for (Pokemon p : water_pokemon_db) {
			cerr << "Pokedex Entry " << p.index << ": " << p.name << " hp: " << p.hp << " attack: " << p.attack;
			cerr << " defense: " << p.defense << " speed: " << p.speed << " special: " << p.special;
			cerr << " type1: " << p.type1 << " type2: " << p.type2 << endl;
		}
	}

	string filename3 = "moves.txt";
	if (answer == "NO") {
		cout << "Please enter the file containing the moves for your Pokémon.\n";
		cin >> filename3;
		load_move_db(filename3);
	} else
		load_move_db(filename3);

	// DEBUG the load from moves.txt
	if constexpr(DEBUG) {
		for (Move m : move_db) {
			if (m.index == 0) continue;
			cerr << "Move Index " << m.index << ": " << m.name << " " << " type: " << m.type << " category: " << m.category << " PP: " << m.PP << " power: " << m.power << " accuracy: " << m.accuracy << "%\n";
		}
	}
	string filename4 = "type_system.txt";
	load_type_system(filename4);

	// Debug the load from type_system.txt
	if constexpr(DEBUG) {
		for (auto rows : type_system) {
			for (auto elem : rows) {
				cout << elem << "\t";
			}
			cout << endl;
		}
	}

	cout << "Do you want to\n1) Print Pokémon Data?\n2) Print Move Data?\n3) Pokemon Battle (1v1)\n4) Explore the World?\n";
	int choice = 0;
	cin >> choice;
	if (!cin || choice < 1 || choice > 4) die();
	if (choice == 1) {
		cout << "Please enter the Pokedex number of the Pokémon whose data you want to print:\n";
		int index = 0;
		cin >> index;
		if (!cin or index <= 0) die();
		//	Pokemon p = pokemon_db.at(index+1); //Pokedex starts at 1, this only works for Gen1 Pokémon
		Pokemon p;
		for (const Pokemon &poke : pokemon_db) if (index == poke.index) p = poke;
		cout << "Pokedex Entry " << p.index << ": " << p.name << " hp: " << p.hp << " attack: " << p.attack;
		cout << " defense: " << p.defense << " speed: " << p.speed << " special: " << p.special;
		cout << " type1: " << p.type1 << " type2: " << p.type2 << endl;
	} else if (choice == 2) {
		cout << "Please enter the move number of the move whose data you want to print:\n";
		int index = 0;
		cin >> index;
		if (!cin) die();
		Move m;
		for (const Move &mo : move_db) if (index == mo.index) m = mo;
		if (m.name == "Error" or m.index == 0) cout << "No move loaded with that index.\n";
		else
			cout << "Move Index " << m.index << ": " << m.name << " " << " type: " << m.type << " category: " << m.category << " PP: " << m.PP << " power: " << m.power << " accuracy: " << m.accuracy << "%\n";
	}
	if (choice == 3) battle_mode(); 
	if (choice == 4) explore_mode();
	else return 0;
}
