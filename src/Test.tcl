load libtclivy.so.3.5
proc conCB { app str2 } {
 puts "TCL:Application:$app $str2"
}
proc discCB { app str2 } {
 puts "TCL:Application:$app $str2"
}
proc msgCB {str} {
 puts "TCL:Message:$str"
}
Ivy::init TESTTCL "TESTTCL Ready" conCB discCB 
Ivy::start ""
Ivy::bind "(.*)" msgCB 
Ivy::applist
Ivy::send test
Ivy::applist
vwait tcl_interactive
