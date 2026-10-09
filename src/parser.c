#include "codexion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

t_validatedInt	validate_numstr(char *s)
{
	char			*c;
	t_validatedInt	out;

	c = s;
	memset(&out, 0, sizeof(out));
	if (!*c)
		return (out.error = 1, out);
	while (*c)
	{
		if (*c < 48 || *c > 57)
			return (out.error = 1, out);
		c++;
	}
	out.val = atoi(s);
	if (out.val < 0)
		out.error = 1;
	return (out);
}

void	print_args(t_cliArgs args)
{
	printf("Number of coders: %d\n", args.values[NUM_CODERS]);
	printf("Time to burnout: %d\n", args.values[TIME_TO_BURNOUT]);
	printf("Time to compile: %d\n", args.values[TIME_TO_COMPILE]);
	printf("Time to debug: %d\n", args.values[TIME_TO_DEBUG]);
	printf("Time to refactor: %d\n", args.values[TIME_TO_REFACTOR]);
	printf("Number of compiles required: %d\n", args.values[COMPILES_REQUIRED]);
	printf("Dongle cooldown: %d\n", args.values[DONGLE_COOLDOWN]);
	printf("Scheduler: %s\n", args.scheduler);
}

t_cliArgsValidation	validate_cli_args(int argc, char *argv[])
{
	t_cliArgsValidation	out;
	int					i;

	memset(&out, 0, sizeof(out));
	if (argc != 9)
		return (out.error = 1, out);
	if (strcmp("fifo", argv[8]) && strcmp("edf", argv[8]))
		return (out.error = 2, out);
	i = 0;
	while (++i < ARG_COUNT + 1)
	{
		if (i == 1 && validate_numstr(argv[i]).val == 0)
			return (out.error = 4, out);
		if (i == 7 && validate_numstr(argv[i]).val < 0)
			return (out.error = 6, out);
		if (validate_numstr(argv[i]).error)
			return (out.error = 3, out);
		out.cli_args.values[i - 1] = validate_numstr(argv[i]).val;
	}
	out.cli_args.scheduler = argv[8];
	return (out);
}

void	display_args(t_cliArgsValidation args)
{
	if (!args.error)
		print_args(args.cli_args);
	if (args.error == 1)
		printf("Error validating cli args: incorrect number of args\n");
	else if (args.error == 2)
		printf("Error validating cli args: unsupported scheduler\n");
	else if (args.error == 3)
		printf("Error validating cli args: non-numerical/negative-valued arg "
			"provided where positive numeric vals were expected\n");
	else if (args.error == 4)
		printf("Error validating cli args: at least one coder is required"
			" to run the simulation\n");
	else if (args.error == 6)
		printf("Error validating cli args: DONGLE_COOLDOWN is mandatory (cannot be 0)\n");
}
