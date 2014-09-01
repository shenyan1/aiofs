
find . -name "*.[ch]" | xargs cat | wc -l
ctags *
indent -i4 -npsl -bl *.c
indent -i4 -npsl -bl *.h
 
