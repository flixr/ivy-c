load ivytcl.so.3.0
Ivy::init TESTTCL "TESTTCL Ready" echo echo
Ivy::start 143.196.53.255:2011
Ivy::bind "(.*)" echo
Ivy::applist
Ivy::send test
Ivy::applist
mainloop

