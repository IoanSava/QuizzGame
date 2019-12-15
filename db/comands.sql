CREATE TABLE questions(
	idQ number PRIMARY KEY,
	question varchar2(100),
	choiceA varchar2(50),
	choiceB varchar2(50),
	choiceC varchar2(50),
	choiceD varchar2(50),
	correctAnswer char(1)
);

INSERT INTO questions VALUES
(1, 'Potato was introduced to Europe by', 'Dutch', 'Portuguese', 'Germans', 'Spanish', 'd');

INSERT INTO questions VALUES
(2, 'Cotton fibers are made of', 'cellulose', 'starch', 'proteins', 'fats', 'a');

INSERT INTO questions VALUES
(3, 'Which metal is the best conductor of electric current?', 'copper', 'silver', 'iron', 'aluminium', 'b');

INSERT INTO questions VALUES
(4, 'The world largest desert is', 'Thar', 'Kalahari', 'Sahara', 'Sonoran', 'c');

INSERT INTO questions VALUES
(5, 'Country that was called as Land of Rising Sun:', 'Russia', 'Japan', 'Korea', 'Holland', 'b');

CREATE TABLE users(
	userName varchar2(32) NOT NULL,
	password number NOT NULL
);