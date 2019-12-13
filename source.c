#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <locale.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FRAMES_PER_STAGE (10 * 30)

#define KEY_ESC 27

#define PLAYER_CHAR '@'
#define OBSTACLE_CHAR '*'
#define WALL_CHAR '#'
#define EMPTY_CHAR ' '

#define EMPTY 1
#define WALL 2
#define OBSTACLE 3

#define RANK_FILE "ranking.dat"

// 사용자의 랭크를 저장하기
struct record {
	time_t time;
	int stage;
	char name[101];
};

WINDOW* window;

int game[40][40];

int stage = 1;
int obs_speed = 1;
int obs_count = 1;

int player_x = 20;
const int player_y = 38;

int main_menu(void);

void game_start(void);
void init_game(void);
int run_game(void);
void draw_game(void);

int show_esc(void);

void make_obs(void);
void move_obs(void);

void game_win(void);

int is_player_dead(void);
void player_move_left(void);
void player_move_right(void);

void show_ranking(void);
void save_ranking(void);
void sort_ranking(struct record records[100], int n);

int compare_record(struct record *record1, struct record *record2);

int main() {
	// rand 함수가 프로그램 실행 시마다 다른 값을 반환하도록 현재 시간을 바탕으로 섞음
	srand(time(0));

	// ncurses가 한글을 지원하도록 설정
	setlocale(LC_CTYPE, "ko_KR.utf-8");

	// ncurses 초기화
	window = initscr();

	// line buffering을 사용하지 않도록 설정
	raw();

	while (1) {
		int choice = main_menu();

		switch (choice) {
		case 1:
			init_game();
			game_start();
			break;

		case 2:
			show_ranking();
			break;

		case 3:
			return EXIT_SUCCESS;

		default:
			return EXIT_FAILURE;
		}
	}
}

int main_menu(void) {
	// 사용자가 입력한 문자열이 화면에 표시되지 않도록 설정
	noecho();

	// 사용자의 입력 커서가 보이지 않도록 설정
	curs_set(0);

	// 메뉴 출력 전 화면 초기화
	clear();

	// 메뉴 출력
	printw(
		"1. play game\n"
		"2. ranking\n"
		"3. exit\n"
	);

	// 출력한 메뉴를 화면에 표시
	refresh();

	// getch 사용시 사용자가 입력을 할 때 까지 기다림
	nodelay(window, FALSE);

	while (1) {
		int ch = getch();

		switch (ch) {
		case '1':
		case '2':
		case '3':
			return ch - '0';

		default:
			break;
		}
	}
}

void init_game(void) {
	stage = 1;
	obs_speed = 1;
	obs_count = 1;

	for (int x = 0; x < 40; x++) {
		game[0][x] = WALL;
		game[39][x] = WALL;
	}

	for (int y = 1; y < 40; y++) {
		game[y][0] = WALL;
		game[y][39] = WALL;
	}

	for (int y = 1; y < 39; y++) {
		for (int x = 1; x < 39; x++) {
			game[y][x] = EMPTY;
		}
	}
}

int run_game(void) {
	// 사용자가 입력한 문자열이 화면에 표시되지 않도록 설정
	noecho();

	// 사용자의 입력 커서가 보이지 않도록 설정
	curs_set(0);

	// 화살표 키를 입력받기 위한 설정
	keypad(stdscr, TRUE);

	// ESC키를 눌렀을 때 delay가 없도록 설정
	// (화살표 키와 같은 특수키가 ESC키와 동일한 '27'을 포함한 입력 값을 가지기 때문에, 특수키 입력시 ESC로 해석하는 오류를 방지하기 위해 기본 delay가 있음)
	//set_escdelay(0);

	int obstacle_move = 0;
	int obstacle_create = 0;

	int frame = 0;
	while (frame < FRAMES_PER_STAGE)
	{
		if (obstacle_move <= 0) {
			move_obs();
			obstacle_move = 12 - obs_speed * 2;
		}

		if (obstacle_create <= 0) {
			make_obs();
			obstacle_create = 12 - obs_count * 2;
		}

		draw_game();

		// getch 사용시 사용자의 입력을 기다리지 않고 넘김
		nodelay(window, TRUE);

		int ch = getch();
		switch (ch) {
		case KEY_LEFT:
			player_move_left();
			break;

		case KEY_RIGHT:
			player_move_right();
			break;

		case KEY_ESC: {
			switch (show_esc()) {
			case 2:
				// 직접 게임을 종료할 경우 프로세스 종료와 함께 상태코드 0을 반환.
				return 0;

			case 1:
			default:
				break;
			}
		}

		default:
			break;
		}

		if (is_player_dead()) {
			// 죽을 경우 프로세스 종료와 함께 상태코드 2를 반환.
			return 2;
		}

		obstacle_move--;
		obstacle_create--;

		frame++;
		flushinp();
		napms(100);
	}

	return 1;
}

void draw_game(void) {
	// 화면에 출력된 내용을 지웁니다. (ncurses에서 제공하는 함수)
	clear();

	for (int y = 0; y < 40; y++) {
		for (int x = 0; x < 40; x++) {
			unsigned int block;

			// 각 칸에 맞는 글자를 선택합니다.
			switch (game[y][x]) {
			case 2:
				block = WALL_CHAR;
				break;

			case 3:
				block = OBSTACLE_CHAR;
				break;

			case 1:
			default:
				block = EMPTY_CHAR;
				break;
			}

			// 선택된 글자를 출력합니다.
			addch(block);
		}
		addch('\n');
	}

	// 캐릭터 위치로 이동해, 캐릭터에 맞는 글자를 출력합니다.
	move(player_y, player_x);
	addch(PLAYER_CHAR);

	// 화면에 출력된 내용을 보여줍니다.
	refresh();
}

void game_start(void) {
	pid_t pid = fork();

	// fork 실패
	if (pid == -1) {
		fprintf(stderr, "Fork error\n");
		exit(EXIT_FAILURE);
	}
	// 자식 프로세스는 게임을 실행한다.
	else if (pid == 0) {
		int state = run_game();
		exit(state);
	}
	// 부모 프로세스는 자식 프로세스를 기다린다.
	else {
		int child_state;
		wait(&child_state);

		// child_state에는 256(0xFF)의 값이 곱해져 있기 때문에 0xFF로 나눔
		// child_state /= 0xFF;
		// 2의 제곱수를 곱하거나 나누는 것은 비트 연산으로 바꿀 수 있음
		child_state >>= 8;

		switch (child_state)
		{
			// 사용자 직접 종료
		case 0:
			return;

			// 게임 통과
		case 1:
			if (stage == 10) {
				game_win();
				save_ranking();
				return;
			}

			stage++;
			obs_speed++;
			obs_count++;
			game_start();
			break;

			// 게임 실패
		case 2:
			stage--;
			save_ranking();
			break;

		default:
			fprintf(stderr, "ERROR\n");
			exit(EXIT_FAILURE);
		}
	}
}

int show_esc(void) {
	clear();

	printw(
		"1. resume\n"
		"2. mainmenu\n"
	);

	refresh();

	nodelay(window, FALSE);

	while (1)
	{
		int ch = getch();
		switch (ch) {
		case '1':
		case '2':
			return ch - '0';

		default:
			break;
		}
	}
}

void make_obs(void) {
	int x = rand() % 38 + 1;

	game[1][x] = OBSTACLE;
}

void move_obs(void) {
	// 게임 칸 전체 반복
	// 맨 아래 줄부터 처리해야함
	// (장애물을 아래로 옮기기 때문에, 맨 위 줄부터 처리하면 다음 줄의 장애물을 처리 할때 또 아래 줄으로 옮기게 됨)
	for (int y = 40; y >= 0; y--) {
		for (int x = 0; x < 40; x++) {
			// 해당 칸이 장애물일 때만 처리
			if (game[y][x] != OBSTACLE) continue;

			// 해당 칸을 빈칸으로 만듦
			game[y][x] = EMPTY;

			// 아래 칸이 벽일 경우에는 장애물이 사라저야함
			if (game[y + 1][x] == WALL) continue;

			// 아래 칸을 장애물로 바꿈
			game[y + 1][x] = OBSTACLE;
		}
	}
}

// 플레이어의 위치에 장애물이 있는 경우 사망
int is_player_dead(void) {
	return game[player_y][player_x] == OBSTACLE;
}

void player_move_left(void) {
	// 이동하려고 하는 위치에 벽이 있을 경우 이동하지 않음
	if (game[player_y][player_x - 1] == WALL)
		return;

	player_x -= 1;
}

void player_move_right(void) {
	// 이동하려고 하는 위치에 벽이 있을 경우 이동하지 않음
	if (game[player_y][player_x + 1] == WALL)
		return;

	player_x += 1;
}

void game_win(void) {
	clear();

	printw("You Win!!!");

	refresh();

	napms(3000);
}

// 게임 기록을 정렬
void sort_ranking(struct record * records, int n) {
	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			if (compare_record(&records[i], &records[j]) != -1) continue;

			struct record tmp = records[i];
			records[i] = records[j];
			records[j] = tmp;
		}
	}
}

// 두 게임 기록을 비교해 record1이 더 낮은 기록이면 -1, record2가 더 낮은 기록이면 1을 반환 (같은 경우 0을 반환)
int compare_record(struct record *record1, struct record *record2) {
	if (record1->stage != record2->stage) {
		return record1->stage < record2->stage ? -1 : 1;
	}

	if (record1->time != record2->time) {
		return record1->time > record2->time ? -1 : 1;
	}

	return 0;
}

void show_ranking(void) {
	clear();

	int fd = open(RANK_FILE, O_RDONLY);

	struct record ranking[100];
	int count = 0;

	while (count < 100) {
		ssize_t read_n = read(fd, &ranking[count], sizeof(struct record));

		if (read_n == -1) break;
		if (read_n == 0) break;

		count++;
	}
	close(fd);

	sort_ranking(ranking, count);

	for (int i = 0; i < count; i++) {
		struct tm* time_info = localtime(&ranking[i].time);
		printw(
			"ranking %d\t%dyear %02dmonth %02dday %02dhour %02dminute %02dsecond\tstage %02d\t%s\n",
			i + 1,
			time_info->tm_year + 1900,
			time_info->tm_mon,
			time_info->tm_mday,
			time_info->tm_hour,
			time_info->tm_min,
			time_info->tm_sec,
			ranking[i].stage,
			ranking[i].name
		);
	}

	printw("\n<ESC>: return to mainmenu\n");

	refresh();

	nodelay(window, FALSE);
	keypad(window, TRUE);
	//set_escdelay(0);

	while (1) {
		int ch = getch();
		fprintf(stderr, "%d, %d\n", ch, KEY_ENTER);

		switch (ch) {
		case KEY_ESC:
			return;

		default:
			break;
		}
	}
}

void save_ranking(void) {
	struct record rank_info;

	clear();

	rank_info.time = time(NULL);
	struct tm* time_info = localtime(&rank_info.time);

	printw(
		"time: %dyear %dmonth %dday %dhour %dminute %dsecond\n",
		time_info->tm_year + 1900,
		time_info->tm_mon,
		time_info->tm_mday,
		time_info->tm_hour,
		time_info->tm_min,
		time_info->tm_sec
	);

	rank_info.stage = stage;
	printw("stage: %d\n", stage);
	refresh();

	// 사용자가 입력한 내용이 화면에 표시되도록 변경
	echo();

	// 사용자 이름 입력
	printw("user name: ");
	scanw("%s", rank_info.name);

	int fd = open(RANK_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
	write(fd, &rank_info, sizeof(struct record));
	close(fd);
}
