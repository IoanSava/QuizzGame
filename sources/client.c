#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>

#define PORT 2091
#define MAX_LENGTH 500
#define HASH_VALUE 5381
extern int errno;

void read_socket_error(int readCode);
void write_socket_error(int writeCode);

void initial_options();
void show_user_requirements();
int valid_user(char* user);

void show_password_requirements();
unsigned long hashing(char * password);
int valid_password(char* password);

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Syntax: %s <server_address>\n", argv[0]);
        return -1;
    }

    int socketDescriptor;
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() error\n");
        return errno;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(PORT);

    if (connect(socketDescriptor, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1)
    {
        perror("connect() error\n");
        return errno;
    }

    int loggedIn = 0;
    while (loggedIn == 0)
    {
        initial_options();
        printf("Choose: ");
        int option;
        scanf("%d", &option);
        while (option < 1 || option > 3)
        {
            printf("Invalid option. Choose: ");
            scanf("%d", &option);
        }

        int writeCode = write(socketDescriptor, &option, sizeof(int));
        write_socket_error(writeCode);

        switch (option)
        {
            case 1:
            {
                int valid = 0;
                char user[MAX_LENGTH];
                while (valid < 1)
                {
                    printf("User: ");
                    scanf("%s", user);

                    int lengthOfUser = strlen(user);
                    writeCode = write(socketDescriptor, &lengthOfUser, sizeof(int));
                    write_socket_error(writeCode);

                    writeCode = write(socketDescriptor, &user, lengthOfUser);
                    write_socket_error(writeCode);

                    int readCode = read(socketDescriptor, &valid, sizeof(int));
                    read_socket_error(readCode);

                    switch (valid)
                    {
                        case -1:
                            printf("Player already logged in\n");
                            break;
                        case 0:
                            printf("Invalid user\n");
                            break;
                    }
                }

                valid = 0;
                while (valid == 0)
                {
                    char password[MAX_LENGTH];
                    printf("Password: ");

                    // disable echo
                    system("stty -echo");

                    scanf("%s", password);

                    // enable echo
                    system("stty echo");
                    printf("\n");

                    unsigned long hash = hashing(password);
                    writeCode = write(socketDescriptor, &hash, sizeof(unsigned long));
                    write_socket_error(writeCode);

                    int readCode = read(socketDescriptor, &valid, sizeof(int));
                    read_socket_error(readCode);

                    if (valid == 0)
                    {
                        printf("Invalid password\n");
                    }
                }

                loggedIn = 1;
                printf("Welcome, %s!!\n", user);
                break;
            }

            case 2:
            {
                int valid = 0;
                while (valid == 0)
                {
                    char user[MAX_LENGTH];
                    int validUser = 0;
                    while (validUser == 0)
                    {
                        printf("User: ");
                        scanf("%s", user);

                        validUser = valid_user(user);
                        if (validUser == 0)
                        {
                            printf("Invalid user\n");
                            show_user_requirements();
                        }
                    }

                    int lengthOfUser = strlen(user);
                    writeCode = write(socketDescriptor, &lengthOfUser, sizeof(int));
                    write_socket_error(writeCode);

                    writeCode = write(socketDescriptor, &user, lengthOfUser);
                    write_socket_error(writeCode);

                    int readCode = read(socketDescriptor, &valid, sizeof(int));
                    read_socket_error(readCode);

                    if (valid == 0)
                    {
                        printf("Invalid user. Already registered\n");
                    }
                }

                valid = 0;
                char password[MAX_LENGTH];
                while (valid == 0)
                {
                    printf("Password: ");

                    // disable echo
                    system("stty -echo");

                    scanf("%s", password);

                    // enable echo
                    system("stty echo");
                    printf("\n");

                    valid = valid_password(password);
                    if (valid == 0)
                    {
                        printf("Invalid password\n");
                        show_password_requirements();
                    }
                }

                unsigned long hash = hashing(password);
                writeCode = write(socketDescriptor, &hash, sizeof(unsigned long));
                write_socket_error(writeCode);

                printf("The account has been succesfully created!\n");
                break;
            }

            case 3:
            {
                printf("Exit\n");
                return 0;
            }
        }
    }

    char result[MAX_LENGTH];
    while (1)
    {
        int lengthOfResult;
        int readCode = read(socketDescriptor, &lengthOfResult, sizeof(int));
        read_socket_error(readCode);

        memset(&result, 0, sizeof(result));
        readCode = read(socketDescriptor, &result, lengthOfResult);
        read_socket_error(readCode);
        result[lengthOfResult] = 0;

        printf("\n%s\n", result);

        int finishedGame = 0;
        while (finishedGame != -1)
        {   
            int lengthOfQuestion;
            readCode = read(socketDescriptor, &lengthOfQuestion, sizeof(int));
            read_socket_error(readCode);

            char question[MAX_LENGTH];
            readCode = read(socketDescriptor, &question, lengthOfQuestion);
            read_socket_error(readCode);
            question[lengthOfQuestion] = 0;
            printf("\n%s\n", question);

            int timeToAnswer;
            readCode = read(socketDescriptor, &timeToAnswer, sizeof(int));
            read_socket_error(readCode);

            readCode = read(socketDescriptor, &lengthOfResult, sizeof(int));
            read_socket_error(readCode);

            memset(&result, 0, sizeof(result));
            readCode = read(socketDescriptor, &result, lengthOfResult);
            read_socket_error(readCode);
            result[lengthOfResult] = 0;
            printf("\n%s\n", result);

            char sent = 'n';
            char answer;
            printf("Enter your answer: ");
            fflush(stdout);

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(0, &readfds);

            struct timeval tmo;
            tmo.tv_sec = timeToAnswer;
            tmo.tv_usec = 0;

            int readyForReading = select(1, &readfds, NULL, NULL, &tmo);

            if (readyForReading == -1)
            {
                printf("select() error");
            }

            if (readyForReading)
            {
                sent = 'y';
                readCode = read(0, &answer, sizeof(char));
                char newLine;
                readCode = read(0, &newLine, sizeof(char));
                printf("\nThe round will end soon\n");
            }

            int writeCode = write(socketDescriptor, &sent, sizeof(char));
            write_socket_error(writeCode);

            if (sent == 'y')
            {
                int writeCode = write(socketDescriptor, &answer, sizeof(char));
                write_socket_error(writeCode);
            }

            readCode = read(socketDescriptor, &lengthOfResult, sizeof(int));
            read_socket_error(readCode);

            memset(&result, 0, sizeof(result));
            readCode = read(socketDescriptor, &result, lengthOfResult);
            read_socket_error(readCode);
            result[lengthOfResult] = 0;
            printf("\n%s\n", result);

            readCode = read(socketDescriptor, &finishedGame, sizeof(int));
            read_socket_error(readCode);

            if (finishedGame != -1)
            {
                printf("\nThe next round will start soon\n");
            }
        }

        readCode = read(socketDescriptor, &lengthOfResult, sizeof(int));
        read_socket_error(readCode);

        memset(&result, 0, sizeof(result));
        readCode = read(socketDescriptor, &result, lengthOfResult);
        read_socket_error(readCode);
        result[lengthOfResult] = 0;

        printf("\n%s", result);
    }

    close (socketDescriptor);
    return 0;
}

void read_socket_error(int readCode)
{
    if (readCode == -1)
    {
        printf("read() error\n");
    }
}

void write_socket_error(int writeCode)
{
    if (writeCode == -1)
    {
        printf("write() error\n");
    }
}

void initial_options()
{
    printf("Options:\n");
    printf("1 - Login\n");
    printf("2 - Register\n");
    printf("3 - Exit\n");
}

void show_user_requirements()
{
    printf("User requierements:\n");
    printf("Minimum length is 3 characters, maximum is 32\n");
    printf("Can only contain letters, periods (.), and underscores (_)\n");
}

void show_password_requirements()
{
    printf("Password requierements:\n");
    printf("Be a minimum of 5 characters in length\n");
    printf("Be memorized; if a password is written down it must be secure\n");
    printf("Be private\n");
    printf("Contains at least 1 uppercase letter (A-Z) and 1 lowercase letter (a-z)\n");
}

int valid_user(char* user)
{
    int len = strlen(user);
    if (len < 3 || len > 32)
    {
        return 0;
    }

    for (int i = 0; i < len; ++i)
    {
        if (!(user[i] == '.' || user[i] == '_' || (user[i] >= 'a' && user[i] <= 'z')
            || (user[i] >= 'A' && user[i] <= 'Z') || (user[i] >= '0' && user[i] <= '9')))
        {
            return 0;
        }
    }

    return 1;
}

unsigned long hashing(char* password)
{
    unsigned long hash = HASH_VALUE;
    int c;

    while (c = *password++)
    {
        hash = ((hash << 3) + hash) + c;
    }

    return hash;
}

int valid_password(char* password)
{
    int len = strlen(password);
    if (len < 5)
    {
        return 0;
    }

    int lowerCaseFlag = 0;
    int upperCaseFlag = 0;

    for (int i = 0; i < len; ++i)
    {
        if (password[i] >= 'a' && password[i] <= 'z')
        {
            lowerCaseFlag = 1;
        }

        if (password[i] >= 'A' && password[i] <= 'Z')
        {
            upperCaseFlag = 1;
        }
    }

    if ((lowerCaseFlag + upperCaseFlag) != 2)
    {
        return 0;
    }
    return 1;
}