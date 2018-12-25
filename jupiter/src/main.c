#if HAVE_CONFIG_H
# include <config.h>
#else
# error this shouldn't happen
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#else
# error this shouldn't happen
#endif

#if HAVE_FOOBAR_H
# error this shouldn't happen
# include <foobar.h>
#endif

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char * argv[])
{
    printf("Hello from %s!\n", argv[0]);
    return 0;
}
