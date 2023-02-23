If you get a build failure like:

'AssertionError: attempt to lock the cache while a parent process is holding the lock'

build the individual ports separately using:

embuilder build sdl2-mt
embuilder build sdl2_net
embuilder build sdl2_ttf
