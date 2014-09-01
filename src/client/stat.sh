
find . -name "*.[ch]" | xargs cat | wc -l
ctags *
indent -i4 *.c
indent -i4 *.h
