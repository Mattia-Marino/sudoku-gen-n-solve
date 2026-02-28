
#include "solver.c"
#include <stdio.h>

int _main(int argc, char *argv[])
{
	FILE *in;

	int c;
	int ret;

	if (argc > 2) {
		fprintf(stderr, "ERROR: too many arguments\n");
		return 1;
	}

	if (argc == 2) {
		in = fopen(argv[1], "r");
		if (in == NULL) {
			fprintf(stderr, "ERROR: could not open \"%s\"\n", argv[1]);
			return 2;
		}
	} else {
		in = stdin;
	}

	while (! feof(in)) {
		c = fgetc(in);
        printf("%c",c);

	}
    fclose(in);
    return 0;
    
}

int main(int argc, char *argv[])
{
	FILE *in;
	struct board b;

	int ret;

	if (argc > 2) {
		fprintf(stderr, "ERROR: too many arguments\n");
		return 1;
	}

	if (argc == 2) {
		in = fopen(argv[1], "r");
		if (in == NULL) {
			fprintf(stderr, "ERROR: could not open \"%s\"\n", argv[1]);
			return 2;
		}
	} else {
		in = stdin;
	}

    fseek(in,0,SEEK_END);
    long int end_pos = ftell(in);
    //printf("%ld\n",end_pos);
    fseek(in,0,SEEK_SET);

    while(!feof(in) && end_pos - 1 > ftell(in) )
    {
        /* Initialize data structures. */
        init_board(&b);

        /* Read and solve board. */
        read_board(in, &b);
        ret = solve_board(&b, MIN_NUM, MIN_NUM);

        printf("\n________________________________\n");
        /* Close input and return. */

        //printf("%d\n",feof(in));
        //printf("%ld\n",ftell(in));

        if (! ret)
            fprintf(stderr, "ERROR: board could not be solved\n");
    }
    fclose(in);
	return (ret?0:3);
}
