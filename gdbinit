# Workaround to avoid a gdb error when debugging past `dlopen("libatiadlxx.so")`
# See https://stackoverflow.com/questions/2702628/
set env LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0
