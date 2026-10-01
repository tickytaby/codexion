NAME = codexion

CC = cc
CFLAGS = -Wall -Wextra -Werror -pthread
CPPFLAGS = -I includes -MMD -MP

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/parser.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

debug:
	$(MAKE) fclean
	$(MAKE) CFLAGS="$(CFLAGS) -g -fsanitize=address,undefined"

tsan:
	$(MAKE) fclean
	$(MAKE) CFLAGS="$(CFLAGS) -g -fsanitize=thread"

-include $(DEPS)

.PHONY: all clean fclean re debug tsan
