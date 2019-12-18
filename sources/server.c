#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>

#define PORT 2091
#define MAX_LENGTH 500
#define MAX_QUESTIONS_PER_ROUND 3
#define TIME_UNTIL_GAME_STARTS 10
#define TIME_TO_ANSWER 10
#define PAUSE 2
#define WIN_POINTS 10
#define LOSE_POINTS -5

typedef struct threadData
{
    int idThread;
    int clientDescriptor;
}threadData;

typedef struct
{
    pthread_t *array;
    int used;
    int size;
}ArrayThreads;

struct Player
{
    char userName[MAX_LENGTH];
    int score;
};

typedef struct
{
    struct Player *array;
    int used;
    int size;
}ArrayPlayers;

struct Question
{
    char question[MAX_LENGTH];
    char choiceA[MAX_LENGTH];
    char choiceB[MAX_LENGTH];
    char choiceC[MAX_LENGTH];
    char choiceD[MAX_LENGTH];
    char correctAnswer;
};

typedef struct
{
    struct Question *array;
    int used;
    int size;
}ArrayQuestions;

extern int errno;
sqlite3 *database;
char *errorMsg = 0;

ArrayPlayers players;
int readyPlayers = 0;

ArrayQuestions questions;
int currentQuestion;
int currentRound;
int gameIsPrepared;

// manage time
time_t start;
int totalTime;

// declaration of thread condition variables
pthread_cond_t conditionReady = PTHREAD_COND_INITIALIZER;
pthread_cond_t conditionPreparedGame = PTHREAD_COND_INITIALIZER;
pthread_cond_t conditionStartRound = PTHREAD_COND_INITIALIZER;
pthread_cond_t conditionNextRound = PTHREAD_COND_INITIALIZER;
pthread_cond_t conditionNextGame = PTHREAD_COND_INITIALIZER;

void init_threads(ArrayThreads* arr);
void add_thread(ArrayThreads* arr);
void free_threads(ArrayThreads* arr);

void init_players(ArrayPlayers* arr);
void insert_players(ArrayPlayers* arr, struct Player newPlayer);
void free_players(ArrayPlayers* arr);
void remove_player(ArrayPlayers* arr, char* name);
void update_players(ArrayPlayers* arr);
void update_score(ArrayPlayers* arr, char* name, int points);
void reset_score(ArrayPlayers* arr);
int winner(ArrayPlayers* arr, int currentRound);

void init_questions(ArrayQuestions* arr);
void add_questions(ArrayQuestions* arr, struct Question newQuestion);
void free_questions(ArrayQuestions* arr);
void shuffle(int* array, int len);

static void * start_quizz();
static void * treat(void *);

int pregame(void* arg, struct Player* currentPlayer);
int game(void* arg, struct Player currentPlayer);

int read_socket_error(int readCode);
int write_socket_error(int writeCode);

int player_logged_in(ArrayPlayers* arr, char* name);
int user_in_database(char* user);
int account_in_database(char* user, unsigned long pass);
void add_account_in_database(char* user, unsigned long pass);

void add_questions_from_db(ArrayQuestions* questions);
int callback(void* notUsed, int argc, char** argv, char** azColName);

void build_question_to_send(char* result, int currentQuestion);
void build_leaderboard(char* resut, int currentRound);

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int rc = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
    if (rc != SQLITE_OK) 
    {
        perror("Cannot set serialized mode\n");
        return 1;
    }

    rc = sqlite3_open_v2("./db/quizz", &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }

    struct sockaddr_in server;
    struct sockaddr_in from;

    int socketDescriptor;
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("socket() error\n");
    }

    int flag = 1;
    setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    memset(&server, 0, sizeof (server));
    memset(&from, 0, sizeof (from));

    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(socketDescriptor, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1)
    {
        perror ("bind() error\n");
    }

    if (listen (socketDescriptor, 10) == -1)
    {
        perror ("listen() error\n");
    }

    // thread which manage the game
    pthread_t gameId;
    pthread_create(&gameId, NULL, &start_quizz, NULL);

    int length = sizeof(from);

    ArrayThreads threads;
    init_threads(&threads);
    while (1)
    {
        int client;
        threadData * td;   

        printf ("Waiting at %d port\n", PORT);
        fflush(stdout);

        if ((client = accept(socketDescriptor, (struct sockaddr *) &from, &length)) < 0)
        {
            perror ("accept() error\n");
            continue;
        }

        td = (struct threadData*)malloc(sizeof(struct threadData));
        td->idThread = threads.used;
        td->clientDescriptor = client;

        pthread_create(&threads.array[threads.used], NULL, &treat, td);
        add_thread(&threads);
    }

    close (socketDescriptor);
    free_threads(&threads);
    return 0;
}

void init_threads(ArrayThreads* arr)
{
    arr->array = (pthread_t *)malloc(MAX_LENGTH * sizeof(pthread_t));
    arr->used = 0;
    arr->size = MAX_LENGTH;
}

void add_thread(ArrayThreads* arr)
{
    if (arr->used == arr->size)
    {
        arr->size *= 2;
        arr->array = (pthread_t *)realloc(arr->array, arr->size * sizeof(pthread_t));
    }

    arr->array[arr->used++] = 0;
}

void free_threads(ArrayThreads* arr)
{
    free(arr->array);
    arr->array = NULL;
    arr->used = 0;
    arr->size = 0;
}

void init_players(ArrayPlayers* arr)
{
    arr->array = (struct Player *)malloc(MAX_LENGTH * sizeof(struct Player));
    arr->used = 0;
    arr->size = MAX_LENGTH;
}

void insert_players(ArrayPlayers* arr, struct Player newPlayer)
{
    if (arr->used == arr->size)
    {
        arr->size *= 2;
        arr->array = (struct Player *)realloc(arr->array, arr->size * sizeof(struct Player));
    }

    arr->array[arr->used++] = newPlayer;
}

void remove_player(ArrayPlayers* arr, char* name)
{
    int i = 0;
    while (strcmp(arr->array[i].userName, name) != 0 && i < arr->used)
    {
        ++i;
    }

    --arr->used;

    while (i < arr->used)
    {
        arr->array[i] = arr->array[++i];
    }
}

void update_score(ArrayPlayers* arr, char* name, int points)
{
    for (int i = 0; i < arr->used; ++i)
    {
        if (strcmp(arr->array[i].userName, name) == 0)
        {
            arr->array[i].score += points;
            return;
        }
    }
}

void update_players(ArrayPlayers* arr)
{
    int sorted;
    do
    {
        sorted = 1;
        for (int i = 0; i < arr->used - 1; ++i)
        {
            if (arr->array[i].score < arr->array[i + 1].score)
            {
                struct Player p = arr->array[i + 1];
                arr->array[i + 1] = arr->array[i];
                arr->array[i] = p;
                sorted = 0;
            }
        }
    }while (sorted == 0);
}

void reset_score(ArrayPlayers* arr)
{
    for (int i = 0; i < arr->used; ++i)
    {
        arr->array[i].score = 0;
    }
}

int winner(ArrayPlayers* arr, int currentRound)
{
    if (arr->used < 2)
    {
        return 0;
    }

    int roundsLeft = MAX_QUESTIONS_PER_ROUND - currentRound;
    int worstScore = arr->array[0].score + LOSE_POINTS * roundsLeft;
    int bestScore = arr->array[1].score + WIN_POINTS * roundsLeft;

    if (worstScore > bestScore)
    {
        return 1;
    }
    return 0;
}

void free_players(ArrayPlayers* arr)
{
    free(arr->array);
    arr->array = NULL;
    arr->used = 0;
    arr->size = 0;
}

void init_questions(ArrayQuestions* arr)
{
    arr->array = (struct Question *)malloc(MAX_LENGTH * sizeof(struct Question));
    arr->used = 0;
    arr->size = MAX_LENGTH;
}

void add_questions(ArrayQuestions* arr, struct Question newQuestion)
{
    if (arr->used == arr->size)
    {
        arr->size *= 2;
        arr->array = (struct Question *)realloc(arr->array, arr->size * sizeof(struct Question));
    }

    arr->array[arr->used++] = newQuestion;
}

void free_questions(ArrayQuestions* arr)
{
    free(arr->array);
    arr->array = NULL;
    arr->used = 0;
    arr->size = 0;
}

void swap(int *x, int *y)
{
    int aux = *x;
    *x = *y;
    *y = aux;
}

void shuffle(int* array, int len)
{
    srand(time(NULL));
    for (int i = len - 1; i > 0; --i)  
    {
        // Pick a random index from 0 to i 
        int j = rand() % (i + 1);  
  
        // Swap array[i] with the element  
        // at random index  
        swap(&array[i], &array[j]); 
    } 
}

static void * start_quizz()
{
    init_players(&players);
    init_questions(&questions);
    add_questions_from_db(&questions);

    while (1)
    {
        gameIsPrepared = 0;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&conditionReady, &lock);
        pthread_mutex_unlock(&lock);

        reset_score(&players);
        totalTime = TIME_UNTIL_GAME_STARTS;

        int orderOfQuestions[MAX_LENGTH];
        for (int i = 0; i < questions.used; ++i)
        {
            orderOfQuestions[i] = i;
        }
        shuffle(orderOfQuestions, questions.used);

        start = time(NULL);

        currentRound = 0;
        for (int i = 0; i < MAX_QUESTIONS_PER_ROUND; ++i)
        {
            totalTime += TIME_TO_ANSWER;
            currentQuestion = orderOfQuestions[i];

            if (i == 0)
            {
                gameIsPrepared = 1;
                pthread_cond_broadcast(&conditionPreparedGame);
            }

            pthread_cond_broadcast(&conditionStartRound);
            if (players.used != 0)
            {
                pthread_mutex_lock(&lock);
                pthread_cond_wait(&conditionNextRound, &lock); 
                pthread_mutex_unlock(&lock);
            }

            if (players.used == 0)
            {
                i = MAX_QUESTIONS_PER_ROUND;
            }
            
            ++currentRound;
            totalTime += PAUSE;
            update_players(&players);

            if (winner(&players, currentRound) == 1)
            {
                i = MAX_QUESTIONS_PER_ROUND;
            }
        }

        currentQuestion = -1;
        pthread_cond_broadcast(&conditionStartRound);
    }

    return NULL;
}

static void *treat(void* arg)
{
    struct threadData tdL;
    tdL = *((struct threadData*)arg);		 
    pthread_detach(pthread_self());
    struct Player currentPlayer;
    currentPlayer.score = 0;

    if (pregame((struct threadData*)arg, &currentPlayer) == 0)
    {
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&lock);
        insert_players(&players, currentPlayer);
        pthread_mutex_unlock(&lock);
 
        game((struct threadData*)arg, currentPlayer);
    }

    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);
    remove_player(&players, currentPlayer.userName);
    if (players.used == 0)
    {
        pthread_cond_signal(&conditionNextRound);
    }
    pthread_mutex_unlock(&lock);

    printf("[thread %d] User disconnected\n", tdL.idThread);
    close((intptr_t)arg);
    return(NULL);
}

int pregame(void* arg, struct Player *currentPlayer)
{
    struct threadData tdL;
    tdL = *((struct threadData*)arg);
    printf("[thread %d] Waiting client to login\n", tdL.idThread);

    while (1)
    {
        int option;
        int readCode = read(tdL.clientDescriptor, &option, sizeof(int));
        if (read_socket_error(readCode) != 0)
        {
            return -1;
        }

        switch (option)
        {
            case 1:
            {//login
                int valid = 0;
                char user[MAX_LENGTH];
                while (valid < 1)
                {
                    int lengthOfUser;
                    readCode = read(tdL.clientDescriptor, &lengthOfUser, sizeof(int));
                    if (read_socket_error(readCode) != 0)
                    {
                        return -1;
                    }

                    memset(&user, 0, sizeof(user));
                    readCode = read(tdL.clientDescriptor, &user, lengthOfUser);
                    if (read_socket_error(readCode) != 0)
                    {
                        return -1;
                    }

                    if (user_in_database(user) == 1)
                    {
                        valid = 1;
                    }

                    if (player_logged_in(&players, user) == 1)
                    {
                        valid = -1;
                    }

                    int writeCode = write(tdL.clientDescriptor, &valid, sizeof(int));
                    if (write_socket_error(writeCode) != 0)
                    {
                        return -1;
                    }
                }

                valid = 0;
                while (valid == 0)
                {
                    unsigned long hash;
                    readCode = read(tdL.clientDescriptor, &hash, sizeof(unsigned long));
                    if (read_socket_error(readCode) != 0)
                    {
                        return -1;
                    }

                    if (account_in_database(user, hash) == 1)
                    {
                        valid = 1;
                    }

                    int writeCode = write(tdL.clientDescriptor, &valid, sizeof(int));
                    if (write_socket_error(writeCode) != 0)
                    {
                        return -1;
                    }
                }

                strcpy(currentPlayer->userName, user);
                return 0;
            }
            case 2:
            {//register
                int valid = 0;
                char user[MAX_LENGTH];
                while (valid == 0)
                {
                    int lengthOfUser;
                    readCode = read(tdL.clientDescriptor, &lengthOfUser, sizeof(int));
                    if (read_socket_error(readCode) != 0)
                    {
                        return -1;
                    }

                    memset(&user, 0, sizeof(user));
                    readCode = read(tdL.clientDescriptor, &user, lengthOfUser);
                    if (read_socket_error(readCode) != 0)
                    {
                        return -1;
                    }

                    if (user_in_database(user) == 0)
                    {
                        valid = 1;
                    }

                    int writeCode = write(tdL.clientDescriptor, &valid, sizeof(int));
                    if (write_socket_error(writeCode) != 0)
                    {
                        return -1;
                    }
                }

                unsigned long hash;
                int readCode = read(tdL.clientDescriptor, &hash, sizeof(unsigned long));
                if (read_socket_error(readCode) != 0)
                {
                    return -1;
                }
                add_account_in_database(user, hash);
                break;
            }
            case 3:
            {//exit
                return 1;
            }
        }
    }
}

int game(void* arg, struct Player currentPlayer)
{
    struct threadData tdL;
    tdL = *((struct threadData*)arg);
    char result[MAX_LENGTH];

    while (1)
    {
        pthread_cond_signal(&conditionReady);

        if (gameIsPrepared == 0)
        {
            pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&lock);
            pthread_cond_wait(&conditionPreparedGame, &lock);
            pthread_mutex_unlock(&lock);
        }

        memset(&result, 0, sizeof(result));
        time_t now = time(NULL);

        if (difftime(now, start) < TIME_UNTIL_GAME_STARTS)
        {
            char str[MAX_LENGTH];
            int timeUntilStart = TIME_UNTIL_GAME_STARTS - (int)(difftime(now, start));
            sprintf(str, "%d", timeUntilStart);

            strcpy(result, "The next game will start in ");
            strcat(result, str);
            strcat(result, " seconds!\0");

            int lengthOfResult = strlen(result);
            int writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            do
            {
                now = time(NULL);
            }while (difftime(now, start) < TIME_UNTIL_GAME_STARTS);
        }
        else
        {
            strcpy(result, "The game already started!\0");

            int lengthOfResult = strlen(result);
            int writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }
        }

        int newQuestion = currentQuestion;

        while (currentQuestion != -1)
        {
            memset(&result, 0, sizeof(result));
            build_question_to_send(result, currentQuestion);

            int lengthOfResult = strlen(result);
            int writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            now = time(NULL);
            int timeToAnswer = TIME_TO_ANSWER - (((int)((difftime(now, start)) - (PAUSE * currentRound)) % 10));

            writeCode = write(tdL.clientDescriptor, &timeToAnswer, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            memset(&result, 0, sizeof(result));
            strcpy(result, "You have ");
            char str[MAX_LENGTH];
            sprintf(str, "%d", timeToAnswer);
            strcat(result, str);
            strcat(result, " seconds to send an answer!\0");

            lengthOfResult = strlen(result);
            writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            char received;
            int readCode = read(tdL.clientDescriptor, &received, sizeof(char));
            if (read_socket_error(readCode) != 0)
            {
                return -1;
            }

            char receivedAnswer;
            if (received == 'y')
            {
                readCode = read(tdL.clientDescriptor, &receivedAnswer, sizeof(char));
                if (read_socket_error(readCode) != 0)
                {
                    return -1;
                }
            }

            memset(&result, 0, sizeof(result));
            if (received == 'n')
            {
                strcpy(result, "\nLimit time exceed. The correct answer was: ");
                result[strlen(result)] = questions.array[currentQuestion].correctAnswer;
                result[strlen(result) + 1] = '\0';
                strcat(result, "\nYou lost 5 points. ");
                update_score(&players, currentPlayer.userName, LOSE_POINTS);
            }
            else
            if (receivedAnswer == questions.array[currentQuestion].correctAnswer ||
                receivedAnswer + 32 == questions.array[currentQuestion].correctAnswer)
            {
                strcpy(result, "Correct answer.\nYou got 10 points.");
                update_score(&players, currentPlayer.userName, WIN_POINTS);
            }
            else
            {
                strcpy(result, "Wrong answer. The correct answer was: ");
                result[strlen(result)] = questions.array[currentQuestion].correctAnswer;
                result[strlen(result) + 1] = 0;
                strcat(result, "\nYou lost 5 points. ");
                update_score(&players, currentPlayer.userName, LOSE_POINTS);
            }

            do
            {
                now = time(NULL);
            }while (difftime(now, start) < totalTime);

            lengthOfResult = strlen(result);
            writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

            pthread_mutex_lock(&lock);
            ++readyPlayers;
            if (readyPlayers != players.used)
            {
                pthread_cond_wait(&conditionNextRound, &lock); 
            }
            else
            {
                pthread_cond_broadcast(&conditionNextRound);
                readyPlayers = 0;
            }
            pthread_mutex_unlock(&lock);

            if (newQuestion == currentQuestion)
            {
                pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_lock(&lock);
                pthread_cond_wait(&conditionStartRound, &lock);
                pthread_mutex_unlock(&lock);
            }

            newQuestion = currentQuestion;

            writeCode = write(tdL.clientDescriptor, &currentQuestion, sizeof(int));
            if (write_socket_error(writeCode) != 0)
            {
                return -1;
            }

            sleep(PAUSE);
        }

        memset(&result, 0, sizeof(result));
        build_leaderboard(result, currentRound);

        int lengthOfResult = strlen(result);
        int writeCode = write(tdL.clientDescriptor, &lengthOfResult, sizeof(int));
        if (write_socket_error(writeCode) != 0)
        {
            return -1;
        }

        writeCode = write(tdL.clientDescriptor, &result, lengthOfResult);
        if (write_socket_error(writeCode) != 0)
        {
            return -1;
        }

        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&lock);
        ++readyPlayers;
        if (readyPlayers != players.used)
        {
            pthread_cond_wait(&conditionNextGame, &lock);
        }
        else
        {
            pthread_cond_broadcast(&conditionNextGame);
            readyPlayers = 0;
        }
        pthread_mutex_unlock(&lock);
    }
}

int read_socket_error(int readCode)
{
    if (readCode <= 0)
    {
        printf("[thread] read() error\n");
        return -1;
    }
    return 0;
}

int write_socket_error(int writeCode)
{
    if (writeCode <= 0)
    {
        printf("[thread] write() error\n");
        return -1;
    }
    return 0;
}

int player_logged_in(ArrayPlayers* arr, char* name)
{
    for (int i = 0; i < arr->used; ++i)
    {
        if (strcmp(arr->array[i].userName, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int user_in_database(char* user)
{
    char sql[MAX_LENGTH] = "SELECT userName from users WHERE userName = '";
    strcat(sql, user);
    strcat(sql,"\';\0");
    sqlite3_stmt *res;

    int rc = sqlite3_prepare_v2(database, sql, -1, &res, 0);
    int step = sqlite3_step(res);

    return (step == SQLITE_ROW);
}

int account_in_database(char* user, unsigned long pass)
{
    char sql[MAX_LENGTH] = "SELECT userName from users WHERE userName = '";
    strcat(sql, user);
    strcat(sql, "\' AND password = ");
    char str[MAX_LENGTH];
    sprintf(str, "%lu", pass);
    strcat(sql, str);
    strcat(sql,";\0");

    sqlite3_stmt *res;

    int rc = sqlite3_prepare_v2(database, sql, -1, &res, 0);
    int step = sqlite3_step(res);

    return (step == SQLITE_ROW);
}

void add_account_in_database(char* user, unsigned long pass)
{
    char sql[MAX_LENGTH] = "INSERT INTO users VALUES(";
    strcat(sql,"\'");
    strcat(sql, user);
    strcat(sql,"\', ");

    char str[MAX_LENGTH];
    sprintf(str, "%lu", pass);
    strcat(sql, str);
    strcat(sql, ");");

    sqlite3_exec(database, sql, 0, 0, &errorMsg);
}

void add_questions_from_db(ArrayQuestions* questions)
{
    char sql[MAX_LENGTH] = "SELECT * FROM questions;";
    int rc = sqlite3_exec(database, sql, callback, 0, &errorMsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errorMsg);
        sqlite3_free(errorMsg);
        sqlite3_close(database);
    }
}

int callback(void* notUsed, int argc, char** argv, char** azColName) 
{
    struct Question q;
    strcpy(q.question, argv[1]);
    strcpy(q.choiceA, argv[2]);
    strcpy(q.choiceB, argv[3]);
    strcpy(q.choiceC, argv[4]);
    strcpy(q.choiceD, argv[5]);
    q.correctAnswer = argv[6][0];

    add_questions(&questions, q);
    
    return 0;
}

void build_question_to_send(char* result, int currentQuestion)
{
    strcpy(result, questions.array[currentQuestion].question);
    strcat(result, "\na.");
    strcat(result, questions.array[currentQuestion].choiceA);
    strcat(result, "\nb.");
    strcat(result, questions.array[currentQuestion].choiceB);
    strcat(result, "\nc.");
    strcat(result, questions.array[currentQuestion].choiceC);
    strcat(result, "\nd.");
    strcat(result, questions.array[currentQuestion].choiceD);
}

void build_leaderboard(char* result, int currentRound)
{
    if (currentRound != MAX_QUESTIONS_PER_ROUND)
    {
        strcpy(result, "The game has finished (anticipated victory)");
    }
    else
    {
        strcpy(result, "The game has finished (the rounds are over)");
    }

    strcat(result, "\nFinal leaderboard:\n");

    for (int i = 0; i < players.used; ++i)
    {
        char str[MAX_LENGTH];
        sprintf(str, "%d. ", i + 1);
        strcat(result, str);
        strcat(result, players.array[i].userName);
        strcat(result, "   ");
        memset(&str, 0, sizeof(str));
        if (players.array[i].score >= 0)
        {
            sprintf(str, "%d", players.array[i].score);
        }
        else
        {
            sprintf(str, "%+d", players.array[i].score);
        }
        strcat(result, str);
        strcat(result, "\n");
    }
    strcat(result, "\0");
}