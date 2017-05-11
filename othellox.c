#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#include <float.h>


//Representation of pieces
#define EMPTY 0
#define BLACK 1
#define WHITE 2

//parallelism
#define MASTER_ID slaves
#define COMPUTATION_TAG 9999

int slaves;//num of slaves
int myid;//id of process
long long comm_time = 0;
long long comp_time = 0;

int boardVisited = 0;
int deepestDepthVisited = 0;
int searchedEntire = 1;
double timeTaken = 0.0;

// initialbrd
int boardSize;
int xSize, ySize;
int color;
char initialWhite[676][4];
char initialBlack[676][4];
int initialBlackCount, initialWhiteCount;
int timeOut;

// evalparams 
int maxDepth;
int maxBoards;
int cornerValue;
int edgeValue;
clock_t start, current;

//int DIRECTION[8][2] = { UP,UP_RIGHT,UP_LEFT,RIGHT,LEFT,DOWN,DOWN_RIGHT,DOWN_LEFT };
int DIRECTION[8][2] = { { 0,1 } ,{ 1,1 },{ -1,1 },{ 1,0 },{ -1,0 },{ 0,-1 },{ 1,-1 },{ -1,-1 } };

/*START OF UTILITIES FUNCTION*/
//get current time
long long wall_clock_time() {
#ifdef LINUX
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (long long)(tp.tv_nsec + (long long)tp.tv_sec * 1000000000ll);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)(tv.tv_usec * 1000 + (long long)tv.tv_sec * 1000000000ll);
#endif
}
//trim white spaces given a string, from stackoverflow
char *trimWhiteSpace(char *str) {
	char *end;
	if (str == NULL)
		return str;
	// Trim leading space
	while (isspace((unsigned char)*str))
		str++;
	if (*str == 0)
		return str;
	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) {
		end--;
	}
	*(end + 1) = 0;

	return str;
}
//print visualized board
void printBoard(char *board) {

	printf("\n");
	printf("    ");
	int i, j;
	for (i = 0; i<xSize; i++) {
		printf("%c ", 'a' + i);
	}
	//i is y axis
	for (i = 0; i < ySize; i++) {
		printf("\n %02d ", i + 1);
		//j is x axis
		for (j = 0; j < xSize; j++) {
			if (board[j*xSize + i] == WHITE)
				printf("w ");
			else if (board[j*xSize + i] == BLACK)
				printf("b ");
			else
				printf(". ");
		}
	}

}
//read argument files
void readFile(char *initialbrd, char *evalparams) {
	char line[300];
	char *left, *right;
	FILE *fileInitial = fopen(initialbrd, "r");
	FILE *fileEval = fopen(evalparams, "r");

	// read initialbrd
	while (fgets(line, sizeof(line), fileInitial)) {
		left = strtok(line, ":");
		if (left != NULL)
			right = trimWhiteSpace(strtok(NULL, ":"));
		if (strcmp(left, "Size") == 0) {
			xSize = atoi(strtok(right, ","));
			ySize = atoi(strtok(NULL, ","));
			boardSize = xSize * ySize;
		}
		else if (strcmp(left, "Color") == 0) {
			if (strcmp(right, "Black") == 0)
				color = BLACK;
			else
				color = WHITE;
		}
		else if (strcmp(left, "Timeout") == 0)
			timeOut = atoi(right);
		else {
			int countInitial = 0;
			right = strtok(trimWhiteSpace(strtok(right, "{}")), ",");
			while (right != NULL) {
				if (strcmp(left, "White") == 0)
					strcpy(initialWhite[countInitial], right);
				else if (strcmp(left, "Black") == 0)
					strcpy(initialBlack[countInitial], right);
				right = strtok(NULL, ",");
				countInitial++;
			}
			if (strcmp(left, "White") == 0)
				initialWhiteCount = countInitial;
			else
				initialBlackCount = countInitial;
		}
	}

	//evalparams.txt
	while (fgets(line, sizeof(line), fileEval)) {
		left = strtok(line, ":");
		if (left != NULL)
			right = trimWhiteSpace(strtok(NULL, ":"));
		if (strcmp(left, "MaxDepth") == 0)
			maxDepth = atoi(right);
		else if (strcmp(left, "MaxBoards") == 0)
			maxBoards = atoi(right);
		else if (strcmp(left, "CornerValue") == 0)
			cornerValue = atoi(right);

		else if (strcmp(left, "EdgeValue") == 0)
			edgeValue = atoi(right);
	}
	fclose(fileInitial);
	fclose(fileEval);
}
//convert alphabet and number string to board index
int stringToBoard(char *pos) {
	//convert a,b,c to 1,2,3
	int alphabet = pos[0] - 96;
	int num;
	//convert 1,2,3 in char to int
	if (strlen(pos) == 2)
		num = pos[1] - 48;
	else
		num = (pos[1] - 48) * 10 + (pos[2] - 48);
	//board position =  letter part*xSize + num part 
	return (alphabet - 1) * xSize + (num - 1);
}
//convert index to string containing the alphabet and number
char *boardToString(int i) {

	int num = (i + 1) % xSize;
	if (num == 0) num = xSize;

	int alphabet = ((i + 1) - num) / xSize;
	char alphabetChar = alphabet + 97;

	char stringNum[10];
	sprintf(stringNum, "%d", num);

	char string[5];
	string[0] = alphabetChar;
	string[1] = stringNum[0];
	if (num > 9) {
		string[2] = stringNum[1];
		string[3] = '\0';
	}
	else
		string[2] = '\0';

	char *result = malloc(5);
	strcpy(result, string);
	return result;
}
//print results
void printResult(int *bestMoves, int bestMovesCount, double timeTaken) {
	printf("\nBest moves: { ");
	if (bestMovesCount == 0) {
		printf("na }\n");
	}
	else {
		int i;
		for (i = 0; i < bestMovesCount; i++) {
			if (i == bestMovesCount - 1) {
				printf("%s }\n", boardToString(bestMoves[i]));
			}
			else {
				printf("%s,", boardToString(bestMoves[i]));
			}
		}
	}

	printf("Number of boards assessed: %d\n", boardVisited);
	printf("Depth of boards: %d\n", deepestDepthVisited);
	if (searchedEntire) {
		printf("Entire space: true\n");
	}
	else {
		printf("Entire space: false\n");
	}
	printf("Elapsed time in seconds: %f\n", timeTaken);
}
/*END OF UTILITIES FUNCTION*/

/*START OF SLAVE AND MASTER*/
void master(char *initialbrd, char *evalparams) {
	char board[676];
	int bestMoves[676];
	int bestMovesCount = 0;

	//read file
	readFile(initialbrd, evalparams);
	//Share information with slaves
	//initialbrd
	MPI_Bcast(&xSize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&ySize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&boardSize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&initialBlackCount, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&initialWhiteCount, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&color, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	int i;
	for (i = 0; i < initialWhiteCount; i++)
		MPI_Bcast(&initialWhite[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);

	for (i = 0; i < initialBlackCount; i++)
		MPI_Bcast(&initialBlack[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	//evalparams
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);

	//start of mini max algorithm
	clock_t begin = clock();
	initBoard(board);
	printBoard(board);
	getMinimaxMoves(board, bestMoves, &bestMovesCount);
	clock_t end = clock();
	timeTaken = (double)(end - begin) / CLOCKS_PER_SEC;

	int slave_id;
	//send terminating request to all slaves
	int request = 0;
	for (slave_id = 0; slave_id < slaves; slave_id++)
		MPI_Send(&request, 1, MPI_INT, slave_id, COMPUTATION_TAG, MPI_COMM_WORLD);

	printResult(bestMoves, bestMovesCount, timeTaken);
}
void slave() {
	MPI_Status status;
	int requestNo;
	int boardSizeInfo;
	int playerInfo;
	int indexInfo;
	int arrayInfo[800];
	char boardInfo[676];
	int results[800];
	int result;
	long long before, after;
	int i;
	//get information from master
	//initialbrd
	MPI_Bcast(&xSize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&ySize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&boardSize, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&initialBlackCount, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&initialWhiteCount, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&color, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	for (i = 0; i < initialWhiteCount; i++) {
		MPI_Bcast(&initialWhite[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	for (i = 0; i < initialBlackCount; i++) {
		MPI_Bcast(&initialBlack[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	//evalparams
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);

	while (1) {
		before = wall_clock_time();
		//get request
		MPI_Recv(&requestNo, 1, MPI_INT, MPI_ANY_SOURCE, COMPUTATION_TAG, MPI_COMM_WORLD, &status);
		if (requestNo == 1) { //request to find whether a move is valid
			MPI_Recv(&boardInfo, boardSize, MPI_CHAR, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&playerInfo, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			if (boardSize > slaves) {//more than 1 moves to check for
				MPI_Recv(&boardSizeInfo, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				MPI_Recv(&arrayInfo, boardSizeInfo, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				after = wall_clock_time();
				comm_time += after - before;
				//check if moves is valid
				before = wall_clock_time();
				for (i = 0; i < boardSizeInfo; i++) {
					if (legalP(boardInfo, arrayInfo[i], playerInfo))
						results[i] = 1;
					else
						results[i] = 0;
				}
				after = wall_clock_time();
				comp_time += after - before;
				//send result back to master
				before = wall_clock_time();
				MPI_Send(results, boardSizeInfo, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				after = wall_clock_time();
				comm_time += after - before;
			}
			else {//only a single move to check for
				MPI_Recv(&indexInfo, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				after = wall_clock_time();
				comm_time += after - before;
				//check if a move is valid
				before = wall_clock_time();
				if (legalP(boardInfo, indexInfo, playerInfo))
					result = 1;
				else
					result = 0;
				after = wall_clock_time();
				comp_time += after - before;
				//send result back to master
				before = wall_clock_time();
				MPI_Send(&result, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				after = wall_clock_time();
				comm_time += after - before;
			}
		}
		else if (requestNo == 0)//terminating request
			break;
	}
	//printf(" --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
}
/*END OF SLAVE MASTER*/

/*START OF BOARD FUNCTION*/
//Create the initial board
void initBoard(char *board) {
	int i;
	for (i = 0; i < boardSize; i++)
		board[i] = EMPTY;
	for (i = 0; i < initialWhiteCount; i++)
		board[stringToBoard(initialWhite[i])] = WHITE;

	for (i = 0; i < initialBlackCount; i++)
		board[stringToBoard(initialBlack[i])] = BLACK;

}
//duplicate a board
void duplicateBoard(char *from, char *to) {
	int i;
	for (i = 0; i < boardSize; i++)
		to[i] = from[i];
}
//get opponent
int opponent(int color) {
	if (color == BLACK)
		return WHITE;
	else if (color == WHITE)
		return BLACK;
	return 0;
}
//convert alphabet and number to the board index
int convertBoardIndex(int alphabet, int num) {
	return (alphabet - 1) * xSize + (num - 1);
}
//check whether a move is legal
int legalP(char *board, int i, int player) {

	int num = (i + 1) % xSize;
	if (num == 0) num = xSize;

	int alphabet = ((i + 1) - num) / xSize + 1;
	//printf("%d,%d\n", num, alphabet);
	int j;
	if (board[i] == EMPTY) {
		j = 0;
		while (j <= 7 && wouldFlip(alphabet, num, board, player, DIRECTION[j]) == -1) {
			j++;
		}
		if (j == 8)
			return 0;
		else {
			return 1;
		}
	}
	return 0;
}
//check if within board range
int validP(int num, int alphabet) {
	if (num<1 || num > ySize)
		return -1;
	if (alphabet < 1 || alphabet > xSize)
		return -1;
	return 1;
}
//check if a move will make any flips in a direction
int wouldFlip(int alphabet, int num, char *board, int player, int move[2]) {
	int y = num + move[1];
	int x = alphabet + move[0];

	if (!validP(x, y))
		return -1;
	if (board[convertBoardIndex(x, y)] == opponent(player)) {
		x += move[0];
		y += move[1];
		if (!validP(x, y))
			return -1;
		return findBracketingPiece(x, y, player, board, move);
	}
	else return -1;
}
//find a bracketing piece that surround opponent piece
int findBracketingPiece(int x, int y, int player, char *board, int move[2]) {
	int opp = opponent(player);
	while (board[convertBoardIndex(x, y)] == opp) {
		x += move[0];
		y += move[1];
		if (!validP(x, y))
			return -1;
	};

	if (board[convertBoardIndex(x, y)] == player)
		return convertBoardIndex(x, y);
	else {
		return -1;
	}
}

//flip opponent piece caught in between
int makeFlips(int alphabet, int num, int player, char *board, int move[2]) {
	int countFlipped = 0;
	int bracketer = wouldFlip(alphabet, num, board, player, move);
	//if there is a bracket piece
	if (bracketer != -1) {
		alphabet += move[0];
		num += move[1];
		while (1) {
			//flip
			board[convertBoardIndex(alphabet, num)] = player;
			countFlipped++;
			alphabet += move[0];
			num += move[1];
			//terminate
			if (convertBoardIndex(alphabet, num) == bracketer)
				return countFlipped;
		}
	}
}
//make a actual move on the board
int makeMove(char *board, int move, int player) {
	int countFlips = 0;
	int i;
	board[move] = player;

	int num = (move + 1) % xSize;
	if (num == 0) move = xSize;

	int alphabet = ((move + 1) - num) / xSize + 1;

	//for all directions, check flipping
	for (i = 0; i <= 7; i++)
		countFlips += makeFlips(alphabet, num, player, board, DIRECTION[i]);

	return countFlips;
}
//check if any legal move for a given player
int anyLegalMove(int player, char * board) {
	int i;
	for (i = 0; i < boardSize; i++) {
		if (legalP(board, i, player) == 1)
			return 1;
	}
	return 0;
}
//check who is the next player
int nextPlayer(char *board, int previousplayer) {
	int opp = opponent(previousplayer);
	if (anyLegalMove(opp, board) == 1)
		return opp;
	else if (anyLegalMove(previousplayer, board) == 1) {
		return previousplayer;
	}
	else {
		return 0;
	}
}
//find all legal moves by distributing work to slaves
void findAllLegalMove(char *board, int player, int *moves, int *moveCounts) {
	MPI_Status status;
	int resultsInfo[800];
	int requestNo = 1;
	int result = 0;
	int count = 0;
	int slave_id;

	int i;
	if (boardSize > slaves) {
		//distribute workload evenly when there is more workload than number of slaves
		for (slave_id = 0; slave_id < slaves; slave_id++) {
			// Start(j) = Start point for process i =floor(N j / P)
			int start = (int)(boardSize * slave_id / slaves);
			// Length(j) = Length of work for i = floor(N (j + 1) / P) – start(j)
			int size = (int)(boardSize * (slave_id + 1) / slaves) - start;
			int arraySend[size];
			for (i = 0; i < size; i++) {
				arraySend[i] = start + i;
			}
			MPI_Send(&requestNo, 1, MPI_INT, slave_id, COMPUTATION_TAG, MPI_COMM_WORLD);
			MPI_Send(&board[0], boardSize, MPI_CHAR, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(&player, 1, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(&size, 1, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(arraySend, size, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
		}
		//receive results back from slaves
		int index = 0;
		for (slave_id = 0; slave_id < slaves; slave_id++) {
			int start = (int)(boardSize * slave_id / slaves);
			int size = (int)(boardSize * (slave_id + 1) / slaves) - start;
			MPI_Recv(&resultsInfo, size, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD, &status);
			for (i = 0; i < size; i++) {
				if (resultsInfo[i]) {
					result = 1;
					moves[count] = index;
					count++;
				}
				index++;
			}
		}
	}
	else {//more slaves than workload, can just distribute 1 to each
		for (i = 0; i < boardSize; i++) {
			MPI_Send(&requestNo, 1, MPI_INT, i, COMPUTATION_TAG, MPI_COMM_WORLD);
			MPI_Send(&board[0], boardSize, MPI_CHAR, i, i, MPI_COMM_WORLD);
			MPI_Send(&player, 1, MPI_INT, i, i, MPI_COMM_WORLD);
			MPI_Send(&i, 1, MPI_INT, i, i, MPI_COMM_WORLD);
		}
		//receive results back from slaves
		int resultInfo;
		for (i = 0; i < boardSize; i++) {
			MPI_Recv(&resultInfo, 1, MPI_INT, i, i, MPI_COMM_WORLD, &status);
			if (resultInfo) {
				result = 1;
				moves[count] = i;
				count++;
			}
		}
	}
	*moveCounts = count;
	return;
}
//calculate differences between pieces
int diffEval(int player, char * board) {
	int oppCount = 0, playerCount = 0;
	int opp = opponent(player);
	int i;
	for (i = 0; i < boardSize; i++) {
		if (board[i] == player)
			playerCount++;
		else if (board[i] == opp)
			oppCount++;
	}
	return playerCount - oppCount;
}
//heuristic evaluation of board
double heuristicEval(int player, char* board) {
	int opp = opponent(player);
	//heuristic 1: differences between pieces
	double pieceHeuristic = 250 * diffEval(player, board);

	//heuristic 2: occupancy of corner
	double cornerHeuristic = 0;
	if (board[0] == player)
		cornerHeuristic++;
	else if (board[0] == opp)
		cornerHeuristic--;
	if (board[xSize - 1] == player)
		cornerHeuristic++;
	else if (board[xSize - 1] == opp)
		cornerHeuristic--;
	if (board[xSize*ySize - 1] == player)
		cornerHeuristic++;
	else if (board[xSize*ySize - 1] == opp)
		cornerHeuristic--;
	if (board[xSize*(ySize - 1)] == player)
		cornerHeuristic++;
	else if (board[xSize*(ySize - 1)] == opp)
		cornerHeuristic--;
	cornerHeuristic *= 801.724;

	//heuristic 3: mobility of player
	int legalMoves[676];
	int countLegalMoves = 0;
	findAllLegalMove(board, player, legalMoves, &countLegalMoves);
	double mobilityHeuristic = countLegalMoves * 39.46;
	return mobilityHeuristic + cornerHeuristic + pieceHeuristic;
}
/*START OF MINI MAX ALGORITHM*/
double getMinMax(int player, char *board, int depth, int minOrMax, double alpha, double beta) {
	current = clock();
	if (depth > maxDepth || boardVisited >= maxBoards || (double)(current - start) / CLOCKS_PER_SEC >(double)(timeOut)) {
		searchedEntire = 0;
		return heuristicEval(player, board);
	}
	if (depth > deepestDepthVisited)
		deepestDepthVisited = depth;
	boardVisited++;
	int next;
	double score;
	double bestScore;
	int opp = opponent(player);

	//get all moves
	int legalMoves[676];
	double scoreLegalMove[676];
	int countLegalMoves = 0;
	findAllLegalMove(board, color, legalMoves, &countLegalMoves);
	char newBoard[676];

	//min, opponent
	if (minOrMax == 0) {
		bestScore = DBL_MAX;
		findAllLegalMove(board, opp, legalMoves, &countLegalMoves);
	}
	else { //max, player
		bestScore = DBL_MIN;
		findAllLegalMove(board, color, legalMoves, &countLegalMoves);
	}
	if (countLegalMoves == 0) {//searched entire space
		return heuristicEval(player, board);
	}
	int i;
	for (i = 0; i < countLegalMoves; i++) {
		duplicateBoard(board, newBoard);
		if (minOrMax == 0) {
			makeMove(board, legalMoves[i], opp);
			next = nextPlayer(newBoard, opp);
		}
		else {//MAX
			makeMove(board, legalMoves[i], player);
			next = nextPlayer(newBoard, player);
		}
		//no player can move, gameover
		if (next == 0) {
			score = heuristicEval(player, board);
		}
		else if (next == player) {
			score = getMinMax(player, newBoard, depth + 1, 1, alpha, beta);
		}
		else if (next == opp) {
			score = getMinMax(player, newBoard, depth + 1, 0, alpha, beta);
		}
		//update
		if (minOrMax == 1) {//MAX
			if (score > bestScore)
				bestScore = score;
			if (score >= beta) {//pruning
				return bestScore;
			}
			if (score > alpha)
				alpha = score;
		}
		else if (minOrMax == 0) {//MIN
			if (score < bestScore)
				bestScore = score;
			if (score <= alpha) { //pruning
				return bestScore;
			}
			if (score < beta)
				beta = score;
		}
	}
	return bestScore;
}
//compute set of best moves
void getMinimaxMoves(char *board, int *bestMoves, int *moveCount) {
	int next;
	double score;
	double bestScore = DBL_MIN;
	int opp = opponent(color);
	double alpha = DBL_MIN;
	double beta = DBL_MAX;

	searchedEntire = 1; boardVisited = 1; deepestDepthVisited = 0; start = clock();
	//get all moves
	int legalMoves[676];
	double scoreLegalMove[676];
	int countLegalMoves = 0;
	findAllLegalMove(board, color, legalMoves, &countLegalMoves);
	char newBoard[676];
	int i;
	for (i = 0; i < countLegalMoves; i++) {
		duplicateBoard(board, newBoard);
		makeMove(board, legalMoves[i], color);

		next = nextPlayer(newBoard, color);

		//no player can move, gameover
		if (next == 0) {
			score = diffEval(color, newBoard);
			if (score > 0) score = DBL_MAX;
			if (score < 0) score = DBL_MIN;
		}
		else if (next == color)
			score = getMinMax(color, newBoard, 1, 1, alpha, beta);
		else if (next == opp)
			score = getMinMax(color, newBoard, 1, 0, alpha, beta);
		if (score > bestScore) {
			bestScore = score;

			if (score > alpha)
				alpha = score;
		}
		scoreLegalMove[i] = score;
	}
	int bestMoveCount = 0;
	int k;
	for (k = 0; k < countLegalMoves; k++) {
		if (scoreLegalMove[k] == bestScore) {
			bestMoves[bestMoveCount] = legalMoves[k];
			bestMoveCount++;
		}
	}
	*moveCount = bestMoveCount;
}
/*END OF MINIMAX ALGORITHM*/


int main(int argc, char **argv) {

	int nprocs;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	slaves = nprocs - 1;

	if (myid == MASTER_ID) {
		master(argv[1], argv[2]);
	}
	else {
		slave();
	}

	MPI_Finalize();
	return 0;
}
