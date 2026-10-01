#ifndef PARSER_H 
# define PARSER_H

typedef enum e_arg
{
	NUM_CODERS,
	TIME_TO_BURNOUT,
	TIME_TO_COMPILE,
	TIME_TO_DEBUG,
	TIME_TO_REFACTOR,
	COMPILES_REQUIRED,
	DONGLE_COOLDOWN,
	ARG_COUNT,
}	t_arg;

typedef struct CliArgs
{
	int		values[ARG_COUNT];
	char	*scheduler;
}	t_cliArgs;

typedef struct cliArgsValidation
{
	int			error;
	t_cliArgs	cli_args;
}	t_cliArgsValidation;

typedef struct ValidateInt
{
	int	error;
	int	val;
}	t_validatedInt;

t_validatedInt		validate_numstr(char *s);
void				print_args(t_cliArgs args);
t_cliArgsValidation	validate_cli_args(int argc, char *argv[]);
void				display_args(t_cliArgsValidation args);

#endif
