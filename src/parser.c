#include "codexion.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

t_validatedInt	validate_numstr(char *s)
{
	char			*c;
	t_validatedInt	out;
	long			cand;

	c = s;
	memset(&out, 0, sizeof(out));
	if (!*c)
		return (out.error = 1, out);
	cand = 0;
	while (*c)
	{
		if (*c < 48 || *c > 57)
			return (out.error = 1, out);
		cand = cand * 10 + (*c - '0');
		if (cand > INT_MAX)
			return (out.error = 7, out);
		c++;
	}
	out.val = (int)cand;
	return (out);
}

t_cliArgsValidation	validate_cli_args(int argc, char *argv[])
{
	t_cliArgsValidation	out;
	t_validatedInt		num;
	int					i;

	memset(&out, 0, sizeof(out));
	if (argc != 9)
		return (out.error = 1, out);
	if (strcmp("fifo", argv[8]) && strcmp("edf", argv[8]))
		return (out.error = 2, out);
	i = 0;
	while (++i < ARG_COUNT + 1)
	{
		num = validate_numstr(argv[i]);
		if (num.error == 7)
			return (out.error = 7, out);
		if (num.error)
			return (out.error = 3, out);
		if (i == 1 && num.val == 0)
			return (out.error = 4, out);
		out.cli_args.values[i - 1] = num.val;
	}
	out.cli_args.scheduler = argv[8];
	return (out);
}

void	display_args(t_cliArgsValidation args)
{
	char	*msg;

	msg = "incorrect number of args";
	if (args.error == 2)
		msg = "unsupported scheduler";
	else if (args.error == 3)
		msg = "non-numerical/negative-valued arg provided where positive "
			"numeric vals were expected";
	else if (args.error == 4)
		msg = "at least one coder is required to run the simulation";
	else if (args.error == 7)
		msg = "numeric value should be valid integer";
	fprintf(stderr, "Error validating cli args: %s\n", msg);
}
