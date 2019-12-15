#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define MAX_LENGTH 300

sqlite3 *database;
char *errorMsg = 0;

int callback(void *notUsed, int argc, char **argv, char **azColName);
void show_options();
void sql_error(int rc);
int insert();
int update();
int delete();

int main() 
{
    int rc = sqlite3_open("./db/quizz", &database);
    
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }

    int quit = 0;
    while (quit == 0)
    {
        show_options();

        printf("Choose: ");
        int option;
        scanf("%d", &option);
        while (option < 1 || option > 4)
        {
            printf("Invalid option. Choose: ");
            scanf("%d", &option);
        }

        switch (option)
        {
            case 1:
            {
                char sql[MAX_LENGTH] = "SELECT * FROM questions;";
                int rc = sqlite3_exec(database, sql, callback, 0, &errorMsg);
                sql_error(rc);
                break;
            }
            case 2:
            {
                int rc = insert();
                sql_error(rc);
                break;
            }
            case 3:
            {
                int rc = update();
                sql_error(rc);
                break;
            }
            case 4:
            {
                rc = delete();
                sql_error(rc);
                break;
            }
        }

        printf("Do you want to perform another operation[y/n]? ");
        char choice;
        scanf(" %c", &choice);
        while (choice != 'y' && choice != 'Y' && choice != 'n' && choice != 'N')
        {
            printf("Invalid option. Choose[y/n]: ");
            scanf(" %c", &choice);
        }

        if (choice == 'N' || choice == 'n')
        {
            quit = 1;
            printf("Exit\n");
        }
    }
    
    sqlite3_close(database);
    return 0;
}


int callback(void *notUsed, int argc, char **argv, char **azColName) 
{
    notUsed = 0;
    
    for (int i = 0; i < argc; ++i) 
    {
        printf("%s : %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    
    printf("\n");
    return 0;
}

void show_options()
{
    printf("What operations do you want to perform on the database with question?\n");
    printf("1 - Show all existing questions\n");
    printf("2 - Add a new question\n");
    printf("3 - Update a question\n");
    printf("4 - Delete a question\n");
}

void sql_error(int rc)
{
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "SQL error: %s\n", errorMsg);
        sqlite3_free(errorMsg);
    }
}

int insert()
{
    char sql[MAX_LENGTH] = "INSERT INTO questions VALUES(";

    printf("Question id: ");
    int id;
    scanf("%d", &id);
    char str[MAX_LENGTH];
    sprintf(str, "%d", id);
    strcat(sql, str);
    strcat(sql, ", \'");

    printf("Question: ");
    char question[MAX_LENGTH];
    scanf("%s", question);
    strcat(sql, question);
    strcat(sql, "\', ");

    char ch = 'a';
    for (int i = 0; i  < 4; ++i) 
    {
        int len = strlen(sql);
        sql[len] = '\'';
        sql[len + 1] = 0;

        printf("Answer %c: ", (ch + i));
        char choice[MAX_LENGTH];
        scanf("%s", choice);
        strcat(sql, choice);
        strcat(sql, "\', ");
    }

    int len = strlen(sql);
    sql[len] = '\'';
    sql[len + 1] = 0;

    printf("Correct answer: ");
    scanf(" %c", &ch);
    while (strchr("abcd", ch) == NULL)
    {
        printf("Invalid answer. Correct answer: ");
        scanf(" %c", &ch);
    }

    len = strlen(sql);
    sql[len] = ch;
    sql[len + 1] = 0;

    strcat(sql, "\');");

    return sqlite3_exec(database, sql, 0, 0, &errorMsg);
}

int update()
{
    char sql[MAX_LENGTH] = "UPDATE questions SET ";

    char mat[6][MAX_LENGTH];
    strcpy(mat[0], "question");
    strcpy(mat[1], "answer a");
    strcpy(mat[2], "answer b");
    strcpy(mat[3], "answer c");
    strcpy(mat[4], "answer d");
    strcpy(mat[5], "correct answer");

    printf("ID of the question you want to update: ");
    int id;
    scanf("%d", &id);

    printf("What field do you want to update?\n");
    for (int i = 0; i < 6; ++i)
    {
        printf("%d - %s\n", (i + 1), mat[i]);
    }

    int choice;
    printf("Choose: ");
    scanf("%d", &choice);
    while (choice < 1 || choice > 6)
    {
        printf("Invalid option. Choose: ");
        scanf("%d", &choice);
    }

    printf("The update: ");
    if (choice == 6)
    {
        char ch;
        scanf(" %c", &ch);
        while (strchr("abcd", ch) == NULL)
        {
            printf("Invalid value. Choose a valid value: ");
            scanf(" %c", &ch);
        }
        strcat(sql, "correctAnswer = \'");
        int len = strlen(sql);
        sql[len] = ch;
        sql[len + 1] = '\'';
        sql[len + 2] = ' ';
        sql[len + 3] = 0;
    }
    else
    {
        char newValue[MAX_LENGTH];
        scanf("%s", newValue);

        switch (choice)
        {
            case 1:
                strcat(sql, "question = \'");
                break;
            case 2:
                strcat(sql, "choiceA = \'");
                break;
            case 3:
                strcat(sql, "choiceB = \'");
                break;
            case 4:
                strcat(sql, "choiceC = \'");
                break;
            case 5:
                strcat(sql, "choiceD = \'");
                break; 
        }

        strcat(sql, newValue);
        int len = strlen(sql);
        sql[len + 1] = '\'';
        sql[len + 2] = ' ';
        sql[len + 3] = 0;
    }

    strcat(sql, "WHERE idQ = ");
    char str[MAX_LENGTH];
    sprintf(str, "%d", id);
    strcat(sql, str);
    strcat(sql, ";");

    return sqlite3_exec(database, sql, 0, 0, &errorMsg);
}

int delete()
{
    char sql[MAX_LENGTH] = "DELETE FROM questions WHERE idQ = ";
    printf("ID of the question you want to delete: ");
    char id;
    scanf(" %c", &id);
    int len = strlen(sql);
    sql[len] = id;
    sql[len + 1] = 0;
    strcat(sql, ";");

    return sqlite3_exec(database, sql, 0, 0, &errorMsg);
}