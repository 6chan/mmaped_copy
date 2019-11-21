# mmaped_copy
(EXPERIMENTAL) MMap memory file copy

This code creates a file with N bytes in very little time, as its purpouse initially was for testing other programs,
for example, if you have a program that handles files, and you want to test you program's behavior with a file of N bytes,
you could quickly call this program, and it would create you a file with exactly that size (and data in it ofcourse, currently
it uses a pattern of '101010' etc).

The code can be applyed to anny program that isnt using memory mapping with the intet of copying it, mapped memory makes it so much faster
than IO operations directly to the disk.

KEEP IN MIND that the speed of wich this code creates your file depends (alot) of what operative system you're using,
how your kernel intereacts with calls, how much RAM you have in total and available at the runtime, your hardware (type of RAM
and disk), and the weather outside.

Goes without saying, annyone is free to contribute to this code, message me and send the changes and ill take a look at them, test and
if they do better ill update the repository.

Thanks for reading.
