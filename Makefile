CC := gcc
CFLAGS := -Wall -g

main_target := oss
sub_target := user
targets := $(main_target) $(sub_target)

main_dependencies := oss.c $(sub_target)
sub_dependencies := user.c

$(main_target): $(main_dependencies)
	$(CC) $(CFLAGS) $< -o $@

$(sub_target): $(sub_dependencies)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(targets)
